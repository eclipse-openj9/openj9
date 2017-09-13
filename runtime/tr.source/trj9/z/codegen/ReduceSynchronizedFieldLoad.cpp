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
   // Materialize the object register first because LPD can only deal with base-displacement type memory references and
   // because the object appears directly underneath the indirect load we are guaranteed not to have an index register
   TR::Register* objectRegister = cg->evaluate(node->getChild(0));
   TR::Register* lockRegister = cg->allocateRegister();

   // The logic for evaluating a particular load is non-trivial in both evaluation sequence and setting of the various
   // register flags (collected references, etc.). As such we evaluate the load preemptively and extract the
   // materialized memory reference directly from the load itself for use in LPD.
   TR::Register* loadRegister = cg->evaluate(node->getChild(1));

   TR::RegisterPair* registerPair = cg->allocateConsecutiveRegisterPair(lockRegister, loadRegister);

   TR::RegisterDependencyConditions* conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg);

   conditions->addPostCondition(registerPair, TR::RealRegister::EvenOddPair);
   conditions->addPostCondition(loadRegister, TR::RealRegister::LegalEvenOfPair);
   conditions->addPostCondition(lockRegister, TR::RealRegister::LegalOddOfPair);

   // Search for the load memory reference from the previously evaluated load
   TR::Instruction* loadInstruction = cg->comp()->getAppendInstruction();
   // Sanity check for loads
   TR_ASSERT_SAFE_FATAL(loadInstruction->isLoad(), "Expecting the append instruction to be a load");

   // Sanity check for instruction kind as we are doing an unsafe cast following this
   TR_ASSERT_SAFE_FATAL(loadInstruction->getKind() == OMR::Instruction::Kind::IsRX || loadInstruction->getKind() == OMR::Instruction::Kind::IsRXY, "Expecting the append instruction to be a load of kind RX or RXY\n");

   TR::MemoryReference* loadMemoryReference = static_cast<TR::S390RXInstruction*>(loadInstruction)->getMemoryReference();

   TR_ASSERT_SAFE_FATAL(loadMemoryReference->getIndexRegister() == NULL, "Load memory reference must not have an index register\n");

   int32_t lockWordOffset = node->getChild(2)->getConst<int32_t>();

   TR::MemoryReference* lockMemoryReference = generateS390MemoryReference(objectRegister, lockWordOffset, cg);

   const bool generateCompressedLockWord = static_cast<TR_J9VMBase*>(cg->comp()->fe())->generateCompressedLockWord();

   const bool generate32BitLoads = TR::Compiler->target.is32Bit() || (TR::Compiler->target.is64Bit() && generateCompressedLockWord && node->getChild(1)->getOpCodeValue() == TR::iloadi);

   if (generate32BitLoads)
      {
      TR::MemoryReference* reusedLoadMemoryReference = generateS390MemoryReference(*loadMemoryReference, 0, cg);

      generateSSFInstruction(cg, TR::InstOpCode::LPD, node, registerPair, reusedLoadMemoryReference, lockMemoryReference);

      if (node->getChild(1)->getOpCodeValue() == TR::l2a)
         {
         generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, loadRegister, loadRegister);
         }
      }
   else
      {
      TR_ASSERT_SAFE_FATAL((lockWordOffset & 7) == 0 || (generateCompressedLockWord && (lockWordOffset & 3) == 0),
            "Lockword must be aligned on a double-word boundary or on a word boundary if the lockword is compressed\n");

      if ((loadMemoryReference->getOffset() & 7) != 0)
         {
         TR_ASSERT_SAFE_FATAL((loadMemoryReference->getOffset() & 3) == 0, "Integer field must be aligned on a word boundary\n");

         TR::MemoryReference* alignedLoadMemoryReference = generateS390MemoryReference(*loadMemoryReference, -4, cg);

         generateSSFInstruction(cg, TR::InstOpCode::LPDG, node, registerPair, alignedLoadMemoryReference, lockMemoryReference);

         // For 64-bit uncompressed lockwords, when the load memory reference is not aligned to double-word boundaries,
         // the loaded value is at the lower 32-bit. We need an AND to zero out the high half of loadRegister.
         if (node->getChild(1)->getOpCodeValue() == TR::iloadi)
            {
            generateRILInstruction(cg, TR::InstOpCode::NIHF, node, loadRegister, static_cast<uintptrj_t>(0));
            }
         }
      else
         {
         TR::MemoryReference* reusedLoadMemoryReference = generateS390MemoryReference(*loadMemoryReference, 0, cg);

         generateSSFInstruction(cg, TR::InstOpCode::LPDG, node, registerPair, reusedLoadMemoryReference, lockMemoryReference);

         // We know we are generating for a 64-bit target. Furthermore we know the field we are loading is on a double-
         // word boundary. However if we are loading an integer it only occupies 32-bits of storage, and since we must
         // use LPDG to load the field it means the field will be generated in the most significant 32-bits of the
         // loaded register. Hence we must carry out a right shift to move the loaded value into the least significant
         // 32-bits of the respective register.
         if (node->getChild(1)->getOpCodeValue() == TR::iloadi)
            {
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, loadRegister, loadRegister, 32);
            }
         }
      }

   TR::LabelSymbol* mergeLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* fastPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* slowPathLabel = generateLabelSymbol(cg);

   // LPD sets condition code 3 if the pair as not loaded by means of interlocked fetch
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, slowPathLabel);

   // Mask out the recursion and lock reservation bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, lockRegister,  ~(OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_RESERVED));

   if (generate32BitLoads)
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
   TR::Node *monentNode = node->getChild(3);
   TR::Node *monexitNode = node->getChild(4);
   TR_S390OutOfLineCodeSection* outOfLineCodeSection = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(slowPathLabel, mergeLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outOfLineCodeSection);
   outOfLineCodeSection->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, slowPathLabel);

   TR::LabelSymbol* helperCallLabel = generateLabelSymbol(cg);
   TR::Instruction* helperCallLabelInstr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperCallLabel);
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->createLinkage(TR_CHelper));
   
   // Calling helper with call node which should NULL
   helperLink->buildDirectDispatch(monentNode);

   // Move the original load to the synchronized region
   loadInstruction->move(helperCallLabelInstr);

   helperLink->buildDirectDispatch(monexitNode);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);

   outOfLineCodeSection->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mergeLabel, conditions);

   for (auto i = 0; i < 3; ++i)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   
   cg->recursivelyDecReferenceCount(monentNode);
   cg->recursivelyDecReferenceCount(monexitNode);
   cg->stopUsingRegister(lockRegister);
   cg->stopUsingRegister(registerPair);
   }

bool
ReduceSynchronizedFieldLoad::perform()
   {
   bool transformed = false;

   if (cg->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z196))
      {
      if (!cg->comp()->getOption(TR_DisableSynchronizedFieldLoad))
         {
         traceMsg(cg->comp(), "Performing ReduceSynchronizedFieldLoad\n");

         transformed = performOnTreeTops(cg->comp()->getStartTree(), NULL);
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

            // TODO: Implement support for compreesedRefs shift > 0
            if (lookaheadChildNode->getOpCodeValue() == TR::l2a)
               {
               TR::Node* l2aChildNode = lookaheadChildNode->getChild(0);

               if (l2aChildNode->isUnneededConversion())
                  {
                  lookaheadChildNode = l2aChildNode->getChild(0);
                  }
               }

            // Look for object or integer loads. When concurrent scavenge is enabled we need to load the object
            // reference using a read barrier however there is no guarded load alternative for the LPD instruction.
            // As such this optimization cannot be carried out for object reference loads under concurrent scavenge.
            if ((lookaheadChildNode->getOpCodeValue() == TR::aloadi && !TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads()) ||
                  lookaheadChildNode->getOpCodeValue() == TR::iloadi)
               {
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
                  if (lookaheadChildNode->getOpCodeValue() == TR::aRegStore || lookaheadChildNode->getOpCodeValue() == TR::iRegStore)
                     {
                     if (lookaheadChildNode->getChild(0) == loadNode)
                        {
                        if (cg->comp()->getOption(TR_TraceCG))
                           {
                           traceMsg(cg->comp(), "Skipping (a|i)regStore [%p]\n", lookaheadChildNode);
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

                        if (lockWordOffset > 0 && lockWordOffset < 4096)
                           {
                           const bool generateCompressedLockWord = static_cast<TR_J9VMBase*>(cg->comp()->fe())->generateCompressedLockWord();

                           // TODO: We need to think harder about 64-bit support to make sure the fields we are loading
                           // are on a double-word boundary for use with LPD. We need to be careful about how we handle
                           // compressed lock words and 64-bit fields in all cases. 64-bit support is disabled at the
                           // moment via the following mechanism.
                           const bool generate32BitLoads = TR::Compiler->target.is32Bit() || (TR::Compiler->target.is64Bit() && generateCompressedLockWord && loadNode->getOpCodeValue() == TR::iloadi);

                           if (generate32BitLoads && performTransformation(cg->comp(), "%sReplacing monent [%p] monexit [%p] synchronized region with fabricated call\n", OPT_DETAILS, monentNode, monexitNode))
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
                              }
                           }
                        }
                     }
                  }
               }
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
