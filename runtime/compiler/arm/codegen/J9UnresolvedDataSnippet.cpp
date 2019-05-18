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

#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "codegen/Relocation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/symbol/StaticSymbol_inlines.hpp"

TR_RuntimeHelper
J9::ARM::UnresolvedDataSnippet::getHelper()
   {
   if (getDataSymbol()->getShadowSymbol() != NULL)           // instance data
      {
      if (resolveForStore())
         return TR_ARMinterpreterUnresolvedInstanceDataStoreGlue;
      else
         return TR_ARMinterpreterUnresolvedInstanceDataGlue;
      }
   if (getDataSymbol()->isClassObject())                // class object
      {
      if (getDataSymbol()->addressIsCPIndexOfStatic())
         return TR_ARMinterpreterUnresolvedClassGlue2;
      return TR_ARMinterpreterUnresolvedClassGlue;
      }

   if (getDataSymbol()->isConstString())                // const string
      return TR_ARMinterpreterUnresolvedStringGlue;

   if (getDataSymbol()->isConstMethodType())
      return TR_interpreterUnresolvedMethodTypeGlue;

   if (getDataSymbol()->isConstMethodHandle())
      return TR_interpreterUnresolvedMethodHandleGlue;

   if (getDataSymbol()->isCallSiteTableEntry())
      return TR_interpreterUnresolvedCallSiteTableEntryGlue;

   if (getDataSymbol()->isMethodTypeTableEntry())
      return TR_interpreterUnresolvedMethodTypeTableEntryGlue;

   if (resolveForStore())
         return TR_ARMinterpreterUnresolvedStaticDataStoreGlue;
      else
         return TR_ARMinterpreterUnresolvedStaticDataGlue;

   }

uint8_t *
J9::ARM::UnresolvedDataSnippet::emitSnippetBody()
   {
   uint8_t            *cursor = cg()->getBinaryBufferCursor();

   getSnippetLabel()->setCodeLocation(cursor);

   *(int32_t *)cursor = encodeHelperBranchAndLink(cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false), cursor, getNode(), cg());  // BL resolve
   cursor += 4;

   *(int32_t *)cursor = (intptr_t)getAddressOfDataReference();   // Code Cache RA
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
               cursor,
               NULL,
               TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
   cursor += 4;

   if (getDataSymbol()->isCallSiteTableEntry())
      {
      *(int32_t *)cursor = getDataSymbol()->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (getDataSymbol()->isMethodTypeTableEntry())
      {
      *(int32_t *)cursor = getDataSymbol()->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else
      {
      *(int32_t *)cursor = getDataSymbolReference()->getCPIndex();  // CP index
      }
   cursor += 4;

   *(int32_t *)cursor = (intptr_t)getDataSymbolReference()->getOwningMethod(cg()->comp())->constantPool();  // CP
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
               cursor,
               *(uint8_t **)cursor,
               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
               TR_ConstantPool, cg()), __FILE__, __LINE__, getNode());
   cursor += 4;

   *(int32_t *)cursor = getMemoryReference()->getOffset(); // offset
   if (getDataSymbol()->isConstObjectRef())
      {
      // reset the offset to NULL
      // the resolve + picbuilder will fully provide the addr to use
      *(int32_t *)cursor = 0;
      }
   cursor += 4;

   *(int32_t *)cursor = 0xE3A00000;                        // Template (mov immed)
   toRealRegister(getMemoryReference()->getModBase())->setRegisterFieldRD((uint32_t *)cursor);
   return cursor+4;
   }

uint32_t
J9::ARM::UnresolvedDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 6 * 4;
   }

