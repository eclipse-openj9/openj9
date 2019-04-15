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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"TRJ9CGRABase#C")
#pragma csect(STATIC,"TRJ9CGRABase#S")
#pragma csect(TEST,"TRJ9CGRABase#T")

#include "codegen/J9CodeGenerator.hpp"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveReference.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "ras/Debug.hpp"

TR::AutomaticSymbol *
J9::CodeGenerator::allocateVariableSizeSymbol(int32_t size)
   {
   TR::AutomaticSymbol *sym = TR::AutomaticSymbol::createVariableSized(self()->trHeapMemory(), size);
   self()->comp()->getMethodSymbol()->addVariableSizeSymbol(sym);
   if (self()->getDebug())
      self()->getDebug()->newVariableSizeSymbol(sym);
   return sym;
   }

TR::SymbolReference *
J9::CodeGenerator::allocateVariableSizeSymRef(int32_t byteLength)
   {
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tallocateVariableSizeSymbolReference: length = %d\n",byteLength);
   TR::SymbolReference *symRef = self()->getFreeVariableSizeSymRef(byteLength);
   TR::AutomaticSymbol *sym = NULL;
   if (symRef == NULL)
      {
      sym = self()->allocateVariableSizeSymbol(byteLength);
      symRef = new (self()->trHeapMemory()) TR::SymbolReference(self()->comp()->getSymRefTab(), sym);
      symRef->setIsTempVariableSizeSymRef();
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\tno available symRef allocate symRef #%d : %s (%p) of length = %d\n",symRef->getReferenceNumber(),self()->getDebug()->getName(sym),sym,byteLength);
      _variableSizeSymRefAllocList.push_front(symRef);
      }
   else
      {
      sym = symRef->getSymbol()->getVariableSizeSymbol();
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\treuse available symRef #%d : %s (%p) with length = %d\n",symRef->getReferenceNumber(),self()->getDebug()->getName(sym),sym,byteLength);
      }
   sym->setActiveSize(byteLength);
   sym->setReferenceCount(0);
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\treturning symRef #%d (%s) : activeSize set to %d (length = %d)\n",
         symRef->getReferenceNumber(),self()->getDebug()->getName(sym),sym->getActiveSize(),sym->getSize());
   return symRef;
   }

void
J9::CodeGenerator::freeAllVariableSizeSymRefs()
   {
   if (!_variableSizeSymRefPendingFreeList.empty())
      {
      auto it = _variableSizeSymRefPendingFreeList.begin();
      while (it != _variableSizeSymRefPendingFreeList.end())
         {
         TR_ASSERT((*it)->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",(*it)->getReferenceNumber());
         self()->freeVariableSizeSymRef(*(it++), true); // freeAddressTakenSymbols=true
         }
      }
   }

void
J9::CodeGenerator::freeVariableSizeSymRef(
      TR::SymbolReference *symRef,
      bool freeAddressTakenSymbol)
   {
   TR_ASSERT(symRef->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",symRef->getReferenceNumber());
   auto *sym = symRef->getSymbol()->getVariableSizeSymbol();
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tfreeVariableSizeSymbol: #%d (%s)%s%s%s\n",
         symRef->getReferenceNumber(),self()->getDebug()->getName(sym),
         sym->isSingleUse()?", isSingleUse=true":"",freeAddressTakenSymbol?", freeAddressTakenSymbol=true":"",sym->isAddressTaken()?", symAddrTaken=true":"");
   TR_ASSERT(!(std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), symRef) != _variableSizeSymRefFreeList.end())
		   ,"symRef #%d has already been freed\n",symRef->getReferenceNumber());
   if (sym->isAddressTaken() && !freeAddressTakenSymbol)
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\tsym->isAddressTaken()=true and freeAddressTakenSymbol=false so do not free sym #%d (%s %p)\n",symRef->getReferenceNumber(),self()->getDebug()->getName(sym),sym);
      return;
      }
   else
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\tfree symRef #%d (%s %p)\n",symRef->getReferenceNumber(),self()->getDebug()->getName(sym),sym);
      sym->setIsSingleUse(false);
      sym->setIsAddressTaken(false);
      sym->setNodeToFreeAfter(NULL);
      sym->setReferenceCount(0);
      symRef->resetHasTemporaryNegativeOffset();
      bool found = (std::find(_variableSizeSymRefPendingFreeList.begin(), _variableSizeSymRefPendingFreeList.end(), symRef) != _variableSizeSymRefPendingFreeList.end());
      if (!_variableSizeSymRefPendingFreeList.empty() && found)
         _variableSizeSymRefPendingFreeList.remove(symRef);
      _variableSizeSymRefFreeList.push_front(symRef);
      }
   }

void
J9::CodeGenerator::pendingFreeVariableSizeSymRef(TR::SymbolReference *symRef)
   {
   TR_ASSERT(symRef->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",symRef->getReferenceNumber());
   bool found = (std::find(_variableSizeSymRefPendingFreeList.begin(), _variableSizeSymRefPendingFreeList.end(), symRef) != _variableSizeSymRefPendingFreeList.end());
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tpendingFreeVariableSizeSymRef: #%d (%s) %s to pending free list\n",
         symRef->getReferenceNumber(),self()->getDebug()->getName(symRef->getSymbol()),found ?"do not add (already present)":"add");
   if (!found)
      _variableSizeSymRefPendingFreeList.push_front(symRef);
   }


TR::SymbolReference *
J9::CodeGenerator::getFreeVariableSizeSymRef(int32_t byteLength)
   {
   TR::SymbolReference *biggestSymRef;
   if(_variableSizeSymRefFreeList.empty())
	   return NULL;
   else
	   biggestSymRef = _variableSizeSymRefFreeList.front();

   TR::SymbolReference *previous = NULL;
   TR::SymbolReference *savedPrevious = NULL;
   TR::SymbolReference *biggestPrevious = NULL;
   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tgetFreeVariableSizeSymRef of length %d\n",byteLength);

   if (biggestSymRef)
      {
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\tset initial biggestSymRef to #%d (%s) with length %d\n",
            biggestSymRef->getReferenceNumber(),self()->getDebug()->getName(biggestSymRef->getSymbol()),biggestSymRef->getSymbol()->getSize());
      auto i = _variableSizeSymRefFreeList.begin();
      while (i != _variableSizeSymRefFreeList.end())
         {
         previous = savedPrevious;
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"\t\texamine free symRef #%d (%s) with length %d\n",(*i)->getReferenceNumber(),self()->getDebug()->getName((*i)->getSymbol()),(*i)->getSymbol()->getSize());
         if ((*i)->getSymbol()->getSize() >= byteLength)
            {
            if (self()->traceBCDCodeGen())
               {
               traceMsg(self()->comp(),"\t\tfound big enough free symRef #%d (%s) with length >= req length of %d\n",(*i)->getReferenceNumber(),self()->getDebug()->getName((*i)->getSymbol()),byteLength);
               traceMsg(self()->comp(),"\t\tremove free symRef #%d (%s) from list, previous is %p\n",(*i)->getReferenceNumber(),self()->getDebug()->getName((*i)->getSymbol()),previous ? previous:(void *)0);
               }
            TR::SymbolReference *symRef = *i;
            if(previous == NULL)
            	_variableSizeSymRefFreeList.pop_front();
            else
            {
            	auto foundIt = std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), previous);
            	if(foundIt != _variableSizeSymRefFreeList.end())
            	{
            		++foundIt;
            		_variableSizeSymRefFreeList.erase(foundIt);   // pops head if previous==NULL
            	}
            }
            TR_ASSERT(!(std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), (*i)) != _variableSizeSymRefFreeList.end())
            		,"shouldn't find symRef #%d as it was just removed\n",(*i)->getReferenceNumber());
            return symRef;
            }
         else if ((*i)->getSymbol()->getSize() > biggestSymRef->getSymbol()->getSize())
            {
            if (self()->traceBCDCodeGen())
               traceMsg(self()->comp(),"\t\tupdate biggest symRef seen to #%d (%s) with length %d\n",(*i)->getReferenceNumber(),self()->getDebug()->getName((*i)->getSymbol()),byteLength);
            biggestPrevious = previous;
            biggestSymRef = *i;
            }
         savedPrevious = *i;
         ++i;
         }
      if (self()->traceBCDCodeGen())
         traceMsg(self()->comp(),"\t\tincrease biggestSymRef #%d (%s) size from %d -> %d\n",
            biggestSymRef->getReferenceNumber(),self()->getDebug()->getName(biggestSymRef->getSymbol()),biggestSymRef->getSymbol()->getSize(),byteLength);
      biggestSymRef->getSymbol()->setSize(byteLength);
      }

   if (self()->traceBCDCodeGen() && biggestSymRef)
      traceMsg(self()->comp(),"\t\tremove free symRef #%d (%s) from list, previous is %p\n",biggestSymRef->getReferenceNumber(),self()->getDebug()->getName(biggestSymRef->getSymbol()),previous ? previous:(void *)9999);
   if(biggestPrevious == NULL)
	   _variableSizeSymRefFreeList.pop_front();
   else
   {
	   auto foundIt = std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), biggestPrevious);
	   if(foundIt != _variableSizeSymRefFreeList.end())
	   {
		   auto nextIt = ++foundIt;
		   _variableSizeSymRefFreeList.remove(*nextIt);   // pops head if previous==NULL
	   }
   }
   TR_ASSERT(!biggestSymRef || !(std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), biggestSymRef) != _variableSizeSymRefFreeList.end())
		   ,"shouldn't find biggestSymRef #%d as it was just removed\n",biggestSymRef->getReferenceNumber());
   return biggestSymRef;
   }

void
J9::CodeGenerator::checkForUnfreedVariableSizeSymRefs()
   {
   bool foundUnfreedSlot = false;
   for (auto i = _variableSizeSymRefAllocList.begin(); i != _variableSizeSymRefAllocList.end(); ++i)
      {
	  bool found = (std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), (*i)) != _variableSizeSymRefFreeList.end());
      if (!found)
         {
         TR_ASSERT((*i)->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",(*i)->getReferenceNumber());
         if (self()->traceBCDCodeGen())
            traceMsg(self()->comp(),"Variable size symRef #%d (%s) has not been freed (symbol refCount is %d)\n",
            		(*i)->getReferenceNumber(),self()->getDebug()->getName((*i)->getSymbol()),(*i)->getSymbol()->getVariableSizeSymbol()->getReferenceCount());
         foundUnfreedSlot = true;
         }
      }
   TR_ASSERT(!foundUnfreedSlot,"Unfreed variable size symRefs found at end of method\n");
   }
