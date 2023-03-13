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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "arm/codegen/ARMRecompilationSnippet.hpp"

#include <stdint.h>
#include "arm/codegen/ARMRecompilation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"

uint8_t *TR::ARMRecompilationSnippet::emitSnippetBody()
   {
   /*
   Snippet will look like:
   bl    TR_ARMcountingRecompileMethod
   dd    jittedBodyInfo
   dd    code start location
   */

   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   TR::SymbolReference  *countingRecompMethodSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMcountingRecompileMethod);

   getSnippetLabel()->setCodeLocation(buffer);

   *(int32_t *)buffer = encodeHelperBranchAndLink(countingRecompMethodSymRef, buffer, getNode(), cg());  // BL resolve
   buffer += 4;

   *(int32_t *)buffer = (int32_t)(intptr_t)cg()->comp()->getRecompilationInfo()->getJittedBodyInfo();
   buffer += 4;

   *(int32_t *)buffer = ((int32_t)(intptr_t)cg()->getCodeStart());
   buffer += 4;

   return buffer;
   }

uint32_t TR::ARMRecompilationSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return  12;
   }
