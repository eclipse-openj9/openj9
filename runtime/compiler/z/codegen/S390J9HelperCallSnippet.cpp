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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "z/codegen/S390J9HelperCallSnippet.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"


uint8_t *
TR::S390J9HelperCallSnippet::emitSnippetBody() {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   TR::Node *callNode = getNode();
   TR::SymbolReference *helperSymRef = getHelperSymRef();
   bool jitInduceOSR = helperSymRef->isOSRInductionHelper();
   bool isJitDispatchJ9Method = callNode->isJitDispatchJ9MethodCall(cg()->comp());
   if (jitInduceOSR || isJitDispatchJ9Method) {
      // Flush in-register arguments back to the stack for interpreter
      cursor = TR::S390J9CallSnippet::S390flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());
   }

   /* The J9Method pointer to be passed to be interpreter is in R7, but the interpreter expects it to be in R1.
    * Since the first integer argument register is also R1, we only load it after we have flushed the args to the stack.
    *
    * 64 bit:
    *    LGR R1, R7
    * 32 bit:
    *    LR R1, R7
    */
   if (getNode()->isJitDispatchJ9MethodCall(comp()))
      {
      if (comp()->target().is64Bit())
         {
         *(int32_t *)cursor = 0xB9040017;
         cursor += sizeof(int32_t);
         }
      else
         {
         *(int16_t *)cursor = 0x1817;
         cursor += sizeof(int16_t);
         }
      }

   return emitSnippetBodyHelper(cursor, helperSymRef);
}
