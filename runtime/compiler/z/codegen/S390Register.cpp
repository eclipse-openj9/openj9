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

#include "z/codegen/S390Register.hpp"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"

TR_StorageReference::TR_StorageReference(TR::SymbolReference *t, TR::Compilation *c) : _temporary(t),
                                                                                     _node(NULL),
                                                                                     _nodeReferenceCount(0),
                                                                                     _owningRegisterCount(0),
                                                                                     _flags(0),
                                                                                     _sharedNodes(NULL),
                                                                                     _nodesToUpdateOnClobber(NULL),
                                                                                     _comp(c)
   {
   TR_ASSERT(t->isTempVariableSizeSymRef(),
      "In the TR_StorageReference constructor the temp symbol reference must be a tempVariableSizeSymRef\n");
   }

TR_StorageReference::TR_StorageReference(TR::Node *n, int32_t refCount, TR::Compilation *c) : _temporary(NULL),
                                                                                            _node(n),
                                                                                            _nodeReferenceCount(refCount),
                                                                                            _owningRegisterCount(0),
                                                                                            _flags(0),
                                                                                            _sharedNodes(NULL),
                                                                                            _nodesToUpdateOnClobber(NULL),
                                                                                            _comp(c)
   {
   TR_ASSERT(n && (n->getOpCode().isLoad() || n->getOpCode().isStore() || n->getOpCode().isLoadConst()),
      "In the TR_StorageReference constructor the node must be a load, load const, or a store\n");
   }

TR_StorageReference *
TR_StorageReference::createTemporaryBasedStorageReference(TR::SymbolReference *t, TR::Compilation *c)
   {
   return new (c->trHeapMemory()) TR_StorageReference(t,c);
   }

TR_StorageReference *
TR_StorageReference::createTemporaryBasedStorageReference(int32_t size, TR::Compilation *c)
   {
   TR::SymbolReference *temporary = c->cg()->allocateVariableSizeSymRef(size);
   return new (c->trHeapMemory()) TR_StorageReference(temporary,c);
   }

TR_StorageReference *
TR_StorageReference::createNodeBasedStorageReference(TR::Node *n, int32_t refCount, TR::Compilation *c)
   {
   return new (c->trHeapMemory()) TR_StorageReference(n,refCount,c);
   }

TR_StorageReference *
TR_StorageReference::createNodeBasedHintStorageReference(TR::Node *n, TR::Compilation *c)
   {
   TR_StorageReference *ref = new (c->trHeapMemory()) TR_StorageReference(n,0,c);
   ref->setIsNodeBasedHint();
   return ref;
   }

TR::AutomaticSymbol*
TR_StorageReference::getTemporarySymbol()
   {
   TR_ASSERT(isTemporaryBased(),"getTemporarySymbol() only valid for temporary based storage references\n");
   TR_ASSERT(getTemporarySymbolReference()->getSymbol()->isVariableSizeSymbol(),"expecting getTemporarySymbolReference() to have an variable size symbol\n");
   return getTemporarySymbolReference()->getSymbol()->getVariableSizeSymbol();
   }

rcount_t
TR_StorageReference::getNodeReferenceCount()
   {
   TR_ASSERT(isNodeBased(),"memRefNode should be non-null when querying the address reference count\n");
   TR_ASSERT(_nodeReferenceCount >= 0,"invalid _nodeReferenceCount of %d\n",_nodeReferenceCount);
   return _nodeReferenceCount;
   }

rcount_t
TR_StorageReference::setNodeReferenceCount(rcount_t count)
   {
   TR_ASSERT(isNodeBased(),"memRefNode should be non-null when querying the address reference count\n");
   return (_nodeReferenceCount=count);
   }

rcount_t
TR_StorageReference::incrementNodeReferenceCount(rcount_t increment)
   {
   TR_ASSERT(isNodeBased(),"memRefNode should be non-null when querying the address reference count\n");
   TR_ASSERT(increment >= 0,"expecting a non-negative increment (%d)\n",increment);
   _nodeReferenceCount+=increment;
   return _nodeReferenceCount;
   }

rcount_t
TR_StorageReference::decrementNodeReferenceCount(rcount_t decrement)
   {
   TR_ASSERT(isNodeBased(),"memRefNode should be non-null when querying the address reference count\n");
   TR_ASSERT(decrement >= 0,"expecting a non-negative decrement (%d)\n",decrement);
   TR_ASSERT(_nodeReferenceCount > 0,"_nodeReferenceCount will underflow to %d after decrement\n",_nodeReferenceCount-decrement);
   _nodeReferenceCount-=decrement;
   return _nodeReferenceCount;
   }


rcount_t
TR_StorageReference::getTemporaryReferenceCount()
   {
   return (isTemporaryBased() ? getTemporarySymbol()->getReferenceCount() : 0);
   }

void
TR_StorageReference::setTemporaryReferenceCount(rcount_t count)
   {
   if (isTemporaryBased())
      {
      TR::AutomaticSymbol *sym = getTemporarySymbol();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tset temporary #%d (%s) reference count %d->%d\n",
            getTemporarySymbolReference()->getReferenceNumber(),comp()->getDebug()->getName(sym),sym->getReferenceCount(),count);
      sym->setReferenceCount(count);
      }
   }

void
TR_StorageReference::decrementTemporaryReferenceCount(rcount_t decrement)
   {
   if (isTemporaryBased())
      {
      TR_ASSERT(getTemporarySymbol(),"expecting a non-null temporary symbol\n");
      TR_ASSERT(decrement >= 0,"expecting a non-negative decrement (%d)\n",decrement);
      TR::AutomaticSymbol *sym = getTemporarySymbol();
      TR_ASSERT(sym->getReferenceCount()-decrement >= 0,"decrement would cause the symbol reference count to become negative (%d)\n",sym->getReferenceCount()-decrement);
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tdecrement temporary #%d (%s) reference count %d->%d\n",
            getTemporarySymbolReference()->getReferenceNumber(),comp()->getDebug()->getName(sym),sym->getReferenceCount(),sym->getReferenceCount()-decrement);
      sym->setReferenceCount(sym->getReferenceCount()-decrement);
      if (!sym->isAddressTaken() && sym->getReferenceCount() == 0)
         {
         // reset the readOnly flag so if this storageRef gets reused (possible if it is a hint too) then it won't inherit the stale readOnly property
         if (comp()->cg()->traceBCDCodeGen() && isReadOnlyTemporary())
            traceMsg(comp(),"\treset readOnlyTemp flag on storageRef #%d (%s) (temp refCount==0 case)\n",getReferenceNumber(),comp()->getDebug()->getName(getSymbol()));
         setIsReadOnlyTemporary(false, NULL);
         comp()->cg()->pendingFreeVariableSizeSymRef(getTemporarySymbolReference());
         TR_ASSERT(getOwningRegisterCount() <= 1,"owningRegisterCount %d should be <= 1 when freeing temp #%d\n",getOwningRegisterCount(),getReferenceNumber());
         }
      }
   }

void
TR_StorageReference::incrementTemporaryReferenceCount(rcount_t increment)
   {
   if (isTemporaryBased())
      {
      TR_ASSERT(getTemporarySymbol(),"expecting a non-null temporary symbol\n");
      TR_ASSERT(increment >= 0,"expecting a non-negative increment (%d)\n",increment);
      TR::AutomaticSymbol *sym = getTemporarySymbol();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tincrement temporary #%d (%s) reference count %d->%d\n",
            getTemporarySymbolReference()->getReferenceNumber(),comp()->getDebug()->getName(sym),sym->getReferenceCount(),sym->getReferenceCount()+increment);
      sym->setReferenceCount(sym->getReferenceCount()+increment);
      if (sym->getReferenceCount() > 0)
         sym->setIsReferenced();
      }
   }

bool
TR_StorageReference::isSingleUseTemporary()
   {
   return isTemporaryBased() && getTemporarySymbol()->isSingleUse();
   }

void
TR_StorageReference::setIsSingleUseTemporary()
   {
   if (isTemporaryBased())
      {
      getTemporarySymbol()->setIsSingleUse();
      getTemporarySymbol()->setIsReferenced();
      setTemporaryReferenceCount(0);
      comp()->cg()->pendingFreeVariableSizeSymRef(getTemporarySymbolReference()); // free after this treetop has been evaluated
      }
   else
      {
      TR_ASSERT(false,"setIsSingleUseTemporary() only valid for temporary based storage references\n");
      }
   }

void
TR_StorageReference::increaseTemporarySymbolSize(int32_t sizeIncrement, TR_OpaquePseudoRegister *reg)
   {
   if (isTemporaryBased())
      {
      TR_ASSERT(sizeIncrement >= 0,"expecting the sizeIncrement (%d) to be >= 0\n",sizeIncrement);
      TR_ASSERT(getTemporarySymbol(),"expecting a non-null temporary symbol\n");

      if (sizeIncrement == 0) return;

      TR::AutomaticSymbol *sym = getTemporarySymbol();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tincreaseTemporarySymbolSize : activeSize %d->%d (on reg %s and %s)\n",
            sym->getActiveSize(),sym->getActiveSize()+sizeIncrement,comp()->cg()->getDebug()->getName(reg),comp()->getDebug()->getName(sym));
      sym->setActiveSize(sym->getActiveSize()+sizeIncrement);
      if (sym->getActiveSize() > sym->getSize())
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\tnew activeSize > symSize (%d > %d) so increment symSize %d->%d\n",sym->getActiveSize(),sym->getSize(),sym->getSize(),sym->getActiveSize());
         sym->setSize(sym->getActiveSize());
         }
      else if (comp()->cg()->traceBCDCodeGen())
         {
         traceMsg(comp(),"\t\tnew activeSize <= symSize (%d <= %d) so leave symSize at %d\n",sym->getActiveSize(),sym->getSize(),sym->getSize());
         }

      reg->clearLeftAlignedState();

      if (getNodesToUpdateOnClobber())
         {
         ListIterator<TR::Node> toUpdate(getNodesToUpdateOnClobber());
         for (TR::Node *update = toUpdate.getFirst(); update; update = toUpdate.getNext())
            {
            TR_OpaquePseudoRegister *updateReg = update->getOpaquePseudoRegister();
            if (updateReg && updateReg != reg && updateReg->getStorageReference() == reg->getStorageReference())
               {
               if (comp()->cg()->traceBCDCodeGen())
                  traceMsg(comp(),"\ty^y : clear left-aligned state of reg %s with ref #%d (%s) on node %s (%p) refCount %d from _nodesToUpdateOnClobber\n",
                           comp()->getDebug()->getName(updateReg),
                           updateReg->getStorageReference()->getReferenceNumber(), comp()->getDebug()->getName(updateReg->getStorageReference()->getSymbol()),
                           update->getOpCode().getName(), update, update->getReferenceCount());
               updateReg->clearLeftAlignedState();
               }
            }
         }
      }
   }

// liveSymbolSize (kept on the register) <= activeSymbolSize <= symbolSize
int32_t
TR_StorageReference::getSymbolSize()
   {
   int32_t size = 0;
   if (isTemporaryBased())
      {
      size = getTemporarySymbol()->getActiveSize();
      }
   else
      {
      size = getNode()->getSize();
      }

   TR_ASSERT(size > 0,"invalid TR_PseudoRegister::getSymbolSize() of %d for %s%p \n",
             size,isTemporaryBased()?"temporary sym ":"node sym ",getSymbol());

   return size;
   }

int32_t
TR_StorageReference::getReferenceNumber()
   {
   return getSymbolReference() ? getSymbolReference()->getReferenceNumber() : -1;
   }

TR::Symbol *
TR_StorageReference::getSymbol()
   {
   return getSymbolReference() ? getSymbolReference()->getSymbol() : NULL;
   }

TR::SymbolReference *
TR_StorageReference::getSymbolReference()
   {
   if (isTemporaryBased())
      return getTemporarySymbolReference();
   else if (isConstantNodeBased())
      return getNode()->getFirstChild()->getOpCode().hasSymbolReference() ? getNode()->getFirstChild()->getSymbolReference() : NULL;
   else
      return getNode()->getSymbolReference();
   }

bool
TR_StorageReference::isTemporaryBased()
   {
   if (getTemporarySymbolReference())
      {
      TR_ASSERT(getNode()==NULL,"a temporary based storage reference should have a null node\n");
      return true;
      }
   else
      {
      TR_ASSERT(getNode(),"a storage reference should be either node or temporary based\n");
      return false;
      }
   }

bool
TR_StorageReference::isNodeBased()
   {
   if (getNode())
      {
      TR_ASSERT(getTemporarySymbolReference()==NULL,"a node based storage reference should have a null temporary\n");
      return true;
      }
   else
      {
      TR_ASSERT(getTemporarySymbolReference(),"a storage reference should be either node or temporary based\n");
      return false;
      }
   }

bool
TR_StorageReference::isConstantNodeBased()
   {
   if (isNodeBased() && getNode()->getOpCode().isLoadConst())
      {
      TR_ASSERT(getTemporarySymbolReference()==NULL,"a constant node based storage reference should have a null temporary\n");
      return true;
      }
   else
      {
      return false;
      }
   }

void
TR_StorageReference::addSharedNode(TR::Node *node)
   {
   TR_ASSERT(isTemporaryBased(),"sharedNodes should only be tracked on temporary based storageRefs\n");
   if (_sharedNodes == NULL)
      _sharedNodes =  new (comp()->trHeapMemory()) List<TR::Node>(comp()->trMemory());

   _sharedNodes->add(node);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tadding node %s (%p) to _sharedNodes on hint #%d\n",node->getOpCode().getName(),node,getReferenceNumber());
   }


void
TR_StorageReference::removeSharedNode(TR::Node *node)
   {
   TR_ASSERT(isTemporaryBased(),"sharedNodes should only be tracked on temporary based storageRefs\n");
   if (_sharedNodes)
      _sharedNodes->remove(node);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tremoving node %s (%p) from _sharedNodes on hint #%d\n",node->getOpCode().getName(),node,getReferenceNumber());
   }

int32_t
TR_StorageReference::getMaxSharedNodeSize()
   {
   TR_ASSERT(isTemporaryBased(),"sharedNodes should only be tracked on temporary based storageRefs\n");
   int32_t maxSize = getSymbolSize();
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tgetMaxSharedNodeSize() for ref #%d : setting initial maxSize=symSize=%d, _sharedNodes=%p\n",
         getReferenceNumber(),maxSize,_sharedNodes);
   if (_sharedNodes)
      {
      ListIterator<TR::Node> listIt(_sharedNodes);
      for (TR::Node *listNode = listIt.getFirst(); listNode; listNode = listIt.getNext())
         {
         int32_t nodeSize = listNode->getStorageReferenceSize();
         if (nodeSize > maxSize)
            {
            if (comp()->cg()->traceBCDCodeGen())
               traceMsg(comp(),"\tupdating maxSize %d->%d from listNode %s (%p)\n",maxSize,nodeSize,listNode->getOpCode().getName(),listNode);
            maxSize = nodeSize;
            }
         }
      }
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\treturning maxSize %d from _sharedNodes on hint #%d\n",maxSize,getReferenceNumber());
   return maxSize;
   }

void TR_StorageReference::setIsReadOnlyTemporary(bool b, TR::Node *nodeToUpdateOnClobber)
   {
   if (isTemporaryBased())
      {
      _flags.set(IsReadOnlyTemporary, b);
      if (b)
         {
         TR_ASSERT(nodeToUpdateOnClobber,"must provide a node when setIsReadOnlyTemporary(true)\n");
         addNodeToUpdateOnClobber(nodeToUpdateOnClobber);
         }
      }
   }

void
TR_StorageReference::addNodeToUpdateOnClobber(TR::Node *node)
   {
   if (node->getReferenceCount() > 1 || node->getRegister() != NULL)
      {
      if (_nodesToUpdateOnClobber == NULL)
         _nodesToUpdateOnClobber =  new (comp()->trHeapMemory()) List<TR::Node>(comp()->trMemory());

      if (_nodesToUpdateOnClobber->find(node))
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\tNOT adding node %s (%p refCount %d) with reg %s to _nodesToUpdateOnClobber on ref #%d (%s) (already present in the list)\n",
               node->getOpCode().getName(),node,node->getReferenceCount(),node->getRegister()?comp()->getDebug()->getName(node->getRegister()):"NULL",getReferenceNumber(),comp()->getDebug()->getName(getSymbol()));
         }
      else
         {
         _nodesToUpdateOnClobber->add(node);
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\tadding node %s (%p refCount %d) with reg %s to _nodesToUpdateOnClobber on ref #%d (%s)\n",
               node->getOpCode().getName(),node,node->getReferenceCount(),node->getRegister()?comp()->getDebug()->getName(node->getRegister()):"NULL",getReferenceNumber(),comp()->getDebug()->getName(getSymbol()));
         }
      }
   }

void
TR_StorageReference::removeNodeToUpdateOnClobber(TR::Node *node)
   {
   if (_nodesToUpdateOnClobber)
      _nodesToUpdateOnClobber->remove(node);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tremoving node %s (%p) with reg %s from _nodesToUpdateOnClobber on ref #%d (%s)\n",
         node->getOpCode().getName(),node,comp()->getDebug()->getName(node->getRegister()),getReferenceNumber(),comp()->getDebug()->getName(getSymbol()));
   }

void
TR_StorageReference::emptyNodesToUpdateOnClobber()
   {
   if (_nodesToUpdateOnClobber)
      _nodesToUpdateOnClobber->init();
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\temptying _nodesToUpdateOnClobber list on ref #%d (%s)\n",getReferenceNumber(),comp()->getDebug()->getName(getSymbol()));
   }

bool TR_StorageReference::mayOverlapWith(TR_StorageReference *ref2)
   {
   TR_StorageReference *ref1 = this;
   if (ref1->isConstantNodeBased() || ref2->isConstantNodeBased())
      return false;

   if (ref1->isTemporaryBased() && !ref2->isTemporaryBased())
      return false;

   if (!ref1->isTemporaryBased() && ref2->isTemporaryBased())
      return false;

   if (ref1->isTemporaryBased() && ref2->isTemporaryBased())
      {
      if (ref1->getTemporarySymbolReference() == ref2->getTemporarySymbolReference())
         return true;
      else
         return false;
      }

   if (ref1->isNonConstantNodeBased() && ref2->isNonConstantNodeBased() &&
       ref1->getNode()->getOpCode().hasSymbolReference() &&
       ref2->getNode()->getOpCode().hasSymbolReference())
      {
      TR::Node *ref1Node = ref1->getNode();
      TR::Node *ref2Node = ref2->getNode();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tmayOverlapWith storageRef : check overlap between ref1Node %s (%p) and ref2Node %s (%p)\n",
            ref1Node->getOpCode().getName(),ref1Node,ref2Node->getOpCode().getName(),ref2Node);

      bool mayOverlap = comp()->cg()->loadAndStoreMayOverlap(ref1Node,
                                                             ref1Node->getSize(),
                                                             ref2Node,
                                                             ref2Node->getSize());
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\toverlap=true (from %s storageRef test)\n",mayOverlap?"true":"false","aliasing");

      return mayOverlap;
      }

   return true;
   }





//
// TR_OpaquePseudoRegister routines
//
void TR_OpaquePseudoRegister::setStorageReference(TR_StorageReference *ref, TR::Node *node)
   {
   if (comp()->cg()->traceBCDCodeGen())
      {
      traceMsg(comp(),"\tsetStorageReference to ref #%d (%s isTemp %d isHint %d) for node %s (%p) and reg %s.\n",
         ref->getReferenceNumber(),
         comp()->getDebug()->getName(ref->getSymbol()),
         ref->isTemporaryBased(),
         ref->isNodeBasedHint(),
         node?node->getOpCode().getName():"NULL",
         node,
         comp()->cg()->getDebug()->getName(this));

     traceMsg(comp(),"\t\texisting _storageReference is #%d (refNode=%p isTemp %d, isHint %d)\n",
         _storageReference?_storageReference->getReferenceNumber():0,
         _storageReference?_storageReference->getNode():0,
         _storageReference?_storageReference->isTemporaryBased():0,
         _storageReference?_storageReference->isNodeBasedHint():0);
      }

   if (_storageReference && (ref != _storageReference))
      {
      if (_storageReference->isTemporaryBased())
         {
         _storageReference->decrementTemporaryReferenceCount(node->getReferenceCount());
         }
      else if (!_storageReference->isNodeBasedHint())
         {
         TR::Node *storageRefNode = _storageReference->getNode();

         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\tdecrement storageRef #%d nodeRefCount by (node->refCount() - 1) = %d : %d->%d\n",
               _storageReference->getReferenceNumber(),
               node->getReferenceCount()-1,
               _storageReference->getNodeReferenceCount(),
               _storageReference->getNodeReferenceCount()-(node->getReferenceCount()-1));

         TR_ASSERT(storageRefNode->getOpCode().isLoadConst() || storageRefNode->getOpCode().isLoadVar() || storageRefNode->getOpCode().isStore(),
            "storageReference node %p must be a load/store op\n",storageRefNode);

         // dec by node->getReferenceCount()-1 to undo the larger increment possibly done in evaluateSignModifyingOperand for passThrough operations
         _storageReference->decrementNodeReferenceCount(node->getReferenceCount()-1);
         if (storageRefNode->getOpCode().isLoadConst() || storageRefNode->getOpCode().isIndirect())
            {
            if (comp()->cg()->traceBCDCodeGen())
               traceMsg(comp(),"\t\t_storageReference is non-hint nodeBased with nodeRefCount %d and addrChild %p\n",
                  _storageReference->getNodeReferenceCount(),storageRefNode->getFirstChild());
            // Do a recursive decrement of the addrChild in two cases:
            // 1) If storageRefNode == node then we must always recDec as all future references to the node will use the new storageRef being set here
            // 2) If storageRefNode != node then this means we are removing a never initialized storageRef on a passThru node so in this case only recDec if
            //    this is the last use (nodeRefCount == 0) as other nodes below this passThru node may still require the addrChild
            bool doRecursiveDecrement = (storageRefNode == node) || (_storageReference->getNodeReferenceCount() == 0);

            if (comp()->cg()->traceBCDCodeGen())
               traceMsg(comp(),"\t\t\tdoRecursiveDecrement=%s on addrChild %p (refCount=%d), addrChild->firstChild %p (refCount %d) if storageRefNode %p == node %p (%s) or nodeRefCount %d == 0 (%s)\n",
                  doRecursiveDecrement?"yes":"no",
                  storageRefNode->getFirstChild(),
                  storageRefNode->getFirstChild()->getReferenceCount(),
                  storageRefNode->getFirstChild()->getNumChildren() > 0 ? storageRefNode->getFirstChild()->getFirstChild() : NULL,
                  storageRefNode->getFirstChild()->getNumChildren() > 0 ? storageRefNode->getFirstChild()->getFirstChild()->getReferenceCount() : -1,
                  storageRefNode,
                  node,
                  storageRefNode == node ? "yes":"no",
                  _storageReference->getNodeReferenceCount(),
                  _storageReference->getNodeReferenceCount() == 0?"yes":"no");

            if (doRecursiveDecrement)
               comp()->cg()->recursivelyDecReferenceCount(storageRefNode->getFirstChild());
            }
         }
      // If the storage reference is being changed then any storage reference dependent state must be reset
      clearLeftAndRightAlignedState();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tsetting the new storageRef #%d (over existing storageRef #%d) on reg %s so reset leftAlignedZeroDigits and deadAndIgnoredBytes to 0\n",
            ref->getReferenceNumber(),_storageReference->getReferenceNumber(),comp()->cg()->getDebug()->getName(this));
      _storageReference->decOwningRegisterCount();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tdecrement owningRegisterCount %d->%d on _storageReference #%d (%s) as new ref is being set\n",
            _storageReference->getOwningRegisterCount()+1,
            _storageReference->getOwningRegisterCount(),
            _storageReference->getReferenceNumber(),
            comp()->getDebug()->getName(_storageReference->getSymbol()));
      }

   if (node && // node may be NULL if calling to simply set the _storageReference = ref so state can be pulled off of the register
       ref &&
       (ref != _storageReference))  // allow multiple calls to setStorageReference but only increment the first time for a particular ref
      {
      if (ref->isTemporaryBased())
         {
         ref->incrementTemporaryReferenceCount(node->getReferenceCount());
         //    pdstore
         // 3     pdcleanB  reg3 t1
         // 1        pdModPrec reg2 t1
         // 2           pdCleanA reg1
         // The pdcleanB and pdModPrec are sharing the read-only storageRef t1 (along with other nodes possibly, like pdcleanA)
         // and reg3->setStorageReference(t1,pdcleanB) has just been called.
         // Must add pdcleanB to the update list as the pdcleanB consumer (a store or a call maybe) may not be calling ssrClobberEvaluate themselves (as this consumer will not
         // be clobbering the value).
         // However some other node, such as pdcleanA, may need to be clobber evaluated and at this point reg3 must be updated with the copy storageRef.
         }
      ref->addNodeToUpdateOnClobber(node);
      ref->incOwningRegisterCount();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tincrement owningRegisterCount %d->%d on ref #%d (%s) for reg %s and node %s (%p) refCount %d\n",
            ref->getOwningRegisterCount()-1,
            ref->getOwningRegisterCount(),
            ref->getReferenceNumber(),
            comp()->getDebug()->getName(ref->getSymbol()),
            comp()->cg()->getDebug()->getName(this),
            node->getOpCode().getName(),
            node,
            node->getReferenceCount());
      }

   if (node &&
       ref &&
       node->getOpCode().canHaveStorageReferenceHint() &&
       node->getStorageReferenceHint() &&
       node->getStorageReferenceHint() == ref)
      {
      ref->setHintHasBeenUsed();
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tsetting hintHasBeenUsed = true on new storageRef #%d\n",ref->getReferenceNumber());
      }

   _storageReference = ref;
   }

int32_t TR_OpaquePseudoRegister::getSymbolSize()
   {
   TR_ASSERT(getStorageReference(),"getStorageReference() must be non-null when querying getSymbolSize()\n");
   return getStorageReference()->getSymbolSize();
   }

void TR_OpaquePseudoRegister::increaseTemporarySymbolSize(int32_t sizeIncrement)
   {
   getStorageReference()->increaseTemporarySymbolSize(sizeIncrement, this);
   }

int32_t TR_OpaquePseudoRegister::getLiveSymbolSize()
   {
   TR_ASSERT(getRightAlignedDeadBytes() >= 0,"getRightAlignedDeadBytes() should be non-negative instead of %d\n",getRightAlignedDeadBytes());
   TR_ASSERT(getRightAlignedIgnoredBytes() >= 0,"getRightAlignedIgnoredBytes() should be non-negative instead of %d\n",getRightAlignedIgnoredBytes());
   return getSymbolSize() - getRightAlignedDeadAndIgnoredBytes();
   }

int32_t TR_OpaquePseudoRegister::getValidSymbolSize()
   {
   TR_ASSERT(getRightAlignedDeadBytes() >= 0,"getRightAlignedDeadBytes() should be non-negative instead of %d\n",getRightAlignedDeadBytes());
   return getSymbolSize() - getRightAlignedDeadBytes();
   }

//
// TR_PseudoRegister routines
//

int32_t TR_PseudoRegister::getDecimalPrecision()
   {
   return _decimalPrecision;
   }

int32_t TR_PseudoRegister::setDecimalPrecision(uint16_t precision)
   {
   _decimalPrecision = precision;
   _size = TR::DataType::getSizeFromBCDPrecision(getDataType(), _decimalPrecision);
   return _decimalPrecision;
   }

int32_t TR_PseudoRegister::getSize()
   {
   return _size;
   }

int32_t TR_PseudoRegister::setSize(int32_t size)
   {
   _size = size;
   _decimalPrecision = TR::DataType::getBCDPrecisionFromSize(getDataType(), _size);
   return _size;
   }

int32_t TR_PseudoRegister::getSymbolDigits()
   {
   return TR::DataType::getBCDPrecisionFromSize(getDataType(), getStorageReference()->getSymbolSize());
   }


// The input startDigit->endDigit range is a right to left range where the sign code (not considered a digit) has the index -1, and the first digit has index 0,
// and the second digit has index 1 and so on...
// pd number 06 66 6c   number is +06666
// indices  43 21 0x   with x=-1
// Note that this range is not inclusive. For example if startDigit=1,endDigit=3 then the first index included in the range
// is the startDigit value of 1 and the last number included in the range is endDigit-1=2 and the range size is 2 (endDigit-startDigit)
// This input scheme is used so the node->getDecimalPrecision() and node->getSize() values can be used without change in most cases.
// From the above startDigit and endDigit values and assuming that the number of digits in the register's symbol is 5 then the left to right range values
// used below are:
//    rangeStart=+2
//    rangeEnd=+4
//
// NOTE: this range tracking is currently only valid for the TR::PackedDecimal and TR::ZonedDecimal types (see assumes in the _leftAlignedZeroDigits getter/setter)
int32_t TR_PseudoRegister::getRangeStart(int32_t startDigit, int32_t endDigit)
   {
   TR_ASSERT(getKind() == TR_SSR, "only valid for initialized TR_SSR registers\n");
   TR_ASSERT(endDigit >= startDigit,"invalid range for getRangeStart\n");
   TR_ASSERT(startDigit>=-1,"invalid startDigit %d for getRangeStart\n",startDigit); // -1 is the sign code, 0 is the first digit, 1 is the second digit...

   int32_t symDigits = getSymbolDigits();

   TR_ASSERT(endDigit<=symDigits,"endDigit > symDigits (%d > %d) for addRangeOfZeroDigits\n",endDigit,symDigits); // ...symDigits-1 is the last digit
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tgetRangeStart %s: startDigit %d, endDigit %d, symSize %d, symDigits %d\n",
         comp()->cg()->getDebug()->getName(this),startDigit,endDigit,getStorageReference()->getSymbolSize(),symDigits);
   TR_StorageReference *storageReference = getStorageReference();
   TR_ASSERT(storageReference,"storageReference should be non-null at this point\n");
   int32_t deadAndIgnoredBytes = getRightAlignedDeadAndIgnoredBytes();
   if (deadAndIgnoredBytes)
      {
      TR_ASSERT(deadAndIgnoredBytes > 0, "deadAndIgnoredBytes should be > 0 and not %d\n",deadAndIgnoredBytes);
      int32_t digitOffset = TR::DataType::bytesToDigits(getDataType(), deadAndIgnoredBytes);
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tdeadAndIgnoredBytes = %d (digitOffset = %d) so inc startDigit %d -> %d and endDigit %d -> %d\n",
            deadAndIgnoredBytes,digitOffset,startDigit,startDigit+digitOffset,endDigit,endDigit+digitOffset);
      startDigit+=digitOffset;
      endDigit+=digitOffset;
      }

   TR_ASSERT(startDigit>=-1,"startDigit < -1 (%d) for addRangeOfZeroDigits\n",startDigit); // -1 is the sign code, 0 is the first digit, 1 is the second digit...
   TR_ASSERT(endDigit<=symDigits,"endDigit > symDigits (%d > %d) for addRangeOfZeroDigits\n",endDigit,symDigits); // -1 is the sign code, 0 is the first digit, 1 is the second digit...

   int32_t rangeStart = symDigits - endDigit;
   TR_ASSERT(rangeStart >= 0,"symbol size is smaller than requested range\n");
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\treturning rangeStart %d\n",rangeStart);

   return rangeStart;
   }

int32_t TR_PseudoRegister::getRangeEnd(int32_t rangeStart, int32_t startDigit, int32_t endDigit)
   {
   TR_ASSERT(getKind() == TR_SSR, "only valid for initialized TR_SSR registers\n");
   TR_ASSERT(endDigit >= startDigit,"invalid range for getRangeStart\n");
   TR_ASSERT(startDigit>=-1,"invalid startDigit %d for addRangeOfZeroDigits\n",startDigit); // -1 is the sign code, 0 is the first digit, 1 is the second digit...
   int32_t rangeSize = endDigit-startDigit;
   int32_t rangeEnd = rangeStart+rangeSize;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\tgetRangeEnd %s returning %d\n",comp()->cg()->getDebug()->getName(this),rangeEnd);
   TR_ASSERT(rangeEnd<=getSymbolDigits()+1,"invalid rangeEnd %d\n",rangeEnd);   // +1 for the sign 'digit' because a rangeEnd can include the sign
   return rangeEnd;
   }

int32_t TR_PseudoRegister::getDigitsToClear(int32_t startDigit, int32_t endDigit)
   {
   TR_ASSERT(getKind() == TR_SSR,"only valid for TR_SSR registers\n");
   if (!trackZeroDigits()) return endDigit-startDigit;
   if (startDigit == endDigit) return 0;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tgetDigitsToClear %s (%s): %d -> %d\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startDigit,endDigit);
   if (getLiveSymbolSize() < TR::DataType::getSizeFromBCDPrecision(getDataType(), endDigit))
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tliveSymSize %d < endByte %d so return a conservative digitsToClear of %d\n",
            getLiveSymbolSize(),TR::DataType::getSizeFromBCDPrecision(getDataType(), endDigit),endDigit-startDigit);
      return endDigit-startDigit;
      }
   int32_t rangeStart = getRangeStart(startDigit, endDigit);
   int32_t rangeEnd = getRangeEnd(rangeStart, startDigit, endDigit);
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   int32_t rangeSize = endDigit-startDigit;
   int32_t digitsToClear = 0;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\trangeStart %d, rangeEnd %d, leftAlignedZeroDigits = %d\n",rangeStart,rangeEnd,leftAlignedZeroDigits);
   if (rangeEnd > leftAlignedZeroDigits) // does the endDigit of range extend beyond the leftmost cleared digits (if so, then some digits will have to be cleared)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting digitsToClear to %d (rangeSize) because rangeEnd %d > leftAlignedZeroDigits %d\n",
            rangeSize,rangeEnd,leftAlignedZeroDigits);
      digitsToClear = rangeSize;              // the max digits to clear, may be adjusted on an overlap
      if (rangeStart < leftAlignedZeroDigits) // is the startDigit of range is within the cleared range (i.e. an overlap)
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\tadjusting digitsToClear %d -> %d due to an overlap (rangeStart %d < leftAlignedZeroDigits %d)\n",
               digitsToClear,digitsToClear-(leftAlignedZeroDigits-rangeStart),rangeStart,leftAlignedZeroDigits);
         digitsToClear -= (leftAlignedZeroDigits-rangeStart);
         }
      else if (comp()->cg()->traceBCDCodeGen())
         {
         traceMsg(comp(),"\t\tnot adjusting digitsToClear (remains at rangeSize = %d) as there is no overlap (rangeStart %d  >= leftAlignedZeroDigits %d)\n",
            digitsToClear,rangeStart,leftAlignedZeroDigits);
         }
      }
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\treturning digitsToClear %d\n",digitsToClear);
   return digitsToClear;
   }

int32_t TR_PseudoRegister::getBytesToClear(int32_t startByte, int32_t endByte) // input range of bytes is right to left : startByte->endByte-1
   {
   if (startByte == endByte) return 0;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tgetBytesToClear %s (%s): (startByte=%d, endByte=%d): defer to getDigitsToClear\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startByte,endByte);
   int32_t startDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), startByte);
   int32_t endDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), endByte);
   int32_t digitsToClear = getDigitsToClear(startDigit, endDigit);

   if ((digitsToClear&0x1) != 0 &&  // if odd
       TR::DataType::getDigitSize(getDataType()) == HalfByteDigit)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\tincrement digitsToClear %d->%d for halfByteType %s\n",digitsToClear,digitsToClear+1,TR::DataType::getName(getDataType()));
      digitsToClear++;  // round up to the next byte to be conservative for half byte digit types
      }
   int32_t bytesToClear = TR::DataType::digitsToBytes(getDataType(), digitsToClear);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\treturning bytesToClear %d\n",bytesToClear);
   return bytesToClear;
   }

int32_t TR_PseudoRegister::getByteOffsetFromLeftForClear(int32_t startDigit, int32_t endDigit, int32_t &digitsToClear, int32_t resultSize)
   {
   TR_ASSERT(getKind() == TR_SSR,"only valid for TR_SSR registers\n");
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tgetByteOffsetFromLeftForClear %s (%s): %d -> %d, digitsToClear %d, resultSize %d\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startDigit,endDigit,digitsToClear,resultSize);
   int32_t rangeStart = getRangeStart(startDigit, endDigit);
   int32_t rangeEnd = getRangeEnd(rangeStart, startDigit, endDigit);
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   TR_ASSERT(leftAlignedZeroDigits < rangeEnd,"there are no digits to clear so calling getByteOffsetFromLeftForClear does not make sense\n");
   int32_t digitOffset = 0;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\trangeStart %d, rangeEnd %d, leftAlignedZeroDigits = %d\n",rangeStart,rangeEnd,leftAlignedZeroDigits);
   if (leftAlignedZeroDigits > rangeStart)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting digitOffset to %d (== leftAlignedZeroDigits) as leftAlignedZeroDigits %d > rangeStart %d (an overlap)\n",leftAlignedZeroDigits,leftAlignedZeroDigits,rangeStart);
      digitOffset = leftAlignedZeroDigits;   // cleared digits overlaps range so only clear from leftAlignedZeroDigits
      }
   else
      {
      // the offset returned by this routine is the offset *within* any left aligned zero digits. In this case the rangeStart->rangeEnd is disjoint from these
      // left aligned zero digits so offset=0 is returned
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting digitOffset to 0 as leftAlignedZeroDigits %d <= rangeStart %d (disjoint)\n",leftAlignedZeroDigits,rangeStart);
      digitOffset = 0;
      }

   if ((digitOffset&0x1) != 0 &&
       TR::DataType::getDigitSize(getDataType()) == HalfByteDigit)
      {
      // This routine is used to get a memory reference offset for use in a generated instruction so full byte addressability is required.
      // Therefore if the returned offset is odd (in the middle of a byte) then it is conservatively decremented to the previous full byte.
      // In addition, so all the original digits still get cleared the digitsToClear amount must be incremented by one to account for the offset decrement.
      // An example of the complicated (and suboptimal) case being avoided is a even digit (say 4) clearing at an odd offset (say 1) that would require a half byte clearing at both
      // ends, on s390 NI +0,XC +1(1),NI +2 instead of XC +0(2),NI +2 which is accomplished if the offset-- (1->0) and digitsToClear++ (4->5).
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tdigitOffset is odd so decrement digitOffset %d -> %d and increment digitsToClear %d -> %d\n",digitOffset,digitOffset-1,digitsToClear,digitsToClear+1);
      digitOffset--;
      digitsToClear++;
      }

   int32_t liveSymbolSize = getLiveSymbolSize();
   TR_ASSERT(liveSymbolSize >= resultSize,"liveSymbolSize should be >= resultSize\n");
   if (digitOffset && (liveSymbolSize > resultSize))
      {
      // if the current resultSize does not fill the entire symbol size then part of the offset will be done when right aligning the memory reference during later code generation
      // so in this case reduce the offset returned by this function so the final offset isn't too great (see pdshr.c misc7a)
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tdecrease digitOffset %d -> %d because liveSymbolSize > resultSize (%d > %d), liveSymbolSize is getSymbolSize() %d - deadAndIgnoredBytes %d\n",
                     digitOffset,digitOffset-TR::DataType::bytesToDigits(getDataType(),liveSymbolSize-resultSize),liveSymbolSize,resultSize,getStorageReference()->getSymbolSize(),getRightAlignedDeadAndIgnoredBytes());
      digitOffset-=TR::DataType::bytesToDigits(getDataType(),liveSymbolSize-resultSize);
      }
   TR_ASSERT(digitOffset >= 0,"digitOffset %d should not be negative\n",digitOffset);
   int32_t byteOffset = TR::DataType::digitsToBytes(getDataType(), digitOffset);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\treturning byte offset %d (from digitOffset %d and type %s)\n",byteOffset,digitOffset,TR::DataType::getName(getDataType()));
   return byteOffset;
   }

void TR_PseudoRegister::addRangeOfZeroDigits(int32_t startDigit, int32_t endDigit)
   {
   TR_ASSERT(getKind() == TR_SSR,"only valid for TR_SSR registers\n");
   if (startDigit == endDigit) return;
   if (!trackZeroDigits()) return;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\taddRangeOfZeroDigits %s (%s): %d -> %d\n",comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startDigit,endDigit);
   int32_t rangeStart = getRangeStart(startDigit, endDigit);
   int32_t rangeEnd = getRangeEnd(rangeStart, startDigit, endDigit);
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\trangeStart %d, rangeEnd %d, leftAlignedZeroDigits = %d\n",rangeStart,rangeEnd,leftAlignedZeroDigits);
   // if range to be added is adjacent to (==) or overlaps (<) then the leftAlignedZeroDigits can be incremented
   if (rangeStart <= leftAlignedZeroDigits && rangeEnd > leftAlignedZeroDigits)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting leftAlignedZeroDigits to %d (leftAlignedZeroDigits %d + (rangeEnd %d - leftAlignedZeroDigits %d) because new range overlaps or is adjacent to current zero range\n",
            leftAlignedZeroDigits+(rangeEnd-leftAlignedZeroDigits),leftAlignedZeroDigits,rangeEnd,leftAlignedZeroDigits);
      setLeftAlignedZeroDigits(leftAlignedZeroDigits+(rangeEnd-leftAlignedZeroDigits));
      }
   else if (comp()->cg()->traceBCDCodeGen())
      {
      traceMsg(comp(),"\t\tnot setting leftAlignedZeroDigits because new range is not adjacent to or overlapping with the current zero range (rangeStart %d > leftAlignedZeroDigits %d)\n",
                        rangeStart,leftAlignedZeroDigits);
      }
   }

void TR_PseudoRegister::addRangeOfZeroBytes(int32_t startByte, int32_t endByte) // input range of bytes is right to left : startByte->endByte-1
   {
   if (startByte == endByte) return;
   if (!trackZeroDigits()) return;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\taddRangeOfZeroBytes %s (%s): (startByte=%d, endByte=%d): defer to addRangeOfZeroDigits\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startByte,endByte);
   int32_t startDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), startByte);
   int32_t endDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), endByte);
   addRangeOfZeroDigits(startDigit, endDigit);
   }

void TR_PseudoRegister::removeRangeOfZeroDigits(int32_t startDigit, int32_t endDigit)
   {
   if (startDigit == endDigit) return;
   if (!trackZeroDigits()) return;
   TR_ASSERT(getKind() == TR_SSR,"only valid for TR_SSR registers\n");
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tremoveRangeOfZeroDigits %s (%s): %d -> %d%s\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startDigit,endDigit,(leftAlignedZeroDigits==0)?" (zeroDigits==0 -- nothing to remove)":"");
   if (leftAlignedZeroDigits == 0)
      return;
   int32_t rangeStart = getRangeStart(startDigit, endDigit);
   int32_t rangeEnd = getRangeEnd(rangeStart, startDigit, endDigit);
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\trangeStart %d, rangeEnd %d, leftAlignedZeroDigits = %d\n",rangeStart,rangeEnd,leftAlignedZeroDigits);

   if (rangeStart < leftAlignedZeroDigits)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting leftAlignedZeroDigits to %d (leftAlignedZeroDigits %d - rangeStart %d) because rangeStart < leftAlignedZeroDigits\n",
            leftAlignedZeroDigits - (leftAlignedZeroDigits-rangeStart),leftAlignedZeroDigits,rangeStart);
      setLeftAlignedZeroDigits(leftAlignedZeroDigits - (leftAlignedZeroDigits-rangeStart));
      }
   else if (comp()->cg()->traceBCDCodeGen())
      {
      traceMsg(comp(),"\t\tnot setting leftAlignedZeroDigits because rangeStart %d >= leftAlignedZeroDigits %d\n",
         rangeStart,leftAlignedZeroDigits);
      }

   TR_ASSERT(getLeftAlignedZeroDigits() >= 0 && getLeftAlignedZeroDigits() <= getSymbolDigits(),"invalid getLeftAlignedZeroDigits value (%d)\n",getLeftAlignedZeroDigits());
   }

void TR_PseudoRegister::removeRangeOfZeroBytes(int32_t startByte, int32_t endByte) // input range of bytes is right to left : startByte->endByte-1
   {
   if (!trackZeroDigits()) return;
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tremoveRangeOfZeroBytes %s (%s): (startByte=%d, endByte=%d)%s\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),startByte,endByte,(leftAlignedZeroDigits==0)?" (zeroDigits==0 -- nothing to remove)":": defer to removeRangeOfZeroDigits");
   if (leftAlignedZeroDigits == 0)
      return;
   // an input byte of zero gives an invalid startDigit of -1 for packed types so catch this here
   int32_t startDigit = (startByte == 0) ? 0 : TR::DataType::getBCDPrecisionFromSize(getDataType(), startByte);
   int32_t endDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), endByte);
   removeRangeOfZeroDigits(startDigit, endDigit);
   }

void TR_PseudoRegister::removeByteRangeAfterLeftShift(int32_t operandByteSize, int32_t shiftDigitAmount)
   {
   if (!trackZeroDigits()) return;
   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\tremoveByteRangeAfterLeftShift %s (%s): (operandByteSize=%d, shiftDigitAmount=%d)\n",
         comp()->cg()->getDebug()->getName(this),TR::DataType::getName(getDataType()),operandByteSize,shiftDigitAmount);
   int32_t startDigit = 0;
   int32_t endDigit = TR::DataType::getBCDPrecisionFromSize(getDataType(), operandByteSize);
   int32_t rangeStart = getRangeStart(startDigit, endDigit);
   int32_t rangeEnd = getRangeEnd(rangeStart, startDigit, endDigit);
   int32_t leftAlignedZeroDigits = getLeftAlignedZeroDigits();
   if (rangeStart <= leftAlignedZeroDigits)
      {
      int32_t newLeftAlignedZeroDigits = std::max(leftAlignedZeroDigits - shiftDigitAmount,rangeStart); // a left shift can never remove zero digits before its range start
      int32_t oldLeftAlignedZeroDigits = leftAlignedZeroDigits - shiftDigitAmount;
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\tsetting leftAlignedZeroDigits to %d = std::max(leftAlignedZeroDigits %d - shiftDigitAmount %d, rangeStart %d) because rangeStart %d <= leftAlignedZeroDigits %d\n",
            newLeftAlignedZeroDigits,leftAlignedZeroDigits,shiftDigitAmount,rangeStart,rangeStart,leftAlignedZeroDigits);
      TR_ASSERT(newLeftAlignedZeroDigits >=0,"newLeftAlignedZeroDigits should always be >=0 and not %d\n",newLeftAlignedZeroDigits);
      setLeftAlignedZeroDigits(newLeftAlignedZeroDigits);
      }
   else if (comp()->cg()->traceBCDCodeGen())
      {
      traceMsg(comp(),"\t\tnot setting leftAlignedZeroDigits because rangeStart %d > leftAlignedZeroDigits %d\n",rangeStart,leftAlignedZeroDigits);
      }
   }

bool TR_PseudoRegister::isLeftMostNibbleClear()
   {
   TR_ASSERT(isEvenPrecision(), "isLeftMostNibbleClear only valid for even precision registers (prec=%d)\n",getDecimalPrecision());
   if (getDigitsToClear(getDecimalPrecision(), getDecimalPrecision()+1) == 0)
      return true;
   else
      return false;
   }

void TR_PseudoRegister::setLeftMostNibbleClear()
   {
   addRangeOfZeroDigits(getDecimalPrecision(), getDecimalPrecision()+1);
   }

bool
TR_PseudoRegister::hasKnownOrAssumedPositiveSignCode() // plus or unsigned
   {
   return hasKnownOrAssumedSignCode() &&
          TR::DataType::rawSignIsPositive(getDataType(), getKnownOrAssumedSignCode());
   }

bool
TR_PseudoRegister::hasKnownOrAssumedNegativeSignCode()
   {
   return hasKnownOrAssumedSignCode() &&
          TR::DataType::rawSignIsNegative(getDataType(), getKnownOrAssumedSignCode());
   }

// The extra work to allow this for non-temp based is to expand the skipCopyOnStore check to all nodes (i.e. do not restrict this flag to those directly under a store node).
// This skipCopyOnStore analysis will then guarantee that the underlying non-temp variable is not killed before its next use(s).
bool TR_PseudoRegister::canBeConservativelyClobberedBy(TR::Node *clobberingNode)
   {
   TR_StorageReference *storageRef = getStorageReference();
   if (storageRef &&
       storageRef->isTemporaryBased())
      {
      if (clobberingNode->getOpCode().isSimpleBCDClean() &&
          isLegalToCleanSign())
         {
         // e.g. a ZAP (by clobberingNode) of a register (this) that has no particular sign requirements is conservatively correct
         return true;
         }
      // another case that can be added is a xSetSign to 0xF where the child already has a positive sign -- watch out for truncating side-effects here
      else
         {
         return false;
         }
      }
   else
      {
      return false;
      }
   }

TR::Register *
TR_OpaquePseudoRegister::getRegister()
   {
   return NULL;
   }

TR_OpaquePseudoRegister *
TR_OpaquePseudoRegister::getOpaquePseudoRegister()
   {
   return this;
   }

TR_PseudoRegister *
TR_OpaquePseudoRegister::getPseudoRegister()
   {
   return NULL;
   }

TR::Register *
TR_PseudoRegister::getRegister()
   {
   return NULL;
   }

TR_OpaquePseudoRegister *
TR_PseudoRegister::getOpaquePseudoRegister()
   {
   return this;
   }

TR_PseudoRegister *
TR_PseudoRegister::getPseudoRegister()
   {
   return this;
   }

