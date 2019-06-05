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

#include "z/codegen/S390Recompilation.hpp"

#include <limits.h>
#include <stdint.h>
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "z/codegen/SystemLinkage.hpp"

// Recompilation Support Runtime methods
//
// Methods headers look different based on the type of compilation: there are
// inherent differences between sampling and counting compilations and yet
// again between counting with profiling and counting without profiling headers
//
// The linkage info field contains information about what kind of compilation
// produced this method.
//
// Sampling Compilation
// ====================
// If preexistence was not performed on this method, the following is what the
// prologue looks like:
//
//  64   32
//
// -32  -24 ST r14,4(r5); save link register
// -28  -20 BASR r4,0 ; establish r4
// -26  -18 L r4,8(r4)
// -22  -14 BASR r14,r4 ->  _samplingRecompiledMethod
// -20  -12 DC <address of _samplingRecompiledMethod>
// -12  -8  DC <jitted body info address>
// -4   -4  method linkage info and flags
//  0    0  1st instruction of method (must be at least 4 bytes)--will turn into BRC

#define DEBUGIT (1==0)

// Compare and swap a 4 byte object need to implement uses CS
extern "C" int32_t _CompareAndSwap4(int32_t * addr, uint32_t oldInsn, uint32_t newInsn);

extern "C" void
s390compareAndExchange4(int32_t * addr, uint32_t oldInsn, uint32_t newInsn)
   {
   // needs to be changed to double word check later ...
#if defined(TR_HOST_64BIT)
   TR_ASSERT((uint64_t(addr) % 8) == 0, "%p not on a word boundary\n", addr);
#else
   TR_ASSERT((uint32_t(addr) % 4) == 0, "%p not on a word boundary\n", addr);
#endif
   // guard so can cross compile
   _CompareAndSwap4(addr, oldInsn, newInsn);
   }

static uint32_t
getJitEntryOffset(TR_LinkageInfo * linkageInfo)
   {
   return linkageInfo->getReservedWord() & 0x0ffff;
   }

// Called at runtime to get the method info from the start PC
//
TR_PersistentJittedBodyInfo *
J9::Recompilation::getJittedBodyInfoFromPC(void * startPC)
   {
   TR_ASSERT(startPC, "startPC is null");
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(startPC);
   if (!linkageInfo->isRecompMethodBody())
      {
      return NULL;
      }

   int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t * jitEntry = (int32_t *) ((int8_t *) startPC + jitEntryOffset);
   TR_PersistentJittedBodyInfo * info = NULL;

   if (false && DEBUGIT)
      {
      printf("calling getmethodinfo %x, jitEntryOffset %d jitEntry %x linkageInfo %x\n", startPC, jitEntryOffset, jitEntry,
         linkageInfo);
      }

   info = (*(TR_PersistentJittedBodyInfo * *) ((int8_t *) startPC + -(sizeof(uint32_t) + sizeof(intptrj_t))));

   if (false && DEBUGIT)
      {
      printf("info is %x\n", info);
      }

   return info;
   }

// Only called when sampling
bool
J9::Recompilation::isAlreadyPreparedForRecompile(void * startPC)
   {
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(startPC);
   int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t * jitEntry = (int32_t *) ((uint8_t *) startPC + jitEntryOffset);
   uint32_t startInsn = *jitEntry;

   int32_t jumpSize = -OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_TRAMPOLINE - jitEntryOffset;
   uint32_t jumpBackInsn = (0x0000ffff & (jumpSize / 2)) | FOUR_BYTE_JUMP_INSTRUCTION;
   if (DEBUGIT)
      {
      printf("MethodPC 0x%x is %sprepared for recompile\n", startPC, startInsn == jumpBackInsn ? "" : "not ");
      }
   return startInsn == jumpBackInsn;
   }


// Changes the method in a way to make the current body unreachable, and to a) trigger a recompilation
// or b) to redirect any future threads entering to jump to a freshly compiled body
//
// Used in the following three scenarios
// o when doing sync compilations, this is called by sampleMethod to schedule a compilation
//      on the next invocation.
// o when doing async compilations, this is called after the next compilation finishes
//    (oldMethodBody must be a sampling body)
// o when prev assumptions fail and a sync recompilation is required
//
void
J9::Recompilation::fixUpMethodCode(void * startPC)
   {
#if defined(TR_HOST_64BIT)
   const bool is64Bit = true;
#else
   const bool is64Bit = false;
#endif
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(startPC);
   if (DEBUGIT)
      {
      printf("fixup: 0x%p %s %s\n", linkageInfo, linkageInfo->isSamplingMethodBody() ? "samplingSet" : "",
         linkageInfo->isCountingMethodBody() ? "CountingSet" : "");
      }

   if (linkageInfo->isCountingMethodBody())
      {
      TR_PersistentJittedBodyInfo * bodyInfo = getJittedBodyInfoFromPC(startPC);
      bodyInfo->setCounter(-1);
      }
   else
      {
      // Will overwrite the first instruction of the body--this should be a store of the link reg
      // ie ST r14,4(r5).  Note that there will be preceding loads for the args for the interpreter
      // entry point and these vary depending on # args, hence add adjustment for jitEntryOffset
      int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
      int32_t * jitEntry = (int32_t *) ((uint8_t *) startPC + jitEntryOffset);
      int32_t preserved = *jitEntry;
      int32_t jumpSize = -OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_TRAMPOLINE - jitEntryOffset;
      uint32_t jumpBackInsn = (0x0000ffff & (jumpSize / 2)) | FOUR_BYTE_JUMP_INSTRUCTION;

      // Atomically change the first instruction of the method to jmp back to the
      // call instruction (when called from sampleMethod or invalidateMethodBody,
      // the call is currently going to samplingRecompileMethod.  when called at
      // the end of the async recompile, the call is going to samplingPatchCallSite)
      //

      // if already a jump, then we've lost the race
      if ((0xffff0000 & preserved) == FOUR_BYTE_JUMP_INSTRUCTION)
         {
         if (DEBUGIT)
            {
            printf("%p: already modified--do not attempt to fixup\n", startPC);
            }
         return;
         }

      // This code will race with switchToInterpreter (NOTE2 in this file) if the following is not atomic
      s390compareAndExchange4(jitEntry, preserved, jumpBackInsn);

      if (DEBUGIT)
         {
         printf("%p: modified %p from %p to %p, will jump to %p(%x)\n", startPC, jitEntry, preserved, jumpBackInsn,
            (char *) jitEntry + jumpSize, *(int *) ((char *) jitEntry + jumpSize));
         }


      if (debug("traceRecompilation"))
         {
         ;//diagnostic("RC>>%p: modified %p from %p to %p, will jump to %p(%x)\n", startPC, jitEntry, preserved, jumpBackInsn, (char *) jitEntry + jumpSize, *(int *) ((char *) jitEntry + jumpSize));
         }
      }
   }

void
J9::Recompilation::methodHasBeenRecompiled(void * oldStartPC, void * newStartPC, TR_FrontEnd * fe)
   {
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(oldStartPC);
   intptrj_t distance, * patchAddr, newInstr, * codePtrAddr;
   int32_t bytesToSaveAtStart;
   int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);

   if (DEBUGIT)
      {
      printf("Method successfully recompiled: %x -> %x\n", oldStartPC, newStartPC);
      }
#if defined(TR_HOST_64BIT)
   const bool is64Bit = true;
#else
   const bool is64Bit = false;
#endif

   if (debug("traceRecompilation"))
      {
      ;//diagnostic("RC>>Method successfully recompiled: %p -> %p\n", oldStartPC, newStartPC);
      }

   if (linkageInfo->isCountingMethodBody())
      {
      // The prologue looks something like
      //
      //   AHI r14,-3*regSize
      //   STM r1,r2,regSize(sp)
      //   ...
      // Label1:
      //   ST r14,4(r14) <- patching code
      //   ...
      //
      //  will patch 1st instr with a jump to the patching code
      // so it will look like:
      //   BR Label1
      //   STM r1,r2,regSize(sp)
      //   ...
      // Label1:
      //   ST r14,4(r14) <- patching code
      //   ...

      int32_t * patchInsnAddr;
      patchInsnAddr = (int32_t *) ((uint8_t *) oldStartPC + jitEntryOffset);
      if (debug("traceRecompilation"))
         {
         ;//diagnostic("Patching counting prologue, starting at %p(%x)\n", patchInsnAddr, *patchInsnAddr);
         }

      if (DEBUGIT)
         {
         printf("Patching counting prologue, starting at %p (%x)\n", patchInsnAddr, *patchInsnAddr);
         }

      int32_t patchCodeOffset = OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_START;
      *patchInsnAddr = FOUR_BYTE_JUMP_INSTRUCTION | ((patchCodeOffset / 2) & 0x0000ffff);

      bytesToSaveAtStart = jitEntryOffset + OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_END;
      }
   else
      {
      intptrj_t oldAsmAddr = 0;
      intptrj_t newAsmAddr = 0;
      // Turn the call from samplingMethodRecompile to a call to samplingPatchCallSite
      // basically, the address following the BASR gets patched
      // BASR R4,0
      //  L R4,8(R4)
      //  BASR r14,r4
      //  DC <old addressToRecompile>  <==== DC<addr to patch asm routine>
      int32_t startPCToAsmOffset = OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_ADDRESS;
      int32_t requiredPad; // pad can vary between methods, so must calculate each time

      requiredPad = (startPCToAsmOffset + jitEntryOffset) % sizeof(intptrj_t);
      if (requiredPad)
         {
         requiredPad = sizeof(intptrj_t) - requiredPad;
         }

      patchAddr = (intptrj_t *) ((uint8_t *) oldStartPC + -(startPCToAsmOffset + requiredPad));
      oldAsmAddr = *patchAddr;
      newAsmAddr = (intptrj_t) runtimeHelpers.getFunctionEntryPointOrConst(TR_S390samplingPatchCallSite);

      if (debug("traceRecompilation"))
         {
         ;//diagnostic("RC>>Attempted to patch %p from %p to %p...\n", patchAddr, oldAsmAddr, newAsmAddr);
         }

      if (DEBUGIT)
         {
         printf("Attempting to patch %p from %p to %p...\n", patchAddr, oldAsmAddr, newAsmAddr);
         }

#if defined(TR_HOST_64BIT)
      TR_ASSERT(0 == ((uint64_t) patchAddr % sizeof(intptrj_t)), "Address %p is not word aligned\n", patchAddr);
#else
      TR_ASSERT(0 == ((uint32_t) patchAddr % sizeof(intptrj_t)), "Address %p is not word aligned\n", patchAddr);
#endif
      *patchAddr = newAsmAddr;

      // save jump at beginning
      bytesToSaveAtStart = TR::InstOpCode::getInstructionLength(TR::InstOpCode::BRC) + jitEntryOffset;

      // The order of this code sync sequence is important. Don't try to common them
      // up and get out of order. For sync compilation, the old body must have been
      // fixed up already.
      // update: the above is no longer true when guarded counting recompilations is on.
      fixUpMethodCode(oldStartPC);
      }

   bool codeMemoryWasAlreadyReleased = linkageInfo->hasBeenRecompiled(); // HCR - can recompile the same body twice
   linkageInfo->setHasBeenRecompiled();

   // Now tell the VM that the method has been uncommitted - and the code memory
   // allocated can be reused
   //
   if (!codeMemoryWasAlreadyReleased)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      fej9->releaseCodeMemory(oldStartPC, bytesToSaveAtStart);
      }

   }

// For whatever reason, method cannot be recompiled, so reduce recompilation
// related overhead and remove checks
void
J9::Recompilation::methodCannotBeRecompiled(void * oldStartPC, TR_FrontEnd * fe)
   {
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(oldStartPC);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   int32_t * patchAddr, distance;
   TR_ASSERT( linkageInfo->isSamplingMethodBody() && !linkageInfo->isCountingMethodBody() ||
          !linkageInfo->isSamplingMethodBody() &&  linkageInfo->isCountingMethodBody(),
          "Jitted body must be either sampling or counting");
   TR_PersistentJittedBodyInfo * bodyInfo = getJittedBodyInfoFromPC(oldStartPC);
   TR_PersistentMethodInfo * methodInfo = bodyInfo->getMethodInfo();

#if defined(TR_HOST_64BIT)
   const bool is64Bit = true;
#else
   const bool is64Bit = false;
#endif

   if (DEBUGIT)
      {
      printf("Cannot recompile method @ %p\n", oldStartPC);
      }

   if (debug("traceRecompilation"))
      {
      ;//diagnostic("RC>> Cannot recompile method @ %p\n", oldStartPC);
      }

   patchAddr = (int32_t *) ((uint8_t *) oldStartPC + getJitEntryOffset(linkageInfo));

   if (bodyInfo->getUsesPreexistence()
       || methodInfo->hasBeenReplaced()
       || (linkageInfo->isSamplingMethodBody() && ! fej9->isAsyncCompilation())) // go interpreted for failed recomps in sync mode
      {
      bool usesSampling = linkageInfo->isSamplingMethodBody();
      // We need to switch the method to interpreted.  Change the first instruction of the
      // method to jump back to the call to the interpreter dispatch
      //
      // NOTE2: this races with fixupMethodCode.  Once the following occurs, fixupMethodCode
      // is prevented from succeeding.  To this end, fixupMethodCode uses an atomic
      // compareAndExchange operation, which will fail if the following has already occurred.
      uint32_t offset = OFFSET_INTEP_VM_CALL_HELPER_TRAMPOLINE;

      offset -= getJitEntryOffset(linkageInfo);

      *patchAddr = FOUR_BYTE_JUMP_INSTRUCTION | ((offset / 2) & 0x0000ffff);

      // Now tell the VM that the method needs to be switched back to Interpreted
      //
      if (!methodInfo->hasBeenReplaced()) // HCR: VM presumably already has the method in its proper state
         fej9->revertToInterpreted(methodInfo->getMethodInfo());

      if (DEBUGIT)
         {
         printf("Reverted to interpreted method\n");
         }

      if (debug("traceRecompilation"))
         {
         ;//diagnostic("Reverted to interpreted method\n");
         }
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      /*
      The counting prologue looks like this
      AHI r14,-3*regSize  # reserve 3 slots for r1,r2, and resume addr
      STM r1,r2,regSize(sp)
      LARL r1,<offset to methodInfo1 above > or BASR r1,0;L r1,<off to MI2 below>(r1)
      L r2,0(r1)
      LTR R2,R2
      BGE LabelResume
      BASR r2,0
      AHI r2,<offset to LabelResume>
      ST  r2,0(sp) # store resume point
      AHI r2,-addressSize (assume to be regSize)
      L r2,0(r2)
      BR 2
      DC <address of asm snippet>
      DC <methodInfo addr> # only on G5 hardware
        ...
      LabelResume
      ...
      overwrite AHI with a jump past all this to the original prologue:
      JL LabelResume
      STM r1,r2,regSize(sp)
      ...
      LabelResume
      ...
      */
      uint32_t offset;
      offset = COUNTING_PROLOGUE_SIZE;

      *patchAddr = FOUR_BYTE_JUMP_INSTRUCTION | ((offset / 2) & 0x0000ffff);


      TR_PersistentProfileInfo * profileInfo = bodyInfo->getProfileInfo();
      if (profileInfo)
         {
         profileInfo->setProfilingFrequency(INT_MAX);
         profileInfo->setProfilingCount(-1);
         }
      }
   else
      {
      if (DEBUGIT)
         {
         printf("Zeroed out method info for 0x%x\n", oldStartPC);
         }
      // For async compilation, the old method is not fixed up anyway
      if (!fej9->isAsyncCompilation())
         {
         uint32_t prevValue = *patchAddr;
         restoreJitEntryPoint((uint8_t*)oldStartPC, (uint8_t*)patchAddr);
         if (debug("traceRecompilation"))
            {
            uint32_t * saveLocn = (uint32_t*)((uint8_t*)oldStartPC + OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION);


            ;//diagnostic("RC>>Jit entry point %p(%x->%x) restored saveLocn %p(%x)\n",patchAddr,*patchAddr, prevValue,saveLocn,*saveLocn);
            }
         }
      }
   linkageInfo->setHasFailedRecompilation();
   }

void
J9::Recompilation::invalidateMethodBody(void * startPC, TR_FrontEnd * fe)
   {
   if (debug("traceRecompilation"))
      {
      ;//diagnostic("RC>>Invalidating %p\n", startPC);
      }

   // Pre-existence assumptions for this method have been violated. Make the
   // method no-longer runnable and schedule it for sync recompilation
   //
   TR_LinkageInfo * linkageInfo = TR_LinkageInfo::get(startPC);
   //linkageInfo->setInvalidated();
   TR_PersistentJittedBodyInfo* bodyInfo = getJittedBodyInfoFromPC(startPC);
   bodyInfo->setIsInvalidated(); // bodyInfo must exist

   // If the compilation has been attempted before then we are fine (in case of success,
   // each caller is being re-directed to the new method -- in case if failure, all callers
   // are being sent to the interpreter)
   //
   if (linkageInfo->recompilationAttempted())
      {
      return;
      }
   fixUpMethodCode(startPC);
   }
