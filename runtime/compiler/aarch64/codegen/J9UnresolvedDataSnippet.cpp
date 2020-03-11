/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"

J9::ARM64::UnresolvedDataSnippet::UnresolvedDataSnippet(TR::CodeGenerator *cg, TR::Node *node, TR::SymbolReference *symRef, bool isStore, bool isGCSafePoint)
   : J9::UnresolvedDataSnippet(cg, node, symRef, isStore, isGCSafePoint),
     _memoryReference(NULL)
   {
   cg->machine()->setLinkRegisterKilled(true);
   }

TR_RuntimeHelper
J9::ARM64::UnresolvedDataSnippet::getHelper()
   {
   if (getDataSymbol()->getShadowSymbol() != NULL) // instance data
      {
      if (isUnresolvedStore())
         return TR_ARM64interpreterUnresolvedInstanceDataStoreGlue;
      else
         return TR_ARM64interpreterUnresolvedInstanceDataGlue;
      }
   else if (getDataSymbol()->isClassObject()) // class object
      {
      if (getDataSymbol()->addressIsCPIndexOfStatic())
         return TR_ARM64interpreterUnresolvedClassGlue2;
      else
         return TR_ARM64interpreterUnresolvedClassGlue;
      }
   else if (getDataSymbol()->isConstString()) // const string
      {
      return TR_ARM64interpreterUnresolvedStringGlue;
      }
   else if (getDataSymbol()->isConstMethodType())
      {
      return TR_interpreterUnresolvedMethodTypeGlue;
      }
   else if (getDataSymbol()->isConstMethodHandle())
      {
      return TR_interpreterUnresolvedMethodHandleGlue;
      }
   else if (getDataSymbol()->isCallSiteTableEntry())
      {
      return TR_interpreterUnresolvedCallSiteTableEntryGlue;
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      return TR_interpreterUnresolvedMethodTypeTableEntryGlue;
      }
   else if (getDataSymbol()->isConstantDynamic())
      {
      return TR_ARM64interpreterUnresolvedConstantDynamicGlue;
      }
   else // must be static data
      {
      if (isUnresolvedStore())
         return TR_ARM64interpreterUnresolvedStaticDataStoreGlue;
      else
         return TR_ARM64interpreterUnresolvedStaticDataGlue;
      }
   }

uint8_t *
J9::ARM64::UnresolvedDataSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR_RuntimeHelper helper = getHelper();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);

   getSnippetLabel()->setCodeLocation(cursor);

   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, getNode()); // BL resolve
   cursor += 4;

   *(intptr_t *)cursor = (intptr_t)getAddressOfDataReference(); // Code Cache RA
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
               cursor,
               NULL,
               TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
   cursor += 8;

   if (getDataSymbol()->isCallSiteTableEntry())
      {
      *(intptr_t *)cursor = getDataSymbol()->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      *(intptr_t *)cursor = getDataSymbol()->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else
      {
      *(intptr_t *)cursor = getDataSymbolReference()->getCPIndex(); // CP index
      }
   cursor += 8;

   *(intptr_t *)cursor = (intptr_t)getDataSymbolReference()->getOwningMethod(cg()->comp())->constantPool(); // CP
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
               cursor,
               *(uint8_t **)cursor,
               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
               TR_ConstantPool, cg()), __FILE__, __LINE__, getNode());
   cursor += 8;

   *(int32_t *)cursor = getMemoryReference()->getOffset(); // offset
   if (getDataSymbol()->isConstObjectRef())
      {
      // reset the offset to zero
      // the resolve + picbuilder will fully provide the addr to use
      *(int32_t *)cursor = 0;
      }
   cursor += 4;

   *(int32_t *)cursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movzx); // instruction template -- "movz extraReg, #0"
   TR_ASSERT(getMemoryReference()->getExtraRegister(), "_extraReg must have been allocated");
   toRealRegister(getMemoryReference()->getExtraRegister())->setRegisterFieldRD((uint32_t *)cursor);

   return cursor+4;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();
   TR_RuntimeHelper helper = snippet->getHelper();
   TR::SymbolReference *glueRef = _cg->getSymRef(helper);

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Unresolved Data Snippet");

   char *info = "";
   int32_t distance;
   if (isBranchToTrampoline(glueRef, cursor, distance))
      {
      info = " Through Trampoline";
      }

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03ffffff; // imm26
   distance = (distance << 6) >> 4; // sign extend and add two 0 bits
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s%s",
             (intptr_t)cursor + distance, getRuntimeHelperName(helper), info);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Code cache return address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t0x%08x\t\t; cpIndex of the data reference", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Constant pool address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".word \t0x%08x\t\t; Offset to be merged", *(int32_t *)cursor);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   int32_t instr = *(int32_t *)cursor;
   trfprintf(pOutFile, ".word \t0x%08x\t\t; Instruction template (extraReg=x%d)", instr, (instr & 0x1F));
   }

uint32_t
J9::ARM64::UnresolvedDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 9 * 4;
   }
