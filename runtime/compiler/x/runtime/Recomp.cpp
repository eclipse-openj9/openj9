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

#include "x/codegen/X86Recompilation.hpp"

#include <limits.h>
#include <stdint.h>
#include "codegen/PrivateLinkage.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"
#include "x/runtime/X86Runtime.hpp"
#include "env/VMJ9.h"
#include "control/CompilationRuntime.hpp"

#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
#define IS_32BIT_RIP(x,rip)  ((intptr_t)(x) == (intptr_t)(rip) + (int32_t)((intptr_t)(x) - (intptr_t)(rip)))

extern "C" void mcc_callPointPatching_unwrapper(void **argsPtr, void *resPtr);

#endif


#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
// AMD64 specific version of the call point patching that checks for patching of IPICDispatch calls
// before forwarding to the common callPointPatching code.
//
extern "C" void mcc_AMD64callPointPatching_unwrapper(void **argsPtr, void **resPtr)
   {
   TR_OpaqueMethodBlock *method   = reinterpret_cast<TR_OpaqueMethodBlock *>(argsPtr[0]);
   uint8_t *callSite              = (uint8_t *)argsPtr[1];
   J9VMThread *vmThread           = (J9VMThread*)argsPtr[3];
   uint8_t *oldStartPC            = (uint8_t*)argsPtr[4];
   uint8_t *oldJitEntry           = oldStartPC + jitEntryOffset(oldStartPC);

   static char *traceAMD64CallPointPatching = feGetEnv("TR_traceAMD64CallPointPatching");

   int32_t expectedDisp32 = (int32_t)(oldJitEntry - (callSite+5));
   int32_t actualDisp32 = *((int32_t *)(callSite+1));

   uint8_t *trampoline = NULL;

   if (expectedDisp32 != actualDisp32)
      {
      // Maybe it's a call through a trampoline.
      //
      static char *alwaysUseTrampolines = feGetEnv("TR_AlwaysUseTrampolines");
      if (!IS_32BIT_RIP(oldJitEntry, (intptr_t)(callSite+5)) || alwaysUseTrampolines)
         {
         trampoline = (uint8_t*)TR::CodeCacheManager::instance()->findMethodTrampoline(method, callSite);
         }
      if (!trampoline)
         {
         if (traceAMD64CallPointPatching)
            {
            fprintf(stderr, "AMD64 NOT PATCHING: Call %p does not target method %p (startPC %p) and there is no trampoline\n",
               callSite, oldJitEntry, oldStartPC);
            }
         return;
         }

      int32_t trampolineDisp32 = (int32_t)(trampoline - (callSite+5));
      if (trampolineDisp32 != actualDisp32)
         {
         if (traceAMD64CallPointPatching)
            {
            fprintf(stderr, "AMD64 NOT PATCHING: Call %p does not target method %p (startPC %p) nor its trampoline %p\n",
               callSite, oldJitEntry, oldStartPC, trampoline);
            }
         return;
         }
      }

   if (0 && traceAMD64CallPointPatching)
      {
      char buf[1000];
      TR_J9VMBase *fej9 = TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
      fej9->printTruncatedSignature(buf, sizeof(buf), method);
      fprintf(stderr, "AMD64 PATCHING: Proceeding to patch call instruction %p (trampoline %p) in %s\n", callSite, trampoline, buf);
      }
   mcc_callPointPatching_unwrapper(argsPtr, resPtr);
   }
#endif


// Called at runtime to get the method info from the start PC
//
TR_PersistentJittedBodyInfo *J9::Recompilation::getJittedBodyInfoFromPC(void *startPC)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      TR_ASSERT(TR::comp(), "Must be used during compilation when calling getJittedBodyInfoFromPC on the server");
      stream->write(JITServer::MessageType::Recompilation_getJittedBodyInfoFromPC, startPC);
      auto recv = stream->read<std::string, std::string>();
      auto &bodyInfoStr = std::get<0>(recv);
      auto &methodInfoStr = std::get<1>(recv);
      return J9::Recompilation::persistentJittedBodyInfoFromString(bodyInfoStr, methodInfoStr, TR::comp()->trMemory());
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   // The body info pointer is stored in the pre-prologue of the method. The
   // location of the field depends upon the type of the method header.  Use the
   // header-type bits in the linkage info fields to determine what kind of header
   // this is.
   //
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   return linkageInfo->isRecompMethodBody() ?
      *(TR_PersistentJittedBodyInfo **)((uint8_t*)startPC + START_PC_TO_METHOD_INFO_ADDRESS) :
      NULL;
   }

// This method should only be called for methods compiled for sampling
//
bool J9::Recompilation::isAlreadyPreparedForRecompile(void *startPC)
   {
   uint16_t *startBytes = (uint16_t*)((uint8_t*)startPC + jitEntryOffset(startPC));
   return *startBytes == jitEntryJmpInstruction(startPC, START_PC_TO_RECOMPILE_SAMPLING);
   }

// Changes the method in a way to make the current body unreachable, and to a) trigger a recompilation
// or b) to redirect any future threads entering to jump to a freshly compiled body
//
// Used in the following three scenarios
// o when doing sync compilations, this is called by sampleMethod to schedule a compilation
//      on the next invocation.
// o when doing async compilations, this is called after the next compilation finishes
//    (oldMethodBody must be a sampling body)
// o when prex assumptions fail and a sync recompilation is required
//
void J9::Recompilation::fixUpMethodCode(void *startPC)
   {
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   if (linkageInfo->isCountingMethodBody())
      {
      TR_PersistentJittedBodyInfo   *bodyInfo = getJittedBodyInfoFromPC(startPC);
      bodyInfo->setCounter(-1);
      }
   else
      {
      // Atomically change the first instruction of the method to jmp back to the
      // call instruction (when called from sampleMethod or invalidateMethodBody,
      // the call is currently going to samplingRecompileMethod.  when called at
      // the end of the async recompile, the call is going to samplingPatchCallSite)
      //
      // This code will race with switchToInterpreter (NOTE2 in this file) if the following is not atomic
      //
      uint16_t newInstruction = jitEntryJmpInstruction(startPC, START_PC_TO_RECOMPILE_SAMPLING);
      uint16_t oldInstruction = *(uint16_t*)((uint8_t*)startPC + START_PC_TO_ORIGINAL_ENTRY_BYTES);
      uint16_t *startBytes = (uint16_t*)((uint8_t*)startPC + jitEntryOffset(startPC));
      AtomicCompareAndSwap(startBytes, oldInstruction, newInstruction);
      }
   }

void J9::Recompilation::methodHasBeenRecompiled(void *oldStartPC, void *newStartPC, TR_FrontEnd *fe)
   {
   char *startByte = (char*)oldStartPC + jitEntryOffset(oldStartPC);
   char *p;
   int32_t offset, bytesToSaveAtStart;
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(oldStartPC);
   if (linkageInfo->isCountingMethodBody())
      {
      // The start of the old method looks like on IA32:
      //  when not profiling:
      //    SUB   [counterAddress], 1    (always 7 bytes)
      //    JL    recompilationSnippet   (always 6 bytes)
      //  when profiling
      //    CMP   [counterAddress], 0    (always 7 bytes)
      //    JL    recompilationSnippet   (always 6 bytes)
      //
      // It is changed to:
      //    CALL  patchCallSite          (5 bytes)
      //    DB    ??                     (2 byte offset to oldStartPC)
      //    JL    recompilationSnippet   (always 6 bytes)
      //
      // The patchCallSite code is aware of the sequence above and can deduce
      // the absolute new start address from the method info in the pre-header
      //
      // A spin loop is introduced while we change the first instruction.
      //
      // On AMD64, the initial sequence is:
      //    MOV     rdi, counterAddress  (10 bytes)
      //    SUB/CMP [rdi], 0/1           (3 bytes)
      //    JL      recompilationSnippet (6 bytes)
      //
      // After patching:
      //    CALL  patchCallSite          (5 bytes)
      //    DB    ??                     (8 bytes of junk)
      //    JL    recompilationSnippet   (6 bytes)
      //
      intptr_t helperAddr = (intptr_t)runtimeHelperValue(COUNTING_PATCH_CALL_SITE);

#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
      if (!IS_32BIT_RIP(helperAddr, (intptr_t)(startByte+5)))
         {
         helperAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(COUNTING_PATCH_CALL_SITE, startByte);
         }
#endif

      offset = ((char*)helperAddr) - startByte - 5;

      *((int16_t*)startByte) = SPIN_LOOP_INSTRUCTION;
      patchingFence16(startByte);
      *((int32_t*)(startByte+2)) = offset >> 8;

      // Offset from CALL's return address to oldStartPC
      // This is only strictly necessary on AMD64.
      //
      *((int16_t*)(startByte+5)) = ((startByte+5) - (char*)oldStartPC);

      // Finish patching and unlock spin loop
      //
      patchingFence16(startByte);
      *((int16_t*)startByte) = ((offset & 0xFF) << 8) | CALL_INSTRUCTION;

      bytesToSaveAtStart = 7 + jitEntryOffset(oldStartPC); // the new call instruction + 2-byte offset; TODO: this could be 5 on IA32
      }
   else
      {
      // The pre-prologue of the old method looks like on IA32:
      //    CALL  _samplingRecompileMethod
      //    DD    persistentMethodInfo
      //
      // It is changed to:
      //    CALL  _samplingPatchCallSite
      //    DD    persistentMethodInfo
      //
      // and the first instruction of the method is changed into a jump to this
      // instruction.
      //
      p = (char*)oldStartPC + START_PC_TO_RECOMPILE_SAMPLING + 1; // the immediate field of the call

      intptr_t helperAddr = (intptr_t)runtimeHelperValue(SAMPLING_PATCH_CALL_SITE);

#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
      if (!IS_32BIT_RIP(helperAddr, (intptr_t)(p+4)))
         {
         helperAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(SAMPLING_PATCH_CALL_SITE, p);
         }
#endif

      offset = ((char*)helperAddr) - p - 4;

      *((uint32_t*)p) = offset;

      // With guarded counting recompilations, we need to fix up the method code regardless of whether it is
      // synchronous or asynchronous
      fixUpMethodCode(oldStartPC);

      bytesToSaveAtStart = 2 + jitEntryOffset(oldStartPC); // the jmp instruction
      }

   bool codeMemoryWasAlreadyReleased = linkageInfo->hasBeenRecompiled(); // HCR - can recompile the same body twice
   linkageInfo->setHasBeenRecompiled();

   if (!linkageInfo->isCountingMethodBody() && !codeMemoryWasAlreadyReleased)
      {
      // Now tell the VM that the method has been uncommitted - and the code memory
      // allocated can be reused
      //
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      fej9->releaseCodeMemory(oldStartPC, bytesToSaveAtStart);
      }
   }

void J9::Recompilation::methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe)
   {
   char *startByte = (char*)oldStartPC + jitEntryOffset(oldStartPC);
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(oldStartPC);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR_ASSERT( linkageInfo->isSamplingMethodBody() && !linkageInfo->isCountingMethodBody() ||
          !linkageInfo->isSamplingMethodBody() &&  linkageInfo->isCountingMethodBody(),
          "Jitted body must be either sampling or counting");
   bool usesSampling = linkageInfo->isSamplingMethodBody();
   TR_PersistentJittedBodyInfo *bodyInfo = getJittedBodyInfoFromPC(oldStartPC);
   TR_PersistentMethodInfo   *methodInfo = bodyInfo->getMethodInfo();

   if (bodyInfo->getUsesPreexistence()
       || methodInfo->hasBeenReplaced()
       || (usesSampling && ! fej9->isAsyncCompilation()) // go interpreted for failed recomps in sync mode
       || methodInfo->isExcludedPostRestore()) // go interpreted if method is excluded post restore
      {
      // We need to switch the method to interpreted.  Change the first instruction of the
      // method to jump back to the call to the interpreter dispatch
      //
      // NOTE2: this races with fixupMethodCode.  Once the following occurs, fixupMethodCode
      // is prevented from succeeding.  To this end, fixupMethodCode uses an atomic
      // compareAndExchange operation, which will fail if the following has already occurred.
      //
      int32_t startPCToTarget = usesSampling? START_PC_TO_ITR_GLUE_SAMPLING : START_PC_TO_ITR_GLUE_COUNTING;
      replaceFirstTwoBytesWithShortJump(oldStartPC, startPCToTarget);

      // Now tell the VM that the method needs to be switched back to Interpreted
      //
      if (!methodInfo->hasBeenReplaced()) // HCR: VM presumably already has the method in its proper state
         fej9->revertToInterpreted(methodInfo->getMethodInfo());
      }
   else if (usesSampling)
      {
      // Restore the original first 2 bytes of the method
      //
      replaceFirstTwoBytesWithData(oldStartPC, START_PC_TO_ORIGINAL_ENTRY_BYTES);
      }
   else
      {
      // Change the method prologue to skip the counting prologue:
      // Before (when not profiling)
      //     SUB    [counterAddress], 1    (always 7 bytes)
      //     JL     recompilationSnippet   (always 6 bytes)
      // when profiling:
      //     CMP    [counterAddress], 0    (always 7 bytes)
      //     JL     recompilationSnippet   (always 6 bytes)
      //
      // It is changed to:
      //     JMP    Lstart                 (2 bytes)
      //     DB[5]  ??                     (5 bytes garbage)
      //     JL     recompilationSnippet   (always 6 bytes)
      // Lstart:
      //
      *((int16_t*)startByte) = TWO_BYTE_JUMP_INSTRUCTION | ((COUNTING_PROLOGUE_SIZE-2) << 8);

      // Make sure we do not profile any longer in there
      TR_PersistentProfileInfo *profileInfo = bodyInfo->getProfileInfo();
      if (profileInfo)
         {
         profileInfo->setProfilingFrequency(INT_MAX);
         profileInfo->setProfilingCount(-1);
         }
      }

   linkageInfo->setHasFailedRecompilation();
   }

void J9::Recompilation::invalidateMethodBody(void *startPC, TR_FrontEnd *fe)
   {
   // Preexistence assumptions for this method have been violated.  Make the
   // method no-longer runnable and schedule it for a sync recompilation
   //
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   //linkageInfo->setInvalidated();
   TR_PersistentJittedBodyInfo* bodyInfo = getJittedBodyInfoFromPC(startPC);
   bodyInfo->setIsInvalidated(); // bodyInfo must exist

   // If the compilation has been attempted before then we are fine (in case of success,
   // each caller is being re-directed to the new method -- in case if failure, all callers
   // are being sent to the interpreter)
   //
   if (linkageInfo->recompilationAttempted())
      return;
   fixUpMethodCode(startPC); // schedule a sync compilation
   }

#if defined(TR_HOST_X86)
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo)
   {
   *((void **)((U_8*)startPC+START_PC_TO_METHOD_INFO_ADDRESS)) = bodyInfo;
   }
#endif
