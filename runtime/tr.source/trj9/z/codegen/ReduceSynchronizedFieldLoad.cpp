/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/ReduceSynchronizedFieldLoad.hpp"

#include <stddef.h>                    // for NULL
#include <stdint.h>                    // for int64_t
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/VMJ9.h"
#include "il/ILOps.hpp"                // for ILOpCode
#include "il/ILOpCodes.hpp"            // for ILOpCodes::pdSetSign, etc
#include "il/Node_inlines.hpp"         // for Node::getType, etc
#include "il/TreeTop.hpp"              // for TreeTop
#include "il/TreeTop_inlines.hpp"      // for TreeTop::getNode, etc
#include "infra/Assert.hpp"            // for TR_ASSERT
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Register.hpp"
#include "trj9/z/codegen/J9S390CHelperLinkage.hpp"

#define OPT_DETAILS "O^O REDUCE SYNCHRONIZED FIELD LOAD: "
void
ReduceSynchronizedFieldLoad::inlineSynchronizedFieldLoad(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* synchronizedObjectNode = node->getChild(0);

   // Materialize the object register first because LPD can only deal with base-displacement type memory references and
   // because the object appears directly underneath the indirect load we are guaranteed not to have an index register
   TR::Register* objectRegister = cg->evaluate(synchronizedObjectNode);

   TR::Node* loadNode = node->getChild(1);

   // The logic for evaluating a particular load is non-trivial in both evaluation sequence and setting of the various
   // register flags (collected references, etc.). As such we evaluate the load preemptively and extract the
   // materialized memory reference directly from the load itself for use in LPD.
   TR::Register* loadRegister = cg->evaluate(loadNode);
   TR::Register* lockRegister = cg->allocateRegister();

   TR::RegisterPair* registerPair = cg->allocateConsecutiveRegisterPair(lockRegister, loadRegister);

   TR::RegisterDependencyConditions* conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

   conditions->addPostCondition(registerPair, TR::RealRegister::EvenOddPair);
   conditions->addPostCondition(loadRegister, TR::RealRegister::LegalEvenOfPair);
   conditions->addPostCondition(lockRegister, TR::RealRegister::LegalOddOfPair);

   // Search for the load memory reference from the previously evaluated load
   TR::Instruction* loadInstruction = cg->comp()->getAppendInstruction();

   TR_ASSERT_SAFE_FATAL(loadInstruction->isLoad() && (loadInstruction->getKind() == OMR::Instruction::Kind::IsRX || loadInstruction->getKind() == OMR::Instruction::Kind::IsRXY), "Expecting the append instruction to be a load of kind RX or RXY\n");

   TR::MemoryReference* loadMemoryReference = static_cast<TR::S390RXInstruction*>(loadInstruction)->getMemoryReference();

   TR_ASSERT_SAFE_FATAL(loadMemoryReference->getIndexRegister() == NULL, "Load memory reference must not have an index register\n");

   // Recreate the memory reference since the load instruction will be moved to the OOL code section and the register
   // allocated to hold value of the base register of the memory reference may not be valid in the main line path
   loadMemoryReference = generateS390MemoryReference(*loadMemoryReference, 0, cg);

   TR::Node* lockWordOffsetNode = node->getChild(2);

   int32_t lockWordOffset = lockWordOffsetNode->getConst<int32_t>();

   TR::MemoryReference* lockMemoryReference = generateS390MemoryReference(objectRegister, lockWordOffset, cg);

   const bool generateCompressedLockWord = static_cast<TR_J9VMBase*>(cg->comp()->fe())->generateCompressedLockWord();

   const bool is32BitLock = TR::Compiler->target.is32Bit() || generateCompressedLockWord;
   const bool is32BitLoad = loadNode->getOpCodeValue() == TR::l2a || loadNode->getSymbolReference()->getSymbol()->getSize() == 4;

   if (is32BitLock && is32BitLoad)
      {
      generateSSFInstruction(cg, TR::InstOpCode::LPD, node, registerPair, loadMemoryReference, lockMemoryReference);

      if (loadNode->getOpCodeValue() == TR::l2a)
         {
         generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, loadRegister, loadRegister);
         }
      }
   else
      {
      // LPDG requires memory references to be aligned to a double-wrod boundary
      TR::MemoryReference* alignedLockMemoryReference = lockMemoryReference;
      TR::MemoryReference* alignedLoadMemoryReference = loadMemoryReference;

      bool lockRegisterRequiresShift = false;
      bool loadRegisterRequiresShift = false;

      bool lockRegisterRequires32BitLogicalSignExtension = false;
      bool loadRegisterRequires32BitLogicalSignExtension = false;

      if (is32BitLock)
         {
         if ((lockWordOffset & 7) == 0)
            {
            lockRegisterRequiresShift = true;
            }
         else
            {
            // This is because we must use LPDG to load a 32-bit value using displacement -4
            TR_ASSERT_SAFE_FATAL((lockWordOffset & 3) == 0, "Lockword must be aligned on a word boundary\n");

            lockRegisterRequires32BitLogicalSignExtension = true;
            alignedLockMemoryReference = generateS390MemoryReference(*lockMemoryReference, -4, cg);
            }
         }
      else
         {
         // This is because we must use LPDG to load a 64-bit value
         TR_ASSERT_SAFE_FATAL((lockWordOffset & 7) == 0, "Lockword must be aligned on a double-word boundary\n");
         }

      if (is32BitLoad)
         {
         if ((loadMemoryReference->getOffset() & 7) == 0)
            {
            loadRegisterRequiresShift = true;
            }
         else
            {
            // This is because we must use LPDG to load a 32-bit value using displacement -4
            TR_ASSERT_SAFE_FATAL((loadMemoryReference->getOffset() & 3) == 0, "Field must be aligned on a word boundary\n");

            loadRegisterRequires32BitLogicalSignExtension = true;
            alignedLoadMemoryReference = generateS390MemoryReference(*loadMemoryReference, -4, cg);
            }
         }
      else
         {
         // This is because we must use LPDG to load a 64-bit value
         TR_ASSERT_SAFE_FATAL((loadMemoryReference->getOffset() & 7) == 0, "Field must be aligned on a double-word boundary\n");
         }

      generateSSFInstruction(cg, TR::InstOpCode::LPDG, node, registerPair, alignedLoadMemoryReference, alignedLockMemoryReference);

      if (lockRegisterRequiresShift)
         {
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, lockRegister, lockRegister, 32);
         }

      if (loadRegisterRequiresShift)
         {
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, loadRegister, loadRegister, 32);
         }

      if (lockRegisterRequires32BitLogicalSignExtension)
         {
         generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, lockRegister, lockRegister);
         }

      if (loadRegisterRequires32BitLogicalSignExtension)
         {
         generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, loadRegister, loadRegister);
         }
      }

   if (loadNode->getOpCodeValue() == TR::l2a)
      {
      if (loadNode->getFirstChild()->getOpCodeValue() == TR::lshl)
         {
         auto shiftValue = loadNode->getFirstChild()->getSecondChild()->getConst<uint32_t>();

         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, loadRegister, loadRegister, shiftValue);
         }
      }

   TR::LabelSymbol* mergeLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* fastPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* slowPathLabel = generateLabelSymbol(cg);

   // LPD sets condition code 3 if the pair as not loaded by means of interlocked fetch
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, slowPathLabel);

   // Mask out the recursion and lock reservation bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, lockRegister,  ~(OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED));

   if (is32BitLock && is32BitLoad)
      {
      // Now check if we loaded the lock of the current thread
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, lockRegister, cg->getMethodMetaDataRealRegister(), TR::InstOpCode::COND_BE, mergeLabel, false);

      // Lock could be free as well
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, lockRegister, 0, TR::InstOpCode::COND_BE, mergeLabel, false);
      }
   else
      {
      // Now check if we loaded the lock of the current thread
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CGR, node, lockRegister, cg->getMethodMetaDataRealRegister(), TR::InstOpCode::COND_BE, mergeLabel, false);

      // Lock could be free as well
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, lockRegister, 0, TR::InstOpCode::COND_BE, mergeLabel, false);
      }

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, slowPathLabel);

   TR::Node *monentSymbolReferenceNode = node->getChild(3);
   TR::Node *monexitSymbolReferenceNode = node->getChild(4);

   TR_S390OutOfLineCodeSection* outOfLineCodeSection = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(slowPathLabel, mergeLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outOfLineCodeSection);
   outOfLineCodeSection->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, slowPathLabel);

   // Generate a dynamic debug counter for the fallback path whose execution should be extremely rare
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/success/OOL/%s", cg->comp()->signature()));

   TR::LabelSymbol* helperCallLabel = generateLabelSymbol(cg);
   TR::Instruction* helperCallLabelInstr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperCallLabel);
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->createLinkage(TR_CHelper));
   
   // Calling helper with call node which should NULL
   helperLink->buildDirectDispatch(monentSymbolReferenceNode);

   // Move the original load to the synchronized region
   loadInstruction->move(helperCallLabelInstr);

   helperLink->buildDirectDispatch(monexitSymbolReferenceNode);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);

   outOfLineCodeSection->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mergeLabel, conditions);

   cg->decReferenceCount(synchronizedObjectNode);
   cg->decReferenceCount(loadNode);
   cg->decReferenceCount(lockWordOffsetNode);
   cg->recursivelyDecReferenceCount(monentSymbolReferenceNode);
   cg->recursivelyDecReferenceCount(monexitSymbolReferenceNode);
   cg->stopUsingRegister(lockRegister);
   cg->stopUsingRegister(registerPair);
   }

bool
ReduceSynchronizedFieldLoad::perform()
   {
   bool transformed = false;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      // When concurrent scavenge is enabled we need to load the object reference using a read barrier however
      // there is no guarded load alternative for the LPD instruction. As such this optimization cannot be carried
      // out for object reference loads under concurrent scavenge.
      if (!TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
         {
         if (!cg->comp()->getOption(TR_DisableSynchronizedFieldLoad))
            {
            traceMsg(cg->comp(), "Performing ReduceSynchronizedFieldLoad\n");

            transformed = performOnTreeTops(cg->comp()->getStartTree(), NULL);
            }
         }
      }

   return transformed;
   }

bool
ReduceSynchronizedFieldLoad::performOnTreeTops(TR::TreeTop* startTreeTop, TR::TreeTop* endTreeTop)
   {
   bool transformed = false;

   TR::list<TR::TreeTop*> treeTopsToRemove (getTypedAllocator<TR::TreeTop*>(cg->comp()->allocator()));

   for (TR::TreeTopIterator iter(startTreeTop, cg->comp()); iter != endTreeTop; ++iter)
      {
      treeTopsToRemove.clear();

      if (iter.currentNode()->getOpCodeValue() == TR::monent)
         {
         treeTopsToRemove.push_back(iter.currentTree());

         TR::Node* monentNode = iter.currentNode();

         if (cg->comp()->getOption(TR_TraceCG))
            {
            traceMsg(cg->comp(), "Examining monent [%p]\n", monentNode);
            }

         // Save the current iterator location for tree replacement should the pattern matching succeed
         TR::TreeTopIterator savedIter = iter;

         if (!advanceIterator(iter, endTreeTop))
            {
            continue;
            }

         TR::Node* lookaheadChildNode = iter.currentNode();

         if (lookaheadChildNode->getOpCodeValue() == TR::treetop || lookaheadChildNode->getOpCodeValue() == TR::compressedRefs)
            {
            TR::Node* loadNode = lookaheadChildNode = lookaheadChildNode->getChild(0);

            if (lookaheadChildNode->getOpCodeValue() == TR::l2a)
               {
               TR::Node* l2aChildNode = lookaheadChildNode->getChild(0);

               if (l2aChildNode->isUnneededConversion())
                  {
                  lookaheadChildNode = l2aChildNode->getChild(0);

                  // There may or may not be a compression sequence at this point
                  if (lookaheadChildNode->getOpCodeValue() == TR::lshl && lookaheadChildNode->containsCompressionSequence())
                     {
                     lookaheadChildNode = lookaheadChildNode->getChild(0);

                     if (lookaheadChildNode->getOpCodeValue() == TR::iu2l && lookaheadChildNode->isUnneededConversion())
                        {
                        lookaheadChildNode = lookaheadChildNode->getChild(0);
                        }
                     }
                  }
               }

            if (lookaheadChildNode->getOpCode().isLoadIndirect())
               {
               // Disallow this optimization for 64-bit loads on 31-bit JVM due to register pairs
               if (TR::Compiler->target.is32Bit() && lookaheadChildNode->getSymbolReference()->getSymbol()->getSize() == 8)
                  {
                  continue;
                  }

               TR::Node* synchronizedObjectNode = monentNode->getChild(0);

               // Make sure the object we are synchronizing on is the same object we are loading from
               if (lookaheadChildNode->getChild(0) == synchronizedObjectNode)
                  {
                  treeTopsToRemove.push_back(iter.currentTree());

                  if (cg->comp()->getOption(TR_TraceCG))
                     {
                     traceMsg(cg->comp(), "Found load [%p] from synchronized object [%p]\n", loadNode, synchronizedObjectNode);
                     }

                  if (!advanceIterator(iter, endTreeTop))
                     {
                     continue;
                     }

                  lookaheadChildNode = iter.currentNode();

                  // Skip monexit fences
                  if (lookaheadChildNode->getOpCodeValue() == TR::monexitfence)
                     {
                     if (cg->comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(cg->comp(), "Skipping monexitfence [%p]\n", lookaheadChildNode);
                        }

                     // Note that we cannot actually eliminate these monexit fences. They are used during instruction
                     // selection as barriers for the live monitor stacks. When we encounter astores which hold monitor
                     // objects (i.e. holdsMonitoredObject returns true) we push the object onto a live monitor stack.
                     // We pop off this stack when a monexitfence is encountered. This stack dictates the live monitor
                     // data that gets assigned to every instruction and subsequently gets filled out if we request
                     // a GC map to be generated for a particular instruction.
                     //
                     // For more details please see J9::CodeGenerator::doInstructionSelection()
                     if (!advanceIterator(iter, endTreeTop))
                        {
                        continue;
                        }

                     lookaheadChildNode = iter.currentNode();
                     }

                  // Skip global register stores of the loaded value since this is a NOP at codegen time
                  if (lookaheadChildNode->getOpCodeValue() == TR::aRegStore || lookaheadChildNode->getOpCodeValue() == TR::iRegStore || lookaheadChildNode->getOpCodeValue() == TR::lRegStore)
                     {
                     if (lookaheadChildNode->getChild(0) == loadNode)
                        {
                        if (cg->comp()->getOption(TR_TraceCG))
                           {
                           traceMsg(cg->comp(), "Skipping (a|i|l)regStore [%p]\n", lookaheadChildNode);
                           }

                        if (!advanceIterator(iter, endTreeTop))
                           {
                           continue;
                           }

                        lookaheadChildNode = iter.currentNode();
                        }
                     }

                  // Skip treetop nodes (in case there is a treetop on top of a monexit)
                  if (lookaheadChildNode->getOpCodeValue() == TR::treetop)
                     {
                     if (cg->comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(cg->comp(), "Skipping treetop [%p] and looking at its child instead\n", lookaheadChildNode);
                        }

                     // TODO: Figure out why this happens
                     lookaheadChildNode = lookaheadChildNode->getChild(0);
                     }

                  if (lookaheadChildNode->getOpCodeValue() == TR::monexit)
                     {
                     treeTopsToRemove.push_back(iter.currentTree());

                     TR::Node* monexitNode = lookaheadChildNode;

                     if (cg->comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(cg->comp(), "Examining monexit [%p]\n", monexitNode);
                        }

                     // Make sure the object we are synchronizing on is the same object
                     if (lookaheadChildNode->getChild(0) == synchronizedObjectNode)
                        {
                        int32_t lockWordOffset = static_cast<TR_J9VMBase*>(cg->comp()->fe())->getByteOffsetToLockword(static_cast<TR_OpaqueClassBlock*>(cg->getMonClass(monentNode)));

                        if (cg->comp()->getOption(TR_TraceCG))
                           {
                           traceMsg(cg->comp(), "Lock word offset = %d\n", lockWordOffset);
                           }

                        // LPD(G) is an SSF instruction with a 12-bit displacement
                        if (lockWordOffset > 0 && lockWordOffset < 4096)
                           {
                           if (performTransformation(cg->comp(), "%sReplacing monent [%p] monexit [%p] synchronized region with fabricated call\n", OPT_DETAILS, monentNode, monexitNode))
                              {
                              transformed = true;

                              // Fabricate a special codegen inlined method call symbol reference
                              TR::SymbolReference* methodSymRef = cg->comp()->getSymRefTab()->findOrCreateCodeGenInlinedHelper(TR::SymbolReferenceTable::synchronizedFieldLoadSymbol);

                              TR::Node* callNode = TR::Node::createWithSymRef(loadNode, TR::call, 5, methodSymRef);

                              callNode->setAndIncChild(0, synchronizedObjectNode);
                              callNode->setAndIncChild(1, loadNode);

                              TR::Node* lockWordOffsetNode = TR::Node::iconst (loadNode, lockWordOffset);

                              callNode->setAndIncChild(2, lockWordOffsetNode);

                              TR::Node* monentSymbolReferenceNode = TR::Node::createWithSymRef(loadNode, TR::call, 1, synchronizedObjectNode, monentNode->getSymbolReference());
                              TR::Node* monexitSymbolReferenceNode = TR::Node::createWithSymRef(loadNode, TR::call, 1, synchronizedObjectNode, monexitNode->getSymbolReference());

                              callNode->setAndIncChild(3, monentSymbolReferenceNode);
                              callNode->setAndIncChild(4, monexitSymbolReferenceNode);

                              TR::Node* treeTopNode = TR::Node::create(loadNode, TR::treetop, 1, callNode);

                              TR::TreeTop* callTreeTop = TR::TreeTop::create(cg->comp(), treeTopNode);

                              // Insert fabricated call treetop
                              savedIter.currentTree()->insertBefore(callTreeTop);

                              // Remove pattern matched intermediate treetops
                              for (auto i = treeTopsToRemove.begin(); i != treeTopsToRemove.end(); ++i)
                                 {
                                 (*i)->unlink(true);
                                 }

                              TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/success/%s", cg->comp()->signature()));
                              }
                           }
                        else
                           {
                           TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/lockword-out-of-bounds/%s", cg->comp()->signature()));
                           }
                        }
                     else
                        {
                        TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/monexit-synchronized-object-mismatch/%s", cg->comp()->signature()));
                        }
                     }
                  else
                     {
                     TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/monexit-not-found/%s", cg->comp()->signature()));
                     }
                  }
               else
                  {
                  TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/load-synchronized-object-mismatch/%s", cg->comp()->signature()));
                  }
               }
            else
               {
               TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/load-not-found/%s", cg->comp()->signature()));
               }
            }
         else
            {
            TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/treetop-or-compressedRefs-not-found/%s", cg->comp()->signature()));
            }
         }
      }

   return transformed;
   }

bool
ReduceSynchronizedFieldLoad::advanceIterator(TR::TreeTopIterator& iter, TR::TreeTop* endTreeTop)
   {
   ++iter;

   if (iter == endTreeTop)
      {
      --iter;

      TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/advanceIterator/%s", cg->comp()->signature()));

      return false;
      }

   if (cg->comp()->getOption(TR_TraceCG))
      {
      traceMsg(cg->comp(), "Advancing iterator to [%p]\n", iter.currentNode());
      }

   // Synchronized region may span multiple blocks
   if (iter.currentNode()->getOpCodeValue() == TR::BBEnd)
      {
      TR::TreeTop* nextTreeTop = iter.currentTree()->getNextTreeTop();

      if (nextTreeTop == NULL || nextTreeTop == endTreeTop || !nextTreeTop->getNode()->getBlock()->isExtensionOfPreviousBlock())
         {
         TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/advanceIterator/%s", cg->comp()->signature()));

         return false;
         }
      else
         {
         ++iter;

         // Sanity check that a new block starts after the previous
         TR_ASSERT(iter.currentNode()->getOpCodeValue() == TR::BBStart, "Expecting a BBStart treetop but found %s\n", iter.currentNode()->getOpCode().getName());

         ++iter;

         if (cg->comp()->getOption(TR_TraceCG))
            {
            traceMsg(cg->comp(), "Advancing iterator to [%p] (skipping extended basic block)\n", iter.currentNode());
            }
         }
      }

   return true;
   }
