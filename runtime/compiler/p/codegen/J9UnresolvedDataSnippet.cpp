/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "codegen/UnresolvedDataSnippet.hpp"
#include "codegen/UnresolvedDataSnippet_inlines.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/CompilerEnv.hpp"

J9::Power::UnresolvedDataSnippet::UnresolvedDataSnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *symRef,
      bool isStore,
      bool canCauseGC) :
   J9::UnresolvedDataSnippet(cg, node, symRef, isStore, canCauseGC),
     _memoryReference(NULL), _dataRegister(NULL)
   {
   }

uint8_t *J9::Power::UnresolvedDataSnippet::getAddressOfDataReference()
   {
   if (self()->getDataReferenceInstruction())
      return self()->getDataReferenceInstruction()->getBinaryEncoding();
   else
      return self()->OMR::UnresolvedDataSnippet::getAddressOfDataReference();
   }

uint8_t *J9::Power::UnresolvedDataSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::SymbolReference *glueRef;
   TR_RuntimeHelper refNum;

   if (getDataSymbol()->getShadowSymbol() != NULL) // instance data
      {
      if (isUnresolvedStore())
         refNum = TR_PPCinterpreterUnresolvedInstanceDataStoreGlue;
      else
         refNum = TR_PPCinterpreterUnresolvedInstanceDataGlue;
      }
   else if (getDataSymbol()->isClassObject())
      {
      if (getDataSymbol()->addressIsCPIndexOfStatic())
         refNum = TR_PPCinterpreterUnresolvedClassGlue2;
      else
         refNum = TR_PPCinterpreterUnresolvedClassGlue;
      }
   else if (getDataSymbol()->isConstString())
      {
      refNum = TR_PPCinterpreterUnresolvedStringGlue;
      }
   else if (getDataSymbol()->isConstMethodType())
      {
      refNum = TR_interpreterUnresolvedMethodTypeGlue;
      }
   else if (getDataSymbol()->isConstMethodHandle())
      {
      refNum = TR_interpreterUnresolvedMethodHandleGlue;
      }
   else if (getDataSymbol()->isCallSiteTableEntry())
      {
      refNum = TR_interpreterUnresolvedCallSiteTableEntryGlue;
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      refNum = TR_interpreterUnresolvedMethodTypeTableEntryGlue;
      }
   else if (getDataSymbol()->isConstantDynamic())
      {
      refNum = TR_PPCinterpreterUnresolvedConstantDynamicGlue;
      }
   else // must be static data
      {
      if (isUnresolvedStore())
         refNum = TR_PPCinterpreterUnresolvedStaticDataStoreGlue;
      else
         refNum = TR_PPCinterpreterUnresolvedStaticDataGlue;
      }

   glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(refNum, false, false, false);
   getSnippetLabel()->setCodeLocation(cursor);

   intptr_t helperAddress = (intptr_t)glueRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptr_t)cursor))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinIFormBranchRange(helperAddress, (intptr_t)cursor), "Helper address is out of range");
      }

   // bl distance
   *(int32_t *)cursor = 0x48000001 | ((helperAddress - (intptr_t)cursor) & 0x03fffffc);
   cg()->addProjectSpecializedRelocation(cursor,(uint8_t *)glueRef, NULL, TR_HelperAddress,
                          __FILE__,
                          __LINE__,
                          getNode());
   if (getDataSymbol()->isClassObject() && cg()->wantToPatchClassPointer(NULL, getAddressOfDataReference()))
      {
      uintptr_t dis = cursor - getAddressOfDataReference();
      cg()->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition((void *) getAddressOfDataReference());
      }
   cursor += 4;

   *(intptr_t *)cursor = (intptr_t)getAddressOfDataReference();   // Code Cache RA
   cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, getNode());
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   if (getDataSymbolReference()->getSymbol()->isCallSiteTableEntry())
      {
      *(int32_t *)cursor = getDataSymbolReference()->getSymbol()->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      *(int32_t *)cursor = getDataSymbolReference()->getSymbol()->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else // constant pool index
      {
      *(int32_t *)cursor = getDataSymbolReference()->getCPIndex();
      }

   if (getMemoryReference()->isTOCAccess())
      *(int32_t *)cursor |= 1<<31;                           // Set Pseudo TOC bit
   if (isSpecialDouble())
      *(int32_t *)cursor |= 1<<30;                           // Set Special Double bit
   if (inSyncSequence())
      *(int32_t *)cursor |= 1<<29;                           // Set Sync Pattern bit
   if (getMemoryReference()->useIndexedForm())
      *(int32_t *)cursor |= 1<<28;                           // Set the index bit
   if (is32BitLong())
      *(int32_t *)cursor |= 1<<27;	                     // Set the double word load/store bit
   cursor += 4;

   *(intptr_t *)cursor = (intptr_t)getDataSymbolReference()->getOwningMethod(comp)->constantPool();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,*(uint8_t **)cursor,
                                                                          getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                          TR_ConstantPool,
                                                                          cg()),
                          __FILE__,
                          __LINE__,
                          getNode());
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   if (getMemoryReference()->isTOCAccess())
      {
      *(int32_t *)cursor = getMemoryReference()->getTOCOffset();
      }
   else
      {
      *(int32_t *)cursor = getMemoryReference()->getOffset(*(comp)); // offset
      if (getDataSymbol()->isConstObjectRef() || getDataSymbol()->isConstantDynamic())
         {
         cg()->addProjectSpecializedRelocation(cursor, *(uint8_t **)(cursor-4),
               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool,
                                __FILE__,
                                __LINE__,
                                getNode());
         }
      }
   cursor += 4;

   if (getMemoryReference()->isTOCAccess())
      {
      if (getMemoryReference()->getTOCOffset() != PTOC_FULL_INDEX)
	 {
         if (getMemoryReference()->getTOCOffset()<LOWER_IMMED ||
             getMemoryReference()->getTOCOffset()>UPPER_IMMED)
	    {
            *(int32_t *)cursor = 0x3c000000;
            toRealRegister(getMemoryReference()->getModBase())->setRegisterFieldRT((uint32_t *)cursor);
	    }
         else
	    {
       *(int32_t *)cursor = cg()->comp()->target().is64Bit()?0xe8000000:0x80000000;
            getDataRegister()->setRegisterFieldRT((uint32_t *)cursor);
	    }
         cg()->getTOCBaseRegister()->setRegisterFieldRA((uint32_t *)cursor);
	 }
      else
	 {
         *(int32_t *)cursor = 0x3c000000;
         getDataRegister()->setRegisterFieldRT((uint32_t *)cursor);
	 }
      }
   else
      {
      *(int32_t *)cursor = 0x3c000000;                        // Template
      toRealRegister(getMemoryReference()->getModBase())->setRegisterFieldRT((uint32_t *)cursor);
      if (getMemoryReference()->getBaseRegister() == NULL)
         {
         cg()->machine()->getRealRegister(TR::RealRegister::gr0)->setRegisterFieldRA((uint32_t *)cursor);
         }
      else
         {
         toRealRegister(getMemoryReference()->getBaseRegister())->setRegisterFieldRA((uint32_t *)cursor);
         }
      }
   cursor += 4;

   *(int32_t *)cursor = 0;                                 // Lock word

   // CLInit case
   cursor += 4;
   *(int32_t *)cursor = 0xdeadbeef; // Patched with lis via runtime code
   cursor += 4;
   intptr_t targetAddress = (intptr_t)getAddressOfDataReference()+4;
   TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinIFormBranchRange(targetAddress, (intptr_t)cursor),
                   "Return address is out of range");
   *(int32_t *)cursor = 0x48000000 | ((targetAddress - (intptr_t)cursor) & 0x03fffffc);

   return cursor+4;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {
   uint8_t            *cursor = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Unresolved Data Snippet");

   TR::SymbolReference *glueRef;

   if (snippet->getDataSymbol()->getShadowSymbol() != NULL) // instance data
      {
      if (snippet->isUnresolvedStore())
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedInstanceDataStoreGlue);
      else
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedInstanceDataGlue);
      }
   else if (snippet->getDataSymbol()->isClassObject())
      {
      if (snippet->getDataSymbol()->addressIsCPIndexOfStatic())
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedClassGlue2);
      else
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedClassGlue);
      }
   else if (snippet->getDataSymbol()->isConstString())
      {
      glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedStringGlue);
      }
   else if (snippet->getDataSymbol()->isConstMethodType())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodTypeGlue);
      }
   else if (snippet->getDataSymbol()->isConstMethodHandle())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodHandleGlue);
      }
   else if (snippet->getDataSymbol()->isCallSiteTableEntry())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedCallSiteTableEntryGlue);
      }
   else if (snippet->getDataSymbol()->isMethodTypeTableEntry())
      {
      glueRef = _cg->getSymRef(TR_interpreterUnresolvedMethodTypeTableEntryGlue);
      }
   else if (snippet->getDataSymbol()->isConstantDynamic())
      {
      glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedConstantDynamicGlue);
      }
   else // must be static data
      {
      if (snippet->isUnresolvedStore())
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedStaticDataStoreGlue);
      else
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedStaticDataGlue);
      }

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(glueRef, cursor, distance))
      info = " Through Trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptr_t)cursor + distance, info);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Code cache return address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; pTOC|VD|SY|IX|cpIndex of the data reference", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant pool address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Offset to be merged", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Instruction template", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Lock word initialized to 0", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; <clinit> case - 1st instruction (0xdeadbeef)", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; <clinit> case - Branch back to main line code", (intptr_t)cursor + distance);
   }

uint32_t J9::Power::UnresolvedDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   return 28+2*TR::Compiler->om.sizeofReferenceAddress();
   }
