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

#include <stdint.h>
#include "rommeth.h"
#include "env/jittypes.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"

#define IS_32BIT_RIP(x,rip)  ((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip)))


// Given a RAM method, determine the appropriate interpreted dispatch glue helper
// (or trampoline) to call and return the disp32 displacement for a direct call
// from the dispatch snippet.
//
// The location of the helper index in the snippet depends on where it best fits for
// alignment.
//
// Unresolved interpreted dispatch snippet shape:
//
// 64-bit
// ======
//       align 8
// (10)  call  interpreterUnresolved{Static|Special}Glue  ; replaced with "mov rdi, 0x0000aabbccddeeff"
// (5)   call  updateInterpreterDispatchGlueSite          ; replaced with "JMP disp32"
// (2)   dw    2-byte glue method helper index
// (8)   dq    cpAddr
// (4)   dd    cpIndex

//
// 32-bit
// ======
//      align 8
// (6)  call  interpreterUnresolved{Static|Special}Glue   ; replaced with "mov edi, 0xaabbccdd"
// (2)  NOP
// ---
// (5)  call  updateInterpreterDispatchGlueSite           ; replaced with "JMP disp32"
// (2)  dw    2-byte glue method helper index
// (4)  dd    cpAddr
// (4)  dd    cpIndex
//
extern "C" int32_t interpretedDispatchGlueDisp32(
   J9Method *ramMethod,
   uint8_t  *callSiteInSnippet)
   {
   TR_RuntimeHelper glueMethodIndex;

   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);

   if (romMethod->modifiers & J9AccNative)
      {
      glueMethodIndex = TR_icallVMprJavaSendNativeStatic;
      }
   else
      {
      // Get unsynchronized glue method index from snippet.
      //
      glueMethodIndex = (TR_RuntimeHelper)(*((uint16_t *)callSiteInSnippet));

      if (romMethod->modifiers & J9AccSynchronized)
         {
         // The glue method runtime helper indices have been arranged successively in pairs
         // to ease the conversion from unsynchronized to synchronized.
         //
         glueMethodIndex = (TR_RuntimeHelper)((int32_t)glueMethodIndex+1);
         }
      }

   // Determine if the glue code can be reached directly from this code cache
   // or whether a trampoline is necessary.
   //
   uint8_t *glueMethodAddress = (uint8_t *)runtimeHelperValue(glueMethodIndex);

   if (!IS_32BIT_RIP(glueMethodAddress, callSiteInSnippet))
      {
      glueMethodAddress = (uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(glueMethodIndex, callSiteInSnippet);
      }

   int32_t disp32 = (int32_t)(glueMethodAddress - callSiteInSnippet);
   return disp32;
   }


// Given a recently resolved RAM method, if there is already a resolved trampoline for
// this method in this code cache then consolidate the resolve/unresolved trampolines.
// Return the disp32 to the dispatch glue from the snippet.
//
extern "C" int32_t adjustTrampolineInterpretedDispatchGlueDisp32(
   J9Method *ramMethod,
   void     *cpAddr,
   int32_t   cpIndex,
   uint8_t  *callSiteInSnippet)
   {
   TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();
   TR::CodeCache *codeCache = manager->findCodeCacheFromPC(callSiteInSnippet);
   if (codeCache)
      {
      codeCache->adjustTrampolineReservation(reinterpret_cast<TR_OpaqueMethodBlock *>(ramMethod), cpAddr, cpIndex);
      }

   return interpretedDispatchGlueDisp32(ramMethod, callSiteInSnippet);
   }


extern "C" void adjustTrampolineInterpretedDispatchGlueDisp32_unwrapper(void **argsPtr, void **resPtr)
   {
   int32_t disp32 = adjustTrampolineInterpretedDispatchGlueDisp32(
        (J9Method *)argsPtr[0],  // ramMethod
                    argsPtr[1],  // cpAddr
(int32_t)(uintptr_t)argsPtr[2],  // cpIndex
         (uint8_t *)argsPtr[3]); // call site

   *resPtr = (void *)(uintptr_t)disp32;
   }


extern "C" void interpretedDispatchGlueDisp32_unwrapper(void **argsPtr, void **resPtr)
   {
   int32_t disp32 = interpretedDispatchGlueDisp32(
      (J9Method *)argsPtr[0],  // ramMethod
       (uint8_t *)argsPtr[1]); // call site

   *resPtr = (void *)(uintptr_t)disp32;
   }
