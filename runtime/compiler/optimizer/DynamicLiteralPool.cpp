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

#include "optimizer/DynamicLiteralPool.hpp"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include "infra/Stack.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"
#include "runtime/J9Runtime.hpp"


TR_DynamicLiteralPool::TR_DynamicLiteralPool(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   _litPoolAddressSym=NULL;
   setAloadFromCurrentBlock(NULL);
   _changed = false;
   }

int32_t TR_DynamicLiteralPool::perform()
   {
   if (!cg()->supportsOnDemandLiteralPool())
      return 1;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   process(comp()->getStartTree(), NULL);

   if (performTransformation(comp(), "%s free reserved literal pool register\n", optDetailString()))
      {
      if (cg()->supportsOnDemandLiteralPool())
         {
         cg()->setOnDemandLiteralPoolRun(true);
         cg()->enableLiteralPoolRegisterForGRA();
         }
      }

   postPerformOnBlocks();
   } // scope of the stack memory region

   if (_changed)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      optimizer()->setAliasSetsAreValid(false);
      requestOpt(OMR::localCSE, true);
      }
   // need that to remove the astore if no literal pool is required
   requestOpt(OMR::localDeadStoreElimination, true);

   return 1;
   }


int32_t TR_DynamicLiteralPool::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop()->getNextTreeTop());
   return 0;
   }


void TR_DynamicLiteralPool::initLiteralPoolBase()
   {
   TR::Node    *tempValue;
   TR::TreeTop *storeToTempTT;
   TR::Node *firstNode,*storeToTemp;
   TR::Block *firstBlock;
   TR::SymbolReference * tempStaticSym;

   firstNode = comp()->getStartTree()->getNode();
   firstBlock = firstNode->getBlock();
   tempStaticSym = getSymRefTab()->createKnownStaticDataSymbolRef(0, TR::Address);
   _litPoolAddressSym = getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);

   tempValue = TR::Node::createWithSymRef(firstNode, TR::aload, 0, tempStaticSym);
   storeToTemp = TR::Node::createWithSymRef(TR::astore, 1, 1, tempValue, _litPoolAddressSym);

   tempStaticSym->setLiteralPoolAddress();
   _litPoolAddressSym->setFromLiteralPool();

   // make sure GC knows about the two symbols, otherwise
   // GC will examine the memory pointed to by the temp/static
   // and will crash because the memory (i.e. the literal pool) is not a J9Object
   tempStaticSym->getSymbol()->setNotCollected();
   getLitPoolAddressSym()->getSymbol()->setNotCollected();

   storeToTempTT = TR::TreeTop::create(comp(), storeToTemp);
   firstBlock->prepend(storeToTempTT);
   _changed = true;
   dumpOptDetails(comp(), "Literal pool base pointer initialized to %p \n", storeToTemp);

   }

int32_t TR_DynamicLiteralPool::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   TR::TreeTop *tt, *exitTree;
   vcount_t visitCount = comp()->incVisitCount();
   for (tt = startTree; (tt != endTree); tt = exitTree->getNextRealTreeTop())
      {
      TR::Block *block = tt->getNode()->getBlock();
      _currentBlock=block;
      exitTree = block->getEntry()->getExtendedBlockExitTreeTop();
      processBlock(block, visitCount);
      }
   return 1;
   }

bool TR_DynamicLiteralPool::processBlock(TR::Block *block, vcount_t visitCount)
   {
   TR::TreeTop *exit = block->getEntry()->getExtendedBlockExitTreeTop();
   setAloadFromCurrentBlock(NULL);

   for (TR::TreeTop *tt = block->getEntry(); tt != exit; tt = tt->getNextRealTreeTop())
      {
      setNumChild(-1);
      visitTreeTop(tt, 0, 0, tt->getNode(), visitCount);
      }
   return true;
   }

bool TR_DynamicLiteralPool::visitTreeTop(TR::TreeTop * tt, TR::Node *grandParent, TR::Node *parent, TR::Node *node, vcount_t visitCount)
   {
   int32_t firstChild = 0;

   if (node->getVisitCount() == visitCount)
      return true;
   node->setVisitCount(visitCount);

   TR::ILOpCode opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   TR_OpaqueClassBlock * classInfo = 0;

   if (cg()->supportsOnDemandLiteralPool())
      {
      // do work on this node
      if (opCode.isLoadConst())
         {
         // reset visitcount to make sure all parents see this child
         if (node->getReferenceCount()>1)  node->setVisitCount(visitCount-1);

         dumpOptDetails(comp(), "looking at const node %p (%s)\n", node, opCode.getName());
         transformLitPoolConst(grandParent, parent,node);
         }
#if defined(TR_TARGET_S390)
      // This will get turned into an exp() call.  The first parm is a dummy.
      else if (opCodeValue == TR::dexp)
         {
         TR::Node * child=node->getChild(0);
         if (child->getOpCode().isLoadConst() && child->getDouble() == exp(1.0))
            {
            firstChild = 1;
            }
         }
      else if (opCodeValue == TR::fexp)
         {
         TR::Node * child=node->getChild(0);
         if (child->getOpCode().isLoadConst() && child->getFloat() == exp(1.0))
            {
            firstChild = 1;
            }
         }
#endif
      else if (opCode.hasSymbolReference() &&
               node->getSymbol()->isStatic() &&
               !node->getSymbolReference()->isLiteralPoolAddress() &&
               (node->getSymbolReference() != comp()->getSymRefTab()->findThisRangeExtensionSymRef()))
         {
         dumpOptDetails(comp(), "looking at the static symref for node %p (%s)\n", node, opCode.getName());
         transformStaticSymRefToIndirectLoad(tt, parent, node);
         }

      // add extra aload child for CurrentTimeMaxPrecision call
      if (opCode.isCall() &&
          comp()->getSymRefTab()->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::currentTimeMaxPrecisionSymbol))
         {
         addNewAloadChild(node);
         }
      // add extra aload child for float conversions
      else if (opCodeValue==TR::fbits2i || opCodeValue==TR::dbits2l)
         {
         addNewAloadChild(node);
         }
      }

   for (int32_t i = firstChild; i < node->getNumChildren(); ++i)
      {
      setNumChild(i);
      visitTreeTop(0, parent, node, node->getChild(i), visitCount);
      }
   return true;
   }

bool TR_DynamicLiteralPool::transformLitPoolConst(TR::Node *grandParent, TR::Node *parent, TR::Node *child)
   {
   switch (child->getOpCodeValue())
      {
      case TR::fconst:
         if (performTransformation(comp(), "%s Float Constant\n", optDetailString()))
            {
            _changed = true;
            transformConstToIndirectLoad(parent, child);
            }
         else
            return false;
         break;
      case TR::dconst:
         // LZDR can be used to load floating point zero
         if (child->getDouble() != 0.0 && performTransformation(comp(), "%s Double Constant\n", optDetailString()))
            {
            _changed = true;
            transformConstToIndirectLoad(parent, child);
            }
         else
            return false;
         break;
      case TR::aconst:
         if (child->isClassUnloadingConst())
            return false;
      case TR::iconst:
      case TR::lconst:
      case TR::bconst:
      case TR::cconst:
      case TR::sconst:
      case TR::iuconst:
      case TR::luconst:
      case TR::buconst:
         if (transformNeeded(grandParent, parent, child))
            {
            if (performTransformation(comp(), "%s Large non-float Constant\n", optDetailString()))
               {
               _changed = true;
               transformConstToIndirectLoad(parent, child);
               }
            else
               {
               return false;
               }
            }
         break;
      default:
         if (child->getDataType().isBCD() || child->getDataType() == TR::Aggregate)
            return false;
         else
            TR_ASSERT(false,"Unknown const %p (type %s)\n",child,child->getDataType().toString());
         break;
      }
   return true;
   }

bool TR_DynamicLiteralPool::transformNeeded(TR::Node *grandParent, TR::Node *parent, TR::Node *child)
   {
   TR::ILOpCode parentOpCode = parent->getOpCode();
   TR::ILOpCodes parentOpCodeValue = parentOpCode.getOpCodeValue();

   // need to ensure a constant setsign can be retrieved
   if (parentOpCode.isSetSign())
      return false;

   if (child->getType().isIntegral() && parentOpCode.skipDynamicLitPoolOnInts())
      return false;

   // If the hardware supports relative fixed point loads/stores from the literal pool,
   // i.e. On z10 or higher, LRL/LGRL/STRL/STGRL, then do not transform to use literal pool
   // base register.
   if ( (child->getType().isIntegral() || child->getType().isAddress()) &&
        cg()->supportsDirectIntegralLoadStoresFromLiteralPool())
      return false;

   // don't modify const children of multiplication or division to
   // allow strength reduction to happen. The check bellow could be
   // more precise, but the logic to determine if strength reduction
   // is possible is quite complex and if we have it here, it needs
   // be kept in sync with the logic in evaluators.
   // In the worst case, when constant actually needs to go lit pool
   // the code wiil insert extra LARL pre-loading lit pool entry
   // The cost of this LARL is negligible considering the cost of MULT or DIV
   if (parentOpCode.isMul() || parentOpCode.isDiv()) return false;
   // If this looks like a pattern that could be evaluated to a test under mask type instruction
   // then do not use the lit pool as this will obfuscate the pattern in the evaluator.
   if (cg()->getSupportsTestUnderMask() &&
       parentOpCode.isIf() &&
       parent->getNumChildren() == 2 &&
       parent->getSecondChild()->getOpCode().isLoadConst() &&
       isPowerOf2(parent->getSecondChild()->get64bitIntegralValue()))
      {
      return false;
      }
   if (cg()->getSupportsTestUnderMask() &&
       grandParent && grandParent->getOpCode().isIf() &&
       parentOpCode.isAnd() &&
       parent->getNumChildren() == 2 &&
       parent->getSecondChild()->getOpCode().isLoadConst() &&
       isPowerOf2(parent->getSecondChild()->get64bitIntegralValue()))
      {
      return false;
      }

   // Check for arithmetic operations for add, sub, and non-guard compares.
   // Guarded if-stmts might contain class pointers will may be unloaded.  Such
   // scenarios will be handled by constLoadNeedsLitPool below.
   if (parentOpCode.isAdd() || parentOpCode.isSub() ||
       (parentOpCode.isBooleanCompare() && !parent->isTheVirtualGuardForAGuardedInlinedCall()))
      {
      if (child->getOpCode().isLong() && (TR::Compiler->target.is32Bit()))
         return false; //avasilev: to be handled better
      else
         {
         TR::ILOpCodes oldOpCode = child->getOpCodeValue();
         if (oldOpCode == TR::iuconst &&
             (parentOpCode.isAdd() || parentOpCode.isSub()))
            {
            TR::Node::recreate(child, TR::iconst);
            }
         else if (oldOpCode == TR::luconst &&
             (parentOpCode.isAdd() || parentOpCode.isSub()))
            {
            TR::Node::recreate(child, TR::lconst);
            }
         else if (oldOpCode == TR::cconst &&
             (parentOpCode.isAdd() || parentOpCode.isSub()))
            {
            TR::Node::recreate(child, TR::sconst);
            }

         bool needs = (cg()->arithmeticNeedsLiteralFromPool(child));
         TR::Node::recreate(child, oldOpCode);
         return needs;
         }
      }
   if (parentOpCode.isAnd() || parentOpCode.isOr() || parentOpCode.isXor() || parentOpCode.isNeg())
      {
      if (child->getOpCode().isLong() && (TR::Compiler->target.is32Bit()))
         return false; // to be handled better
      else
         return (cg()->bitwiseOpNeedsLiteralFromPool(parent,child));
      }
   if (parentOpCode.isLeftShift() || parentOpCode.isRightShift() ||  parentOpCode.isShiftLogical())
      return false;

   // TR::arraytranslateAndTest may throw AIOB, but that is taken care of by the evaluator.  Also, the
   // iconst may be the character to search.
   if (parentOpCode.isBndCheck() && parentOpCodeValue != TR::arraytranslateAndTest)
      return false;

   // TR::arrayset evaluator expects to see const child, and it's evaluated
   // specially. Don't modify const children of TR::arrayset. See 74553
   if (parentOpCodeValue == TR::arrayset) return false;

   if (child->isClassUnloadingConst()) return false;

   // TODO: This function can be simplified because the context in which it is used the following call will always result yield false.
   // As such many paths through this function that return false are effectively dead and can be removed. There is only one path that
   // does not return false, and this is the only path that needs to exist, though my intuition is that this path too should be verified
   // as it may no longer be applicable.
   return (cg()->constLoadNeedsLiteralFromPool(child));
   }

bool TR_DynamicLiteralPool::transformConstToIndirectLoad(TR::Node *parent, TR::Node *child)
   {
   //TR::Node *loadLiteralFromThePool;
   //TR::Node *loadLiteralPoolAddress;
   TR::Node *constCopy, *indirectLoad;
   TR::SymbolReference *shadow;

   dumpOptDetails(comp(), "transforming const %p (%s)\n", child, child->getOpCode().getName());

   TR::Node *addrNode;
   addrNode = getAloadFromCurrentBlock(parent);

   constCopy =TR::Node::copy(child);
   shadow = getSymRefTab()->findOrCreateImmutableGenericIntShadowSymbolReference((intptrj_t)constCopy);
   shadow->setLiteralPoolAddress();

   if (child->getReferenceCount() > 1)  // rematerialize the const by creating new indirect load node
      {
      indirectLoad=TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(child->getDataType()), 1, 1, addrNode, shadow);
      dumpOptDetails(comp(), "New node created %p, refcount of const child was %d\n",indirectLoad,child->getReferenceCount());
      parent->setAndIncChild(getNumChild(),indirectLoad);
      child->decReferenceCount();
      }
   else
      {
      //convert const node to indirect load node
      child->setNumChildren(1);
      child = TR::Node::recreateWithSymRef(child, comp()->il.opCodeForIndirectLoad(child->getDataType()), shadow);
      child->setAndIncChild(0, addrNode);
      }

   return true;
   }

bool TR_DynamicLiteralPool::transformStaticSymRefToIndirectLoad(TR::TreeTop * tt, TR::Node *parent, TR::Node * & child)
   {
   // Only transform direct references
   if (child->getOpCode().isIndirect())
      return false;

   TR::SymbolReference * childSymRef = child->getSymbolReference();
   //childSymRef->setFromLiteralPool();
   TR::ILOpCode childOpcode=child->getOpCode();
   TR::ILOpCodes childOpcodeValue=child->getOpCodeValue();
   
   if (childOpcodeValue==TR::loadaddr)
      {
      return false;
      }
   else
      {
      TR::SymbolReference *intChildShadow = NULL;
      
      if (childSymRef->isUnresolved())
         {
         if (cg()->supportsDirectIntegralLoadStoresFromLiteralPool())
            {
            return false;
            }

         childSymRef->setFromLiteralPool();
         
         if (performTransformation(comp(), "%s unresolved static ref for node %p (%s)\n", optDetailString(), child, child->getOpCode().getName()))
            {
            _changed = true;
            intChildShadow = getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(0);
            }
         else
            {
            return false;
            }
         }
      else
         {
         return false;
         }

      intChildShadow->setFromLiteralPool();
      getSymRefTab()->aliasBuilder.setLitPoolGenericIntShadowHasBeenCreated();

      TR::Node * loadLiteralFromThePool = TR::Node::createWithSymRef(TR::aloadi, 1, 1, getAloadFromCurrentBlock(child), childSymRef);
      loadLiteralFromThePool->getSymbol()->setNotCollected();
      if (childOpcodeValue==TR::awrtbar)
         {
         child->getFirstChild()->decReferenceCount();
         child->getSecondChild()->decReferenceCount();
         child = TR::Node::create(TR::awrtbari, 3, loadLiteralFromThePool, child->getFirstChild(), child->getSecondChild());
         if (parent)
            parent->setAndIncChild(0, child);
         else
            tt->setNode(child);
         }
      else
         {
         //TR::DataType type = childSymRef->getSymbol()->castToStaticSymbol()->getDataType();
         TR::DataType type = child->getDataType();
         if (childOpcode.isStore())
            {
            child->setSecond(child->getFirstChild());
            TR::Node::recreate(child, comp()->il.opCodeForIndirectStore(type));
            }
         else if (childOpcode.isLoad())
            {
            TR::Node::recreate(child, comp()->il.opCodeForIndirectLoad(type));
            }
         else
            {
            TR_ASSERT(0,"not Load or Store, what is it ?\n");
            }
         child->setAndIncChild(0, loadLiteralFromThePool);
         child->setNumChildren(child->getNumChildren()+1);
         }
      child->setSymbolReference(intChildShadow);

      dumpOptDetails(comp(), "created TR::aloadi %p from child %p\n", loadLiteralFromThePool, child);
      }

   return true;
   }

bool TR_DynamicLiteralPool::addNewAloadChild(TR::Node *node)
   {
   if (!performTransformation(comp(), "%s creating new aload child for node %p (%s) %p \n", optDetailString(),node,node->getOpCode().getName()))
      return false;
   _changed = true;
   node->setAndIncChild(node->getNumChildren(),getAloadFromCurrentBlock(node));
   node->setNumChildren(node->getNumChildren()+1);
   return true;
   }


bool TR_DynamicLiteralPool::handleNodeUsingVMThread(TR::TreeTop * tt, TR::Node *parent, TR::Node *node, vcount_t visitCount)
   {
   /*
   if (node->getOpCode().isLoadVarOrStore())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      if (symRef->isUnresolved())
         {
         }
      else
         {
         if (symRef->getSymbol()->isMethodMetaData())
            {
            TR_ASSERT(node->getOpCode().isDirect(), "Load/store from meta data should use a direct opcode\n");
            TR::SymbolReference *metaDataShadow = getSymRefTab()->findOrCreateGenericIntShadowSymbolReference(symRef->getOffset());
            TR::Node * loadVMThreadFromTheStack = getVMThreadAloadFromCurrentBlock(node);
            TR::DataType type = node->getDataType();
            if (node->getOpCode().isStore())
               {
               node->setSecond(node->getFirstChild());
               TR::Node::recreate(node, comp()->il.opCodeForIndirectStore(type));
               }
            else if (node->getOpCode().isLoad())
               {
               TR::Node::recreate(node, comp()->il.opCodeForIndirectLoad(type));
               }
            else
               {
               TR_ASSERT(0,"not Load or Store, what is it ?\n");
               }
            node->setAndIncChild(0, loadVMThreadFromTheStack);
            node->setNumChildren(node->getNumChildren()+1);
            node->setSymbolReference(metaDataShadow);
            }

         if (node->getOpCode().isWrtBar())
            {
            }
         }
      }
   */

   return true;
   }


bool TR_DynamicLiteralPool::handleNodeUsingSystemStack(TR::TreeTop * tt, TR::Node *parent, TR::Node *node, vcount_t visitCount)
   {
   return true;
   }


TR::SymbolReference * getVMThreadSym()
      {
      return NULL;
      }

TR::Node *TR_DynamicLiteralPool::getVMThreadAloadFromCurrentBlock(TR::Node *parent)
   {
   if (_vmThreadAloadFromCurrentBlock==NULL)
      {
      setVMThreadAloadFromCurrentBlock(TR::Node::createWithSymRef(parent, TR::aload, 0, getVMThreadSym()));
      dumpOptDetails(comp(), "New VM thread aload needed, it is: %p!\n", _vmThreadAloadFromCurrentBlock);
      }
   else
      {
      dumpOptDetails(comp(), "Can re-use VM thread aload %p!\n",_vmThreadAloadFromCurrentBlock);
      }
   return _vmThreadAloadFromCurrentBlock;
   }

const char *
TR_DynamicLiteralPool::optDetailString() const throw()
   {
   return "O^O DYNAMIC LITERAL POOL: ";
   }
