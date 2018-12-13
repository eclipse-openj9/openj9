/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <stddef.h>                                 // for NULL
#include <stdint.h>                                 // for int32_t, uint8_t, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/MemoryReference.hpp"              // for MemoryReference
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"                      // for ObjectModel
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                           // for intptrj_t, uintptrj_t
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"                              // for Node
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"                // for LabelSymbol, etc
#include "il/symbol/StaticSymbol.hpp"               // for StaticSymbol
#include "il/symbol/StaticSymbol_inlines.hpp"       // for StaticSymbol
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "p/codegen/PPCTableOfConstants.hpp"        // for PTOC_FULL_INDEX
#include "ras/Debug.hpp"                            // for TR_Debug
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


uint8_t *J9::Power::UnresolvedDataSnippet::emitSnippetBody()
   {
   // *this   swipeable for debugger
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::SymbolReference *glueRef;

   if (getDataSymbol()->getShadowSymbol() != NULL) // instance data
      {
      if (isUnresolvedStore())
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedInstanceDataStoreGlue, false, false, false);
      else
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedInstanceDataGlue, false, false, false);
      }
   else if (getDataSymbol()->isClassObject())
      {
      if (getDataSymbol()->addressIsCPIndexOfStatic())
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedClassGlue2, false, false, false);
      else
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedClassGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstString())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStringGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstMethodType())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodTypeGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstMethodHandle())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodHandleGlue, false, false, false);
      }
   else if (getDataSymbol()->isCallSiteTableEntry())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedCallSiteTableEntryGlue, false, false, false);
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_interpreterUnresolvedMethodTypeTableEntryGlue, false, false, false);
      }
   else if (getDataSymbol()->isConstantDynamic())
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedConstantDynamicGlue, false, false, false);
      }
   else // must be static data
      {
      if (isUnresolvedStore())
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStaticDataStoreGlue, false, false, false);
      else
         glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStaticDataGlue, false, false, false);
      }

   getSnippetLabel()->setCodeLocation(cursor);

   intptrj_t distance = (intptrj_t)glueRef->getMethodAddress() - (intptrj_t)cursor;
   if (!(distance<=BRANCH_FORWARD_LIMIT && distance>=BRANCH_BACKWARD_LIMIT))
      {
      distance = fej9->indexedTrampolineLookup(glueRef->getReferenceNumber(), (void *)cursor) - (intptrj_t)cursor;
      TR_ASSERT(distance<=BRANCH_FORWARD_LIMIT && distance>=BRANCH_BACKWARD_LIMIT,
             "CodeCache is more than 32MB.\n");
      }


   // bl distance
   *(int32_t *)cursor = 0x48000001 | (distance & 0x03fffffc);
   cg()->addProjectSpecializedRelocation(cursor,(uint8_t *)glueRef, NULL, TR_HelperAddress,
                          __FILE__,
                          __LINE__,
                          getNode());
   if (getDataSymbol()->isClassObject() && cg()->wantToPatchClassPointer(NULL, getAddressOfDataReference()))
      {
      uintptrj_t dis = cursor - getAddressOfDataReference();
      cg()->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition((void *) getAddressOfDataReference());
      }
   cursor += 4;

   *(intptrj_t *)cursor = (intptrj_t)getAddressOfDataReference();   // Code Cache RA
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

   *(intptrj_t *)cursor = (intptrj_t)getDataSymbolReference()->getOwningMethod(comp)->constantPool();
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
	    *(int32_t *)cursor = TR::Compiler->target.is64Bit()?0xe8000000:0x80000000;
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
   *(int32_t *)cursor = 0xdeadbeef; // Pached with lis via runtime code
   cursor += 4;
   intptrj_t ra_distance = ((intptrj_t)getAddressOfDataReference()+4) - (intptrj_t)cursor;
   TR_ASSERT(ra_distance<=BRANCH_FORWARD_LIMIT && ra_distance>=BRANCH_BACKWARD_LIMIT, "Return address is more than 32MB.\n");
   *(int32_t *)cursor = 0x48000000 | (ra_distance & 0x03fffffc);

   return cursor+4;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {

   // *this  swipeable for debugger
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
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Code cache return address", *(intptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; pTOC|VD|SY|IX|cpIndex of the data reference", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant pool address", *(intptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

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
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; <clinit> case - Branch back to main line code", (intptrj_t)cursor + distance);
   }

uint32_t J9::Power::UnresolvedDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   // *this   swipeable for debugger
   TR::Compilation* comp = cg()->comp();
   return 28+2*TR::Compiler->om.sizeofReferenceAddress();
   }
