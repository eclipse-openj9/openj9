/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/Stack.hpp"
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "ilgen/ByteCodeIteratorWithState.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "compiler/il/OMRTreeTop_inlines.hpp"

class TR_J9ByteCodeIteratorWithState :
   public TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, TR::Node *>
   {
   typedef TR_ByteCodeIteratorWithState<TR_J9ByteCode, J9BCunknown, TR_J9ByteCodeIterator, TR::Node *> Base;

   public:
   TR_J9ByteCodeIteratorWithState(TR::ResolvedMethodSymbol * methodSymbol, TR_J9VMBase * fe, TR::Compilation * comp)
      : Base(methodSymbol, comp),
        _replacedSymRefs(NULL)
      {
      TR_J9ByteCodeIterator::initialize(static_cast<TR_ResolvedJ9Method *>(methodSymbol->getResolvedMethod()), fe);
      }

protected:

   void initialize()
      {
      Base::initialize();
      }

   void markAnySpecialBranchTargets(TR_J9ByteCode bc)
      {
      int32_t i = bcIndex();
      if (bc == J9BCtableswitch)
         {
         int32_t index = defaultTargetIndex();
         markTarget(i, nextSwitchValue(index));
         int32_t low = nextSwitchValue(index);
         int32_t high = nextSwitchValue(index) - low + 1;
         for (int32_t j = 0; j < high; ++j)
            markTarget(i, nextSwitchValue(index));
         }
      else if (bc == J9BClookupswitch)
         {
         int32_t index = defaultTargetIndex();
         markTarget(i, nextSwitchValue(index));
         int32_t tableSize = nextSwitchValue(index);
         for (int32_t j = 0; j < tableSize; ++j)
            {
            index += 4; // match value
            markTarget(i, nextSwitchValue(index));
            }
         }
      }

   void findAndMarkExceptionRanges()
      {
      int32_t handlerIndex=0;
      for (;handlerIndex < method()->numberOfExceptionHandlers(); handlerIndex++)
         {
         int32_t start, end, type;
         int32_t handler = method()->exceptionData(handlerIndex, &start, &end, &type);
         markExceptionRange(handlerIndex, start, end, handler, type);
         }

      if (handlerIndex > 0)
         _methodSymbol->setEHAware();
      }

   virtual void saveStack(int32_t) = 0;

   void patchPartialInliningCallBack(int32_t slot, TR::SymbolReference *symRef, TR::SymbolReference *tempRef, TR::TreeTop *tt2) //TR::Block *restartBlock)
      {
      TR::TreeTop *tt = tt2->getNextRealTreeTop();     //restartBlock->getEntry();
      TR::Node *ttnode = tt->getNode();
      TR::Node *callNode = ttnode->getFirstChild();
      TR::Node *child = NULL;

      for(int32_t i = 0 ; i < callNode->getNumChildren() ; i++)
         {
         child = callNode->getChild(i);
         if(child->getSymbolReference() == symRef)
            {
            child->setSymbolReference(tempRef);
            }

         }
      }
   void addTempForPartialInliningCallBack(int32_t slot,TR::SymbolReference *tempRef, int32_t numParms)
      {
      if(_replacedSymRefs == 0)
         _replacedSymRefs = new (_compilation->trStackMemory()) TR_Array<TR::SymbolReference*>(_compilation->trMemory(), numParms , true, stackAlloc);

      (*_replacedSymRefs)[slot] = tempRef;
      }

   TR::TreeTop *genPartialInliningCallBack(int32_t index, TR::TreeTop *callTreeTop)
      {
      genBBStart(index);

      TR::Node *ttNode = TR::Node::create(TR::treetop, 1);
      TR::Node *callNode =TR::Node::copy(callTreeTop->getNode()->getFirstChild());
      callNode->setReferenceCount(1);
      //TR_ASSERT(callNode->getOpCode().isCall(), "node %p isn't a call!\n",callTreeTop->getNode()->getFirstChild());
      ttNode->setFirst(callNode);       //copying callnode. will replace children shortly


      ListIterator<TR::ParameterSymbol> li (&_methodSymbol->getParameterList());
      TR::ParameterSymbol *si = NULL;
      int32_t i;
      for(si = li.getFirst(),  i=0 ; si != NULL ; si = li.getNext(), i++ )
         {
         TR::SymbolReference *ref = _compilation->getSymRefTab()->findOrCreateAutoSymbol(_methodSymbol,si->getSlot(),si->getDataType());
         if(_replacedSymRefs && (*_replacedSymRefs)[si->getSlot()])
            {
            ref = (*_replacedSymRefs)[si->getSlot()];
            }

         if(callNode->getOpCode().isIndirect() && i==0)  // need to generate vft offset & call
            {
            TR::Node *thisPointer =  TR::Node::createWithSymRef(_compilation->il.opCodeForDirectLoad(si->getDataType()),0,ref);
            callNode->setAndIncChild(0, TR::Node::createWithSymRef(TR::aloadi, 1, 1, thisPointer, _compilation->getSymRefTab()->findOrCreateVftSymbolRef()));
            i++;
            }
         TR::Node *child = TR::Node::createWithSymRef(_compilation->il.opCodeForDirectLoad(si->getDataType()),0,ref);
         callNode->setAndIncChild(i,child);                //setChild doesn't modify the ref count.  setAndIncChild() will properly inc the ref count.
         }
      blocks(index)->append(TR::TreeTop::create(_compilation, ttNode));
      TR::Node *retNode = NULL;
      if(TR::ILOpCode::returnOpCode(callNode->getDataType())  == TR::Return)  //null return?
         {
         retNode = TR::Node::create(TR::ILOpCode::returnOpCode(callNode->getDataType()),0);
         }
      else
         {
         retNode = TR::Node::create(TR::ILOpCode::returnOpCode(callNode->getDataType()),1,callNode);
         }
      blocks(index)->append(TR::TreeTop::create(_compilation,retNode));
      setIsGenerated(index);
      return blocks(index)->getEntry();
      }

   TR::TreeTop *genGotoPartialInliningCallBack(int32_t index, TR::TreeTop *gotoTreeTop)
      {
      genBBStart(index);
      if(!isGenerated(index)) // could also consider blocks(index)->isEmptyBlock()
         {
         blocks(index)->append(TR::TreeTop::create(_compilation,TR::Node::create(TR::Goto, 0, gotoTreeTop)));
         }
      setIsGenerated(index);
      return blocks(index)->getEntry();
      //genTreeTop(TR::Node::create(TR::Goto, 0, genTarget(target)));
      }

   bool hasExceptionHandlers()
      {
      return method()->numberOfExceptionHandlers() > 0;
      }

   // how many bytes is the given operand stack element's result?
   int32_t getSize(TR::Node * n)
      {
      return n->getSize();
      }

   // what is the data type of the operand stack element's result?
   TR::DataType getDataType(TR::Node * n)
      {
      return n->getDataType();
      }

   // how many bytes are used on the operand stack to hold the operand stack element's result?
   //   size of addresses depends on the target platform (32- or 64-bit)
   //
   int32_t numberOfByteCodeStackSlots(TR::Node * n)
      {
      // note, getSize(TR::Address) = 8 on 64-bit platforms
      if (getDataType(n) == TR::Address)
         return 4;
      else
         return getSize(n);
      }

   // duplicate the top two elements of the stack
   void dup2()
      {
      TR::Node * &nWord1 = top();

      if (numberOfByteCodeStackSlots(nWord1) == 4)
         shift(2,2);// i think this can be just "shift"
      else
         shift(1,1);// i think this can be just "shift"
      }

   void dupx1()
      {
      shiftAndCopy(2, 1);
      }

   void dup2x1()
      {
      TR::Node * &nWord1 = node(_stack->topIndex());

      if (numberOfByteCodeStackSlots(nWord1) == 4)
         {
         shiftAndCopy(3,2);
         }
      else
         {
         shiftAndCopy(2,1);
         }
      }

   void dupx2()
      {
      TR::Node * &nWord2 = node(_stack->topIndex()-1);

      if (numberOfByteCodeStackSlots(nWord2) == 4)
         {
         shiftAndCopy(3,1);
         }
      else
         {
         shiftAndCopy(2,1);
         }
      }

   void dup2x2()
      {
      TR::Node * &nWord1 = node(_stack->topIndex());

      if (numberOfByteCodeStackSlots(nWord1) == 4)
         {
         TR::Node * &nWord3 = node(_stack->topIndex() - 2);

         if (numberOfByteCodeStackSlots(nWord3) == 4)
            {
            shiftAndCopy(4,2);
            }
         else
            {
            shiftAndCopy(3,2);
            }
         }
      else
         {
         TR::Node * &nWord2 = node(_stack->topIndex() - 1);

         if (numberOfByteCodeStackSlots(nWord2) == 4)
            {
            shiftAndCopy(3,1);
            }
         else
            {
            shiftAndCopy(2,1);
            }
         }
      }

   TR_Array<TR::SymbolReference *> *   _replacedSymRefs;          //Partial Inlining
   };
