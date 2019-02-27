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

#include "codegen/ARMHelperCallSnippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/jittypes.h"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *TR::ARMHelperCallSnippet::emitSnippetBody()
   {
   uint8_t   *buffer = cg()->getBinaryBufferCursor();
   intptrj_t distance = (intptrj_t)getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress() - (intptrj_t)buffer - 8;

   getSnippetLabel()->setCodeLocation(buffer);

   if (!(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT))
      {
      distance = TR::CodeCacheManager::instance()->findHelperTrampoline(getDestination()->getReferenceNumber(), (void *)buffer) - (intptrj_t)buffer - 8;
      TR_ASSERT(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT,
             "CodeCache is more than 32MB.\n");
      }

   // b|bl distance
   *(int32_t *)buffer = 0xea000000 | ((distance >> 2)& 0x00ffffff);
   if (_restartLabel != NULL)
      *(int32_t *)buffer |= 0x01000000;
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                             buffer,
                             (uint8_t *)getDestination(),
                             TR_HelperAddress, cg()), __FILE__, __LINE__, getNode());
   buffer += 4;

   gcMap().registerStackMap(buffer, cg());

   if (_restartLabel != NULL)
      {
      int32_t returnDistance = _restartLabel->getCodeLocation() - buffer - 8 ;
      *(int32_t *)buffer = 0xea000000 | ((returnDistance >> 2) & 0x00ffffff);
      buffer += 4;
      }

   return buffer;
   }

uint32_t TR::ARMHelperCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return ((_restartLabel==NULL)?4:8);
   }
