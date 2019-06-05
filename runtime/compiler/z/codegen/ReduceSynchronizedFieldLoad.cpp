/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/ReduceSynchronizedFieldLoad.hpp"

#include <stddef.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/VMJ9.h"
#include "il/ILOps.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Register.hpp"
#include "z/codegen/J9S390CHelperLinkage.hpp"

#define OPT_DETAILS "O^O REDUCE SYNCHRONIZED FIELD LOAD: "

void
ReduceSynchronizedFieldLoad::inlineSynchronizedFieldLoad(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* synchronizedObjectNode = node->getChild(0);

   // Materialize the object register first because LPD can only deal with base-displacement type memory references and
   // because the object appears directly underneath the indirect load we are guaranteed not to have an index register
   TR::Register* objectRegister = cg->evaluate(synchronizedObjectNode);

   TR::Node* loadNode = node->getChild(1);

   TR::Node* monentSymbolReferenceNode = node->getChild(3);
   TR::Node* monexitSymbolReferenceNode = node->getChild(4);

   TR::LabelSymbol* mergeLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* fastPathLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* slowPathLabel = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fastPathLabel);

   TR_S390OutOfLineCodeSection* outOfLineCodeSection = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(slowPathLabel, mergeLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outOfLineCodeSection);
   outOfLineCodeSection->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, slowPathLabel);

   // Generate a dynamic debug counter for the fallback path whose execution should be extremely rare
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/success/OOL/%s", cg->comp()->signature()));

   TR::S390CHelperLinkage* helperLink = static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));

   // Calling helper with call node which should NULL
   helperLink->buildDirectDispatch(monentSymbolReferenceNode);

   // The logic for evaluating a particular load is non-trivial in both evaluation sequence and setting of the various
   // register flags (collected references, etc.). As such we evaluate the load preemptively and extract the
   // materialized memory reference directly from the load itself for use in LPD.
   TR::Register* loadRegister = cg->evaluate(loadNode);

   // Search for the load memory reference from the previously evaluated load
   TR::Instruction* loadInstruction = cg->getAppendInstruction();

   TR_ASSERT_SAFE_FATAL(loadInstruction->isLoad() && (loadInstruction->getKind() == OMR::Instruction::Kind::IsRX || loadInstruction->getKind() == OMR::Instruction::Kind::IsRXY), "Expecting the append instruction to be a load of kind RX or RXY\n");

   TR::MemoryReference* loadMemoryReference = static_cast<TR::S390RXInstruction*>(loadInstruction)->getMemoryReference();

   TR_ASSERT_SAFE_FATAL(loadMemoryReference->getIndexRegister() == NULL, "Load memory reference must not have an index register\n");

   helperLink->buildDirectDispatch(monexitSymbolReferenceNode);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);

   outOfLineCodeSection->swapInstructionListsWithCompilation();

   TR::Register* lockRegister = cg->allocateRegister();
   TR::RegisterPair* registerPair = cg->allocateConsecutiveRegisterPair(lockRegister, loadRegister);

   TR::RegisterDependencyConditions* conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

   conditions->addPostCondition(registerPair, TR::RealRegister::EvenOddPair);
   conditions->addPostCondition(loadRegister, TR::RealRegister::LegalEvenOfPair);
   conditions->addPostCondition(lockRegister, TR::RealRegister::LegalOddOfPair);

   // Recreate the memory reference since we cannot share the same one for fast and slow paths of the ICF diamond
   loadMemoryReference = generateS390MemoryReference(*loadMemoryReference, 0, cg);

   TR::Node* lockWordOffsetNode = node->getChild(2);

   int32_t lockWordOffset = lockWordOffsetNode->getConst<int32_t>();

   TR::MemoryReference* lockMemoryReference = generateS390MemoryReference(objectRegister, lockWordOffset, cg);

   const bool generateCompressedLockWord = static_cast<TR_J9VMBase*>(cg->comp()->fe())->generateCompressedLockWord();

   const bool is32BitLock = TR::Compiler->target.is32Bit() || generateCompressedLockWord;
   const bool is32BitLoad = J9::DataType::getSize(loadNode->getDataType()) == 4;

   bool lockRegisterRequiresZeroExt = false;
   bool loadRegisterRequiresZeroExt = false;
   bool loadRegisterRequiresSignExt = false;

   if (is32BitLock && is32BitLoad)
      {
      generateSSFInstruction(cg, TR::InstOpCode::LPD, node, registerPair, loadMemoryReference, lockMemoryReference);

      loadRegisterRequiresZeroExt = loadNode->isZeroExtendedTo64BitAtSource();
      loadRegisterRequiresSignExt = loadNode->isSignExtendedTo64BitAtSource();
      }
   else
      {
      // LPDG requires memory references to be aligned to a double-word boundary
      TR::MemoryReference* alignedLockMemoryReference = lockMemoryReference;
      TR::MemoryReference* alignedLoadMemoryReference = loadMemoryReference;

      bool lockRegisterRequiresShift = false;
      bool loadRegisterRequiresShift = false;

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

            lockRegisterRequiresZeroExt = true;
            alignedLockMemoryReference = generateS390MemoryReference(*lockMemoryReference, -4, cg);
            }
         }
      else
         {
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

            loadRegisterRequiresZeroExt = loadNode->isZeroExtendedTo64BitAtSource();
            loadRegisterRequiresSignExt = loadNode->isSignExtendedTo64BitAtSource();

            alignedLoadMemoryReference = generateS390MemoryReference(*loadMemoryReference, -4, cg);
            }
         }
      else
         {
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
      }

   if (lockRegisterRequiresZeroExt)
      {
      generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, lockRegister, lockRegister);
      }

   if (loadRegisterRequiresZeroExt)
      {
      generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, loadRegister, loadRegister);
      }

   if (loadRegisterRequiresSignExt)
      {
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, loadRegister, loadRegister);
      }

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

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
      {
      if (!cg->comp()->getOption(TR_DisableSynchronizedFieldLoad) && cg->comp()->getMethodSymbol()->mayContainMonitors())
         {
         traceMsg(cg->comp(), "Performing ReduceSynchronizedFieldLoad\n");

         for (TR::TreeTopOrderExtendedBlockIterator iter(cg->comp()); iter.getFirst() != NULL; ++iter)
            {
            transformed = performOnTreeTops(iter.getFirst()->getEntry(), iter.getLast()->getExit());
            }
         }
      }

   return transformed;
   }

bool
ReduceSynchronizedFieldLoad::performOnTreeTops(TR::TreeTop* startTreeTop, TR::TreeTop* endTreeTop)
   {
   bool transformed = false;

   for (TR::TreeTopIterator iter(startTreeTop, cg->comp()); iter != endTreeTop; ++iter)
      {
      if (iter.currentNode()->getOpCodeValue() == TR::monent ||
          iter.currentNode()->getOpCodeValue() == TR::treetop && iter.currentNode()->getFirstChild()->getOpCodeValue() == TR::monent)
         {
         TR::TreeTop* monentTreeTop = iter.currentTree();
         TR::Node* monentNode = iter.currentNode()->getOpCodeValue() == TR::monent ?
            iter.currentNode() :
            iter.currentNode()->getFirstChild();

         if (cg->comp()->getOption(TR_TraceCG))
            {
            traceMsg(cg->comp(), "Found monent [%p]\n", monentNode);
            }

         for (++iter; iter != endTreeTop; ++iter)
            {
            if (iter.currentNode()->getOpCodeValue() == TR::monexit ||
                iter.currentNode()->getOpCodeValue() == TR::treetop && iter.currentNode()->getFirstChild()->getOpCodeValue() == TR::monexit)
               {
               TR::TreeTop* monexitTreeTop = iter.currentTree();
               TR::Node* monexitNode = iter.currentNode()->getOpCodeValue() == TR::monexit ?
                  iter.currentNode() :
                  iter.currentNode()->getFirstChild();

               if (cg->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(cg->comp(), "Found monexit [%p]\n", monexitNode);
                  }

               TR::Node* synchronizedObjectNode = monentNode->getFirstChild();

               if (synchronizedObjectNode == monexitNode->getFirstChild())
                  {
                  if (cg->comp()->getOption(TR_TraceCG))
                     {
                     traceMsg(cg->comp(), "Children of monent and monexit are synchronizing on the same object\n", monexitNode);
                     }

                  TR::Node* loadNode = findLoadInSynchornizedRegion(startTreeTop, endTreeTop, monentTreeTop, monexitTreeTop, synchronizedObjectNode);

                  if (loadNode != NULL)
                     {
                     // Disallow this optimization for 64-bit loads on 31-bit JVM due to register pairs
                     if (TR::Compiler->target.is32Bit() && J9::DataType::getSize(loadNode->getDataType()) == 8)
                        {
                        TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/31-bit-register-pairs/%s", cg->comp()->signature()));

                        break;
                        }

                     // When concurrent scavenge is enabled we need to load the object reference using a read barrier however
                     // there is no guarded load alternative for the LPD instruction. As such this optimization cannot be carried
                     // out for object reference loads under concurrent scavenge.
                     if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none && loadNode->getDataType().isAddress())
                        {
                        TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/read-barrier/%s", cg->comp()->signature()));

                        break;
                        }

                     int32_t lockWordOffset = static_cast<TR_J9VMBase*>(cg->comp()->fe())->getByteOffsetToLockword(static_cast<TR_OpaqueClassBlock*>(cg->getMonClass(monentNode)));

                     if (cg->comp()->getOption(TR_TraceCG))
                        {
                        traceMsg(cg->comp(), "Lock word offset = %d\n", lockWordOffset);
                        }

                     // LPD(G) is an SSF instruction with a 12-bit displacement
                     if (lockWordOffset > 0 && lockWordOffset < 4096)
                        {
                        if (performTransformation(cg->comp(), "%sReplacing monent [%p] - monexit [%p] synchronized region on load [%p] with fabricated call\n", OPT_DETAILS, monentNode, monexitNode, loadNode))
                           {
                           transformed = true;

                           // Fabricate a special codegen inlined method call symbol reference
                           TR::SymbolReference* methodSymRef = cg->comp()->getSymRefTab()->findOrCreateCodeGenInlinedHelper(TR::SymbolReferenceTable::synchronizedFieldLoadSymbol);

                           TR::Node* callNode = TR::Node::createWithSymRef(loadNode, TR::call, 5, methodSymRef);

                           callNode->setAndIncChild(0, synchronizedObjectNode);
                           callNode->setAndIncChild(1, loadNode);

                           TR::Node* lockWordOffsetNode = TR::Node::iconst(loadNode, lockWordOffset);

                           callNode->setAndIncChild(2, lockWordOffsetNode);

                           TR::Node* monentSymbolReferenceNode = TR::Node::createWithSymRef(loadNode, TR::call, 1, synchronizedObjectNode, monentNode->getSymbolReference());
                           TR::Node* monexitSymbolReferenceNode = TR::Node::createWithSymRef(loadNode, TR::call, 1, synchronizedObjectNode, monexitNode->getSymbolReference());

                           callNode->setAndIncChild(3, monentSymbolReferenceNode);
                           callNode->setAndIncChild(4, monexitSymbolReferenceNode);

                           TR::Node* treeTopNode = TR::Node::create(loadNode, TR::treetop, 1, callNode);

                           TR::TreeTop* callTreeTop = TR::TreeTop::create(cg->comp(), treeTopNode);

                           // Insert fabricated call treetop
                           monentTreeTop->insertBefore(callTreeTop);

                           // Remove the monitor region
                           monentTreeTop->unlink(true);
                           monexitTreeTop->unlink(true);

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
                     TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/load-not-found/%s", cg->comp()->signature()));
                     }
                  }
               else
                  {
                  TR::DebugCounter::incStaticDebugCounter(cg->comp(), TR::DebugCounter::debugCounterName(cg->comp(), "codegen/z/ReduceSynchronizedFieldLoad/failure/monexit-synchronized-object-mismatch/%s", cg->comp()->signature()));
                  }

               break;
               }
            }

         if (iter == endTreeTop)
            {
            break;
            }
         }
      }

   return transformed;
   }

TR::Node*
ReduceSynchronizedFieldLoad::findLoadInSynchornizedRegion(TR::TreeTop* startTreeTop, TR::TreeTop* endTreeTop, TR::TreeTop* monentTreeTop, TR::TreeTop* monexitTreeTop, TR::Node* synchronizedObjectNode)
   {
   TR::PreorderNodeIterator iter(startTreeTop, cg->comp());

   // First iterate through all the nodes from the start treetop until we reach the monitor provided so that all nodes
   // seen thus far would have already been visited, and hence we will not recurse into them in the subsequent for loop
   // since a reference was already seen. This enables us to carry out the reduce synchronized field load optimization
   // even if there are side-effect nodes within the monitored region - as long as those side-effect nodes have been
   // evaluated outside of the monitored region.
   for (; iter != monentTreeTop->getNextTreeTop(); ++iter)
      {
      TR::Node* currentNode = iter.currentNode();

      if (cg->comp()->getOption(TR_TraceCG))
         {
         traceMsg(cg->comp(), "Iterating node [%p] outside the monitored region\n", currentNode);
         }
      }

   TR::Node* loadNode = NULL;

   for (; iter != monexitTreeTop; ++iter)
      {
      TR::Node* currentNode = iter.currentNode();

      if (cg->comp()->getOption(TR_TraceCG))
         {
         traceMsg(cg->comp(), "Iterating node [%p] inside the monitored region\n", currentNode);
         }

      TR::ILOpCode opcode = currentNode->getOpCode();

      if (opcode.hasSymbolReference() || opcode.isBranch())
         {
         if (loadNode == NULL &&
            opcode.isLoadIndirect() && (opcode.isRef() || opcode.isInt() || opcode.isLong()) &&
            currentNode->getFirstChild() == synchronizedObjectNode)
            {
            if (cg->comp()->getOption(TR_TraceCG))
               {
               traceMsg(cg->comp(), "Found load node [%p]\n", currentNode);
               }

            loadNode = currentNode;
            }
         else
            {
            if (cg->comp()->getOption(TR_TraceCG))
               {
               traceMsg(cg->comp(), "Found sideeffect node [%p] within the monitored region\n", currentNode);
               }

            loadNode = NULL;

            break;
            }
         }
      }

   return loadNode;
   }
