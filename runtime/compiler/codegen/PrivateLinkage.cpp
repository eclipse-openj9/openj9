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

#include "codegen/PrivateLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/ParameterSymbol.hpp"

intptr_t
J9::PrivateLinkage::entryPointFromCompiledMethod()
   {
   uint8_t *methodEntry = cg()->getCodeStart();
   methodEntry += J9::PrivateLinkage::LinkageInfo::get(methodEntry)->getReservedWord();
   return reinterpret_cast<intptr_t>(methodEntry);
   }

intptr_t
J9::PrivateLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

void
J9::PrivateLinkage::mapIncomingParms(TR::ResolvedMethodSymbol *method)
   {
   int32_t offsetToFirstArg = method->getNumParameterSlots() * TR::Compiler->om.sizeofReferenceAddress() + getOffsetToFirstParm();

   const bool is64Bit = cg()->comp()->target().is64Bit();
   ListIterator<TR::ParameterSymbol> paramIterator(&method->getParameterList());
   for (TR::ParameterSymbol* paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      // This is a slightly convoluted way of enforcing the JVM specification which states that long and double 
      // variables take up two stack slots. A stack slot in OpenJ9 is a `uintptr_t`, so on 64-bit int variables
      // are still placed in 64-bit stack slots, hence the need to check for 64-bit in the query below. For more
      // details please see eclipse/openj9#8360.
      int32_t slotMultiplier = is64Bit && paramCursor->getDataType() != TR::Address ? 2 : 1;

      paramCursor->setParameterOffset(offsetToFirstArg -
         paramCursor->getParameterOffset() -
         paramCursor->getSize() * slotMultiplier);
      }
   }
