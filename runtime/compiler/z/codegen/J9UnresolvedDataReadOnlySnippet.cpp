/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#pragma csect(CODE,"J9ZUnresolvedDataReadOnlySnippet#C")
#pragma csect(STATIC,"J9ZUnresolvedDataReadOnlySnippet#S")
#pragma csect(TEST,"J9ZUnresolvedDataReadOnlySnippet#T")


#include "codegen/J9UnresolvedDataReadOnlySnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "objectfmt/GlobalFunctionCallData.hpp"
#include "objectfmt/ObjectFormat.hpp"
#include "runtime/J9Runtime.hpp"

J9::Z::UnresolvedDataReadOnlySnippet::UnresolvedDataReadOnlySnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *dataSymRef,
      intptr_t resolveDataAddress,
      TR::LabelSymbol *startResolveSequenceLabel,
      TR::LabelSymbol *volatileFenceLabel,
      TR::LabelSymbol *doneLabel) :
   Snippet(cg, node, generateLabelSymbol(cg)),
      resolveDataAddress(resolveDataAddress),
      dataSymRef(dataSymRef),
      startResolveSequenceLabel(startResolveSequenceLabel),
      volatileFenceLabel(volatileFenceLabel),
      doneLabel(doneLabel)
   {}

uint8_t*
J9::Z::UnresolvedDataReadOnlySnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();

   uint8_t *cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   TR::SymbolReference *glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false);

   TR::GlobalFunctionCallData dataDestination(glueSymRef, getNode(), cursor, cg(), self());
   cursor = cg()->getObjFmt()->encodeGlobalFunctionCall(dataDestination);

   // Relative address to the CCUnresolvedData*
   intptr_t addressA = reinterpret_cast<intptr_t>(resolveDataAddress);
   intptr_t addressB = reinterpret_cast<intptr_t>(cursor);

   TR_ASSERT_FATAL(cg()->canUseRelativeLongInstructions(addressA), "CCUnresolvedData* [%p] is outside relative immediate range", addressA);

   *reinterpret_cast<int32_t*>(cursor) = static_cast<int32_t>(addressA - addressB);
   cursor += 4;

   // Constant pool index
   *reinterpret_cast<int32_t*>(cursor) = getCPIndex();
   cursor += 4;

   // Relative address to the start of the mainline resolution
   addressA = reinterpret_cast<intptr_t>(startResolveSequenceLabel->getCodeLocation());
   addressB = reinterpret_cast<intptr_t>(cursor);

   TR_ASSERT_FATAL(cg()->canUseRelativeLongInstructions(addressA), "startResolveSequenceLabel [%p] is outside relative immediate range", addressA);

   *reinterpret_cast<int32_t*>(cursor) = static_cast<int32_t>(addressA - addressB);
   cursor += 4;

   if (volatileFenceLabel != NULL)
      {
      volatileFenceLabel->setCodeLocation(cursor);

      // Volatile store memory fence (BCR 14,R0)
      *reinterpret_cast<int16_t*>(cursor) = 0x07E0;
      cursor += 2;

      // Branch to doneLabel (BRCL 0xF,[doneLabel])
      *reinterpret_cast<int16_t*>(cursor) = 0xC0F4;

      addressA = reinterpret_cast<intptr_t>(doneLabel->getCodeLocation());
      addressB = reinterpret_cast<intptr_t>(cursor);

      TR_ASSERT_FATAL(cg()->canUseRelativeLongInstructions(addressA), "doneLabel [%p] is outside relative immediate range", addressA);

      *reinterpret_cast<int32_t*>(cursor + 2) = static_cast<int32_t>((addressA - addressB) / 2);
      cursor += 6;
      }

   return cursor;
   }

uint32_t
J9::Z::UnresolvedDataReadOnlySnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t length = 8 + 4 + 4 + 4;

   if (volatileFenceLabel != NULL)
      length += 2 + 6;

   return length;
   }

void
J9::Z::UnresolvedDataReadOnlySnippet::print(TR::FILE *f, TR_Debug *debug)
   {
   uint8_t *cursor = getSnippetLabel()->getCodeLocation();
   debug->printSnippetLabel(f, getSnippetLabel(), cursor, "Unresolved Data Read-Only Snippet");

   TR::SymbolReference *glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false);

   debug->printPrefix(f, NULL, cursor, 6);
   trfprintf(f, "LGRL \tGPR14, %s", debug->getName(glueSymRef));
   cursor += 6;

   debug->printPrefix(f, NULL, cursor, 2);
   trfprintf(f, "BASR \tGPR14, GPR14");
   cursor += 2;

   debug->printPrefix(f, NULL, cursor, 6);
   trfprintf(f, "DC    \t0x%08X \t# Relative address to the CCUnresolvedData*", *reinterpret_cast<int32_t*>(cursor));
   cursor += 4;

   debug->printPrefix(f, NULL, cursor, 6);
   trfprintf(f, "DC    \t0x%08X \t# Constant pool index", *reinterpret_cast<int32_t*>(cursor));
   cursor += 4;

   debug->printPrefix(f, NULL, cursor, 6);
   trfprintf(f, "DC    \t0x%08X \t# Relative address to the start of the mainline resolution", *reinterpret_cast<int32_t*>(cursor));
   cursor += 4;

   if (volatileFenceLabel != NULL)
      {
      debug->printSnippetLabel(f, volatileFenceLabel, cursor, "volatileFenceLabel");

      debug->printPrefix(f, NULL, cursor, 6);
      trfprintf(f, "BCR   \t14, GPR0");
      cursor += 2;

      debug->printPrefix(f, NULL, cursor, 6);
      trfprintf(f, "BRCL  \t0xF, [doneLabel]");
      cursor += 6;
      }
   }

TR_RuntimeHelper
J9::Z::UnresolvedDataReadOnlySnippet::getHelper()
   {
   TR::Symbol* dataSymbol = dataSymRef->getSymbol();

   if (dataSymbol->isShadow())
      return getNode()->getOpCode().isStore() ? 
         TR_interpreterUnresolvedFieldSetterReadOnlyGlue :
         TR_interpreterUnresolvedFieldReadOnlyGlue;

   if (dataSymbol->isClassObject())
      return dataSymbol->addressIsCPIndexOfStatic() ? 
         TR_interpreterUnresolvedClassFromStaticFieldReadOnlyGlue :
         TR_interpreterUnresolvedClassReadOnlyGlue;

   if (dataSymbol->isConstString())
      return TR_interpreterUnresolvedStringReadOnlyGlue;

   if (dataSymbol->isConstMethodType())
      return TR_interpreterUnresolvedMethodTypeReadOnlyGlue;

   if (dataSymbol->isConstMethodHandle())
      return TR_interpreterUnresolvedMethodHandleReadOnlyGlue;

   if (dataSymbol->isCallSiteTableEntry())
      return TR_interpreterUnresolvedCallSiteTableEntryReadOnlyGlue;

   if (dataSymbol->isMethodTypeTableEntry())
      return TR_interpreterUnresolvedMethodTypeTableEntryReadOnlyGlue;

   if (dataSymbol->isConstantDynamic())
      return TR_interpreterUnresolvedConstantDynamicReadOnlyGlue;

   // Must be a static data reference
   return getNode()->getOpCode().isStore() ?
      TR_interpreterUnresolvedStaticFieldSetterReadOnlyGlue :
      TR_interpreterUnresolvedStaticFieldReadOnlyGlue;
   }

int32_t
J9::Z::UnresolvedDataReadOnlySnippet::getCPIndex()
   {
   TR::Symbol* dataSymbol = dataSymRef->getSymbol();

   if (dataSymbol->isCallSiteTableEntry())
      return dataSymbol->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
   
   if (dataSymbol->isMethodTypeTableEntry())
      return dataSymbol->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();

   return dataSymRef->getCPIndex();
   }
