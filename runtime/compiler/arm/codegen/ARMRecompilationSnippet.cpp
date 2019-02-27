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

#include "arm/codegen/ARMRecompilationSnippet.hpp"

#include <stdint.h>
#include "arm/codegen/ARMRecompilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

uint8_t *TR::ARMRecompilationSnippet::emitSnippetBody()
   {
   /*
   Snippet will look like:
   bl    TR_ARMcountingRecompileMethod
   dd    jittedBodyInfo
   dd    code start location
   */

   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *countingRecompMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMcountingRecompileMethod, false, false, false);

   getSnippetLabel()->setCodeLocation(buffer);

   *(int32_t *)buffer = encodeHelperBranchAndLink(countingRecompMethodSymRef, buffer, getNode(), cg());  // BL resolve
   buffer += 4;

   *(int32_t *)buffer = (int32_t)(intptrj_t)cg()->comp()->getRecompilationInfo()->getJittedBodyInfo();
   buffer += 4;

   *(int32_t *)buffer = ((int32_t)(intptrj_t)cg()->getCodeStart());
   buffer += 4;

   return buffer;
   }

uint32_t TR::ARMRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return  12;
   }
