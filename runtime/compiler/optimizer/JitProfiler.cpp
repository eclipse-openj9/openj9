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

#include "optimizer/JitProfiler.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "runtime/J9Runtime.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"

#define OPT_DETAILS "O^O JIT PROFILER: "
#define CLASS_POINTER_MASK 1

// ------------------------------ Things that will hopefully soon be replaced ----------------------------
#define isInvokeVirtualOrInterface( x ) ( (x) == 182 || (x) == 185 || (x) == 231 )
#define isInvokeStaticOrSpecial( x ) ( (x) == 184 || (x) == 234 || (x) == 183 || (x) == 235 )
#define isInstanceOf( x ) ( (x) == 193 )
#define isCheckCast( x ) ( (x) == 192 )
#define isProfilableBranch( x ) ( (x) >= 153 && (x) <= 166 || (x) == 198 || (x) == 199 )
// -------------------------------------------------------------------------------------------------------


TR_JitProfiler::TR_JitProfiler(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}


int32_t TR_JitProfiler::perform()
   {
   if (!comp()->getOptions()->isAnySamplingJProfilingOptionSet())
      {
      if (trace())
         traceMsg(comp(), "JIT Profiling disabled, returning\n");
      return 0;
      }

   if (trace())
      traceMsg(comp(), "Processing method: %s\n", comp()->signature());

   _cfg = comp()->getFlowGraph();
   _lastTreeTop = comp()->findLastTree();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _checklist = new (trStackMemory()) TR::NodeChecklist(comp());

   if (trace())
      comp()->dumpMethodTrees("Before JIT Profiling");

   if (_cfg->getStructure() != NULL)
      _cfg->setStructure(NULL);

   // Iterate through method's treetops
   int32_t changesPerformed = 0;
   for (TR::TreeTop *cursor = comp()->getStartTree(); cursor; cursor = cursor->getNextTreeTop())
      {
      changesPerformed += performOnNode(cursor->getNode(), cursor);
      }

   // Mark the body that it contains samplingJProfiling instructions
   if (changesPerformed)
      {
      if (comp()->getRecompilationInfo()) // DLTs don't have recompilationInfo
         comp()->getRecompilationInfo()->getJittedBodyInfo()->setUsesSamplingJProfiling();
      }

   if (trace())
      comp()->dumpMethodTrees("After JIT Profiling");

   } // scope for stack memory region

   return 0;
   }

const char *
TR_JitProfiler::optDetailString() const throw()
   {
   return "O^O SAMPLING JPROFILER: ";
   }

int32_t TR_JitProfiler::performOnNode(TR::Node *node, TR::TreeTop *tt)
   {
   int32_t changesPerformed = 0;
   if ( _checklist->contains(node) )
      return changesPerformed;

   // Profile address if node is an indirect call
   if (
        node->getOpCode().isCall() &&
        ( comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingInvokeStatic) &&
            node->getSymbol()->getMethodSymbol()->isStatic() ||
          comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingInvokeVirtual) &&
            node->getSymbol()->getMethodSymbol()->isVirtual() ||
          comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingInvokeInterface) &&
            node->getSymbol()->getMethodSymbol()->isInterface()
        )
      )
      {
      addCallProfiling(node, tt, tt->getEnclosingBlock(), addProfilingBypass);
      changesPerformed++;
      }
   else if ( comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingInstanceOf) &&
               node->getOpCodeValue() == TR::instanceof ||
             comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingCheckCast) &&
               node->getOpCodeValue() == TR::checkcast
           )
      {
      addInstanceProfiling(node, tt, tt->getEnclosingBlock(), addProfilingBypass);
      changesPerformed++;
      }
   else if ( comp()->getOptions()->getSamplingJProfilingOption(TR_SamplingJProfilingBranches) &&
             node->getOpCode().isIf()
           )
      {
      addBranchProfiling(node, tt, tt->getEnclosingBlock(), addProfilingBypass);
      changesPerformed++;
      }

   _checklist->add(node);

   // Walk its children
   for (intptrj_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      changesPerformed += performOnNode(child, tt);
      }

   return changesPerformed;
   }


// This method adds an optional "bypass valve" to the profiling code, so that it can be switched off
// on a java thread-by-java thread basis
TR::Block *TR_JitProfiler::appendBranchTree(TR::Node *profilingNode, TR::Block *currentBlock)
   {
   TR::Block *ifBlock = TR::Block::createEmptyBlock(profilingNode, comp(), currentBlock->getFrequency());

   // Add the if tree to jump to profiling code
   TR::Node *flagNode = TR::Node::createWithSymRef(profilingNode, TR::iload, 0,
                           comp()->getSymRefTab()->findOrCreateThreadDebugEventData(4));
   TR::Node *maskNode = TR::Node::create(profilingNode, TR::iconst, 0, 1);
   TR::Node *jitProfile = TR::Node::create(TR::iand, 2, flagNode, maskNode);
   TR::Node *offNode = TR::Node::create(profilingNode, TR::iconst, 0, 0);
   TR::Node *branchNode = TR::Node::createif(
                                           TR::ificmpne,
                                           jitProfile,
                                           offNode,
                                           ifBlock->getEntry());

   currentBlock->append(TR::TreeTop::create(comp(), branchNode));


   // Fix cfg
   _cfg->addNode(ifBlock);
   _cfg->addEdge(currentBlock, ifBlock);

   _lastTreeTop->join(ifBlock->getEntry());
   _lastTreeTop = ifBlock->getExit();


   if (trace())
      traceMsg(comp(), "Inserted Profiling Bypass Branch %p for node %p\n", branchNode, profilingNode);

   _checklist->add(branchNode);

   return ifBlock;
   }


// Create the sequence of blocks to which the profiling trees will be added
// These blocks are an if block, consisting solely of an if to check that we
// have the required space in our profiling buffer, a call block, which contains
// a call to a JIT helper that will be executed when there is insufficient
// space in the profiling buffer, and a profiling block, which will be created empty,
// but to which profiling trees may be added.
TR::Block *TR_JitProfiler::createProfilingBlocks(TR::Node *profilingNode, TR::Block *ifBlock, int32_t bufferSizeRequired)
   {
   // Create the blocks first
   TR::Block *profBlock = TR::Block::createEmptyBlock(profilingNode, comp(), ifBlock->getFrequency());

   TR::Block *callBlock = TR::Block::createEmptyBlock(profilingNode, comp(), VERSIONED_COLD_BLOCK_COUNT);
   callBlock->setIsCold();


   // If block:
   // Compute buffer cursor + size required
   TR::Node *cursorNode = TR::Node::createWithSymRef(profilingNode, TR::aload, 0, getSymRefTab()->findOrCreateProfilingBufferCursorSymbolRef());
   TR::Node *incNode = TR::Node::create(profilingNode, TR::iconst, 0, bufferSizeRequired);
   TR::Node *aiAddNode = TR::Node::create(TR::aiadd, 2, cursorNode, incNode);

   // Check to see if there is enough space left in the buffer
   TR::Node *endNode = TR::Node::createWithSymRef(profilingNode, TR::aload, 0, getSymRefTab()->findOrCreateProfilingBufferEndSymbolRef());
   TR::Node *ifNode = TR::Node::createif(TR::ificmple, aiAddNode, endNode, profBlock->getEntry());
   ifBlock->append(TR::TreeTop::create(comp(), ifNode));


   // Call block:
   // If there is no room left, call runtime helper first to process buffer
   TR::Node *vmThread = TR::Node::createWithSymRef(profilingNode, TR::loadaddr, 0, new (trHeapMemory()) TR::SymbolReference(getSymRefTab(), TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(),"vmThread")));

   TR::SymbolReference *parser = getSymRefTab()->findOrCreateRuntimeHelper(TR_jitProfileParseBuffer, false, false, true);
#ifndef TR_TARGET_X86
   parser->getSymbol()->castToMethodSymbol()->setPreservesAllRegisters();
#endif
   parser->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();

   TR::Node *parserCall = TR::Node::createWithSymRef(profilingNode, TR::call, 1, parser);
   parserCall->setAndIncChild(0, vmThread);

   TR::Node *parserNode = TR::Node::create(TR::treetop, 1, parserCall);
   callBlock->append(TR::TreeTop::create(comp(), parserNode));


   // Fix the CFG
   _cfg->addNode(callBlock);
   _cfg->addNode(profBlock);

   ifBlock->getExit()->join(callBlock->getEntry());
   callBlock->getExit()->join(profBlock->getEntry());
   profBlock->getExit()->setNextTreeTop(NULL);

   _cfg->addEdge(ifBlock, profBlock);
   _cfg->addEdge(ifBlock, callBlock);
   _cfg->addEdge(callBlock, profBlock);

   _lastTreeTop = profBlock->getExit();


   if (trace())
      traceMsg(comp(), "Added buffer condition to block_%d, added call block_%d, and added empty profiling block_%d\n", ifBlock->getNumber(), callBlock->getNumber(), profBlock->getNumber());

   _checklist->add(parserCall);
   _checklist->add(ifNode);

   return profBlock;
   }


// Handles all types of ifs, and could be adapted to handle lookup/switch if we ever decide to implement it...
void TR_JitProfiler::addBranchProfiling(TR::Node *branchNode, TR::TreeTop* tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch)
   {
   TR_ASSERT(branchNode->getOpCode().isIf(), "Attempting to add branch profiling trees to something other than a branch.\n");
   if ( !performTransformation(comp(), "%sAdding profiling trees for conditional branch node [%p]\n", optDetailString(), branchNode) )
      return;

   // Lookup the bytecode associated with the branch
   U_8 *byteCode = (U_8 *) comp()->fej9()->getBytecodePC(branchNode->getOwningMethod(), branchNode->getByteCodeInfo());

   // Check that the bytecode is what we expect
   if ( !isProfilableBranch(*byteCode) )
      return;


   // First split the block (both before, to add profiling trees, and after, so we have a place
   // to branch back to
   currentBlock->split(tt, _cfg, true, true);
   TR::Block *fallThrough = currentBlock->getNextBlock();
   TR::Block *ifBlock = NULL;


   // If we want to add a conditional to turn on and off profiling, then add the necessary conditional to the block
   // and create a new block (ifBlock) to branch to
   if ( addBypassBranch == addProfilingBypass )
      {
      ifBlock = appendBranchTree(branchNode, currentBlock);
      }
   else
      {
      ifBlock = currentBlock;
      }

   // Create empty profiling blocks, and initialize blockCreator
   TR::Block *profilingBlock = createProfilingBlocks(branchNode, ifBlock, TR::Compiler->om.sizeofReferenceAddress() + sizeof(U_8) );
   ProfileBlockCreator blockCreator(this, profilingBlock, fallThrough, branchNode);


   // Adding to profiling block:

   // Record bytecode address
   TR::Node *bpcNode = TR::Compiler->target.is64Bit() ? TR::Node::lconst(branchNode, (uintptrj_t) byteCode) :
                                                       TR::Node::iconst(branchNode, (uintptrj_t) byteCode);
   blockCreator.addProfilingTree(TR::lstorei, bpcNode, TR::Compiler->om.sizeofReferenceAddress());

   // Re-create branch
   TR::Node *firstChildLoadNode  = branchNode->getFirstChild()->duplicateTree();
   TR::Node *secondChildLoadNode = NULL;
   if ( branchNode->getSecondChild() )
      secondChildLoadNode = branchNode->getSecondChild()->duplicateTree();
   TR_JitProfiler::ProfileBlockPair blockPair = blockCreator.addConditionTree(branchNode->getOpCodeValue(), firstChildLoadNode, secondChildLoadNode);

   // Store one on truth side...
   ProfileBlockCreator trueCreator(this, blockPair.trueBlock, branchNode->getBranchDestination()->getEnclosingBlock(), branchNode, TR::Compiler->om.sizeofReferenceAddress());
   TR::Node *trueSideConst = TR::Node::bconst(branchNode, 1);
   trueCreator.addProfilingTree(TR::bstorei, trueSideConst, sizeof(U_8));

   // Store zero on false side...
   ProfileBlockCreator falseCreator(this, blockPair.falseBlock, fallThrough, branchNode, TR::Compiler->om.sizeofReferenceAddress());
   TR::Node *falseSideConst = TR::Node::bconst(branchNode, 0);
   falseCreator.addProfilingTree(TR::bstorei, falseSideConst, sizeof(U_8));


   if (trace())
      traceMsg(comp(), "Populated block_%d to profile branch node [%p]\n", profilingBlock->getNumber(), branchNode);

   return;
   }


// Handles instanceof and checkcast
void TR_JitProfiler::addInstanceProfiling(TR::Node *instanceNode, TR::TreeTop* tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch)
   {
   TR_ASSERT(instanceNode->getOpCodeValue() == TR::instanceof || instanceNode->getOpCodeValue() == TR::checkcast,
             "Attempting to add instance profiling trees to something other than an instance check.\n");
   if ( !performTransformation(comp(), "%sAdding profiling trees for instanceof/checkcast node [%p]\n", optDetailString(), instanceNode) )
      return;

   // Lookup the bytecode associated with the instanceNode
   U_8 *byteCode = (U_8 *) comp()->fej9()->getBytecodePC(instanceNode->getOwningMethod(), instanceNode->getByteCodeInfo());

   // Check that the bytecode is what we expect
   if ( !isInstanceOf(*byteCode) && !isCheckCast(*byteCode) )
      return;


   // Split the block before instanceof/checkcast tree...
   TR::Block *nextBlock = currentBlock->split(tt, _cfg, true, true);
   TR::Block *ifBlock = NULL;


   // If we want to add a conditional to turn on and off profiling, then add the necessary conditional to the block
   // and create a new block (ifBlock) to branch to
   if ( addBypassBranch == addProfilingBypass )
      {
      ifBlock = appendBranchTree(instanceNode, currentBlock);
      }
   else
      {
      ifBlock = currentBlock;
      }

   // Create empty profiling blocks, and initialize blockCreator
   TR::Block *profilingBlock = createProfilingBlocks(instanceNode, ifBlock, 2 * TR::Compiler->om.sizeofReferenceAddress());
   ProfileBlockCreator blockCreator(this, profilingBlock, nextBlock, instanceNode);


   // Adding to profiling block:

   // Record bytecode address
   TR::Node *bpcNode = TR::Compiler->target.is64Bit() ? TR::Node::lconst(instanceNode, (uintptrj_t) byteCode) :
                                                       TR::Node::iconst(instanceNode, (uintptrj_t) byteCode);
   blockCreator.addProfilingTree(TR::lstorei, bpcNode, TR::Compiler->om.sizeofReferenceAddress());

   // Check object is non-null
   TR::Node *classLoadNode = instanceNode->getFirstChild()->duplicateTree();
   TR::Node *nullNode      = TR::Node::aconst(instanceNode, 0);
   TR_JitProfiler::ProfileBlockPair blockPair = blockCreator.addConditionTree(TR::ifacmpeq, classLoadNode, nullNode);

   // Store null on true side...
   ProfileBlockCreator trueCreator(this, blockPair.trueBlock, nextBlock, instanceNode, TR::Compiler->om.sizeofReferenceAddress());
   TR::Node *trueSideNullNode = TR::Node::aconst(instanceNode, 0);
   trueCreator.addProfilingTree(TR::astorei, trueSideNullNode, TR::Compiler->om.sizeofReferenceAddress());

   // Record class pointer
   ProfileBlockCreator falseCreator(this, blockPair.falseBlock, nextBlock, instanceNode, TR::Compiler->om.sizeofReferenceAddress());
   TR::Node *classLoadPtrNode = instanceNode->getFirstChild()->duplicateTree();
   TR::Node *vftNode          = TR::Node::createWithSymRef(TR::aloadi, 1, 1, classLoadPtrNode, getSymRefTab()->findOrCreateVftSymbolRef());
   falseCreator.addProfilingTree(TR::astorei, vftNode, TR::Compiler->om.sizeofReferenceAddress());


   if (trace())
      traceMsg(comp(), "Populated block_%d to profile instanceof/checkcast node [%p]\n", profilingBlock->getNumber(), instanceNode);

   return;
   }


void TR_JitProfiler::addCallProfiling(TR::Node *callNode, TR::TreeTop* tt, TR::Block *currentBlock, ProfilingBlocksBypassBranchFlag addBypassBranch)
   {

   TR_ASSERT(callNode->getOpCode().isCall(), "Attempting to add indirect call profiling trees to something other than an indirect call.\n");
   if ( !performTransformation(comp(), "%sAdding profiling trees for call node [%p]\n", optDetailString(), callNode) )
      return;

   //Lookup the bytecode associated with the callNode
   U_8 *byteCode = (U_8 *) comp()->fej9()->getBytecodePC(callNode->getOwningMethod(), callNode->getByteCodeInfo());

   // We currently only have profiling infrastructure to deal with these flavors of call...
   if ( !( isInvokeVirtualOrInterface(*byteCode) && callNode->getOpCode().isCallIndirect() ) &&
        !isInvokeStaticOrSpecial(*byteCode) )
      return;


   // Start by splitting block after callee
   TR::Block *nextBlock = currentBlock->split(tt, _cfg, true, true);
   TR::Block *ifBlock = NULL;


   // If we want to add a conditional to turn on and off profiling, then add the necessary conditional to the block
   // and create a new block (ifBlock) to branch to
   if ( addBypassBranch == addProfilingBypass )
      {
      ifBlock = appendBranchTree(callNode, currentBlock);
      }
   else
      {
      ifBlock = currentBlock;
      }

   // Create empty profiling blocks, and initialize blockCreator
   TR::Block *profilingBlock = createProfilingBlocks(callNode, ifBlock,
                                                     isInvokeVirtualOrInterface(*byteCode) ?
                                                     4 * TR::Compiler->om.sizeofReferenceAddress() :
                                                     2 * TR::Compiler->om.sizeofReferenceAddress()
                                                     );
   ProfileBlockCreator blockCreator(this, profilingBlock, nextBlock, callNode);


   // Adding to profiling block:

   // Record bytecode address
   TR::Node *bpcNode = TR::Compiler->target.is64Bit() ? TR::Node::lconst(callNode, (uintptrj_t) byteCode) :
                                                       TR::Node::iconst(callNode, (uintptrj_t) byteCode);
   blockCreator.addProfilingTree(TR::lstorei, bpcNode, TR::Compiler->om.sizeofReferenceAddress());

   if ( isInvokeVirtualOrInterface(*byteCode) )
      {
      // Record class pointer
      TR::Node *vftNode  = callNode->getFirstChild()->duplicateTree();//TODO: CAST!
      TR::Node *maskNode = TR::Node::create(callNode, TR::iconst, 0, CLASS_POINTER_MASK);
      TR::Node *orNode   = TR::Node::create(TR::Compiler->target.is64Bit() ? TR::lor : TR::ior, 2, vftNode, maskNode);
      blockCreator.addProfilingTree(TR::astorei, orNode, TR::Compiler->om.sizeofReferenceAddress());

      // Record caller
      TR::Node * callerNode = TR::Node::aconst(callNode, (uintptrj_t)callNode->getOwningMethod());
      callerNode->setIsMethodPointerConstant(true);
      blockCreator.addProfilingTree(TR::astorei, callerNode, TR::Compiler->om.sizeofReferenceAddress());

      // Record callee
      TR::Node * calleeNode = TR::Node::aconst(callNode, 0x0); /* calleeSymbol->getResolvedMethod()->classOfMethod()*/ //TODO fix it for CSI
      blockCreator.addProfilingTree(TR::astorei, calleeNode, TR::Compiler->om.sizeofReferenceAddress());
      }
   else // invokeStatic
      {
      // Record caller
      TR::Node * callerNode = TR::Node::aconst(callNode, (uintptrj_t)callNode->getOwningMethod());
      callerNode->setIsMethodPointerConstant(true);
      blockCreator.addProfilingTree(TR::astorei, callerNode, TR::Compiler->om.sizeofReferenceAddress());
      }


   if (trace())
      traceMsg(comp(), "Populated block_%d to profile call [%p]\n", profilingBlock->getNumber(), callNode);

   return;
   }


TR_JitProfiler::ProfileBlockCreator::ProfileBlockCreator(TR_JitProfiler *jp, TR::Block *profilingBlock, TR::Block *successorBlock, TR::Node *profilingNode, int32_t currentOffset)
   {
   _jitProfiler = jp;
   _profilingBlock = profilingBlock;
   _successorBlock = successorBlock;
   _profilingNode = profilingNode;
   _cursor = TR::Node::createWithSymRef(profilingNode, TR::aload, 0, _jitProfiler->getSymRefTab()->findOrCreateProfilingBufferCursorSymbolRef());
   _currentOffset = currentOffset;
   _createdConditional = false;
   }


TR_JitProfiler::ProfileBlockCreator::~ProfileBlockCreator()
   {
   if ( !_createdConditional )
      {
      if ( _currentOffset != 0 )
         {
         TR::Node *incNode = TR::Node::create(_profilingNode, TR::iconst, 0, _currentOffset);
         TR::Node *addNode = TR::Node::create(TR::aiadd, 2, _cursor, incNode);
         TR::Node *storeNode = TR::Node::createWithSymRef(TR::astore, 1, 1, addNode, _jitProfiler->getSymRefTab()->findOrCreateProfilingBufferCursorSymbolRef());
         _profilingBlock->append(TR::TreeTop::create(_jitProfiler->comp(), storeNode));
         }

      TR::Node *gotoNode = TR::Node::create(_profilingNode, TR::Goto, 0, _successorBlock->getEntry());
      _profilingBlock->append(TR::TreeTop::create(_jitProfiler->comp(), gotoNode));

      _jitProfiler->_cfg->addEdge(_profilingBlock, _successorBlock);
      }
   }


void TR_JitProfiler::ProfileBlockCreator::addProfilingTree(TR::ILOpCodes storeType, TR::Node *storeValue, int32_t storeWidth)
   {

   TR::Node *storeOffset = NULL;
   if ( _currentOffset == 0 )
      {
      storeOffset = _cursor;
      }
   else
      {
      TR::Node *incNode = TR::Node::create(_profilingNode, TR::iconst, 0, _currentOffset);
      storeOffset = TR::Node::create(TR::aiadd, 2, _cursor, incNode);
      }
   _currentOffset += storeWidth;


   TR::Node *storeNode = TR::Node::createWithSymRef(storeType, 2, 2, storeOffset, storeValue, _jitProfiler->getSymRefTab()->findOrCreateProfilingBufferSymbolRef());
   _profilingBlock->append(TR::TreeTop::create(_jitProfiler->comp(), storeNode));

   return;
   }


TR_JitProfiler::ProfileBlockPair TR_JitProfiler::ProfileBlockCreator::addConditionTree(TR::ILOpCodes conditionType, TR::Node *firstChild, TR::Node *secondChild)
   {
   // Create two new blocks
   TR::Block *trueBlock = TR::Block::createEmptyBlock(_profilingNode, _jitProfiler->comp(), _profilingBlock->getFrequency() / 2);
   TR::Block *falseBlock = TR::Block::createEmptyBlock(_profilingNode, _jitProfiler->comp(), _profilingBlock->getFrequency() / 2);

   // Create if node
   TR::Node *ifNode = TR::Node::createif(conditionType, firstChild, secondChild, trueBlock->getEntry());
   _profilingBlock->append(TR::TreeTop::create(_jitProfiler->comp(), ifNode));

   // Fix cfg
   _jitProfiler->_cfg->addNode(trueBlock);
   _jitProfiler->_cfg->addNode(falseBlock);

   _jitProfiler->_cfg->addEdge(_profilingBlock, trueBlock);
   _jitProfiler->_cfg->addEdge(_profilingBlock, falseBlock);

   _profilingBlock->getExit()->join(falseBlock->getEntry());
   falseBlock->getExit()->join(trueBlock->getEntry());
   trueBlock->getExit()->setNextTreeTop(NULL);

   _jitProfiler->_lastTreeTop = trueBlock->getExit();

   // We no longer need to add a goto to the end of this block
   _createdConditional = true;

   _jitProfiler->_checklist->add(ifNode);

   return TR_JitProfiler::ProfileBlockPair(trueBlock, falseBlock);
   }

