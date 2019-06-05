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

#include <limits.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/VMJ9.h"

extern "C" int32_t _compareAndSwap(int32_t * addr, uint32_t oldInsn, uint32_t newInsn);
extern "C" int32_t _compareAndSwapSMP(int32_t * addr, uint32_t oldInsn, uint32_t newInsn);
extern void armCodeSync(uint8_t *, uint32_t);
extern uint32_t encodeBranchDistance(uint32_t from, uint32_t to);
uint32_t encodeRuntimeHelperBranchAndLink(TR_RuntimeHelper helper, uint8_t *cursor, TR_FrontEnd *fe);

#define DEBUG_ARM_RECOMP false

static uint32_t
getJitEntryOffset(TR_LinkageInfo * linkageInfo)
   {
   return linkageInfo->getReservedWord() & 0x0ffff;
   }
// Called at runtime to get the body info from the start PC
//
TR_PersistentJittedBodyInfo *J9::Recompilation::getJittedBodyInfoFromPC(void *startPC)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   TR_PersistentJittedBodyInfo *info = linkageInfo->isRecompMethodBody() ? *(TR_PersistentJittedBodyInfo **)((uint8_t*)startPC + OFFSET_METHODINFO_FROM_STARTPC):0;
   return info;
   }

bool J9::Recompilation::isAlreadyPreparedForRecompile(void *startPC)
   {
   int32_t  jitEntryOffset = getJitEntryOffset(TR_LinkageInfo::get(startPC));
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   if (DEBUG_ARM_RECOMP)
      {
      printf("\nmethodPC 0x%x is %salready prepared for recompile\n", startPC, ((*jitEntry & 0x0e000000) == 0x0A000000) ? "" : "not ");
      fflush(stdout);
      }

   return ((*jitEntry & 0x0E000000) == 0x0A000000);  // is jit entry instruction a branch?
   }

// Changes the method in a way to make the current body unreachable, and to a) trigger a recompilation
// or b) to redirect any future threads entering to jump to a freshly compiled body
//
// Used in the following three scenarios
// o when doing sync compilations, this is called by sampleMethod to schedule a compilation
//      on the next invocation.
// o when doing async compilations, this is called after the next compilation finishes
// o when prex assumptions fail and a sync recompilation is required
//
void J9::Recompilation::fixUpMethodCode(void *startPC)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC); //(TR_LinkageInfo *) (((uint8_t*)startPC) + PC_TO_LINKAGE_INFO);
   if (linkageInfo->isCountingMethodBody())
      {
      TR_PersistentJittedBodyInfo *bodyInfo = getJittedBodyInfoFromPC(startPC);
      if(DEBUG_ARM_RECOMP)
         {
         printf("fixUpMethodCode for counting, setting bodyInfo (0x%x) counter to -1\n",bodyInfo);
         fflush(stdout);
         }
      bodyInfo->setCounter(-1);
      }
   else
      {
      // 0:      mov     r4, lr
      // 4:      bl      0x404042b8 <_samplingRecompileMethod>
      // 8:      dd      jittedBodyInfo
      // 12:     dd      space for preserved inst
      // 16:     dd      magic word
      // 24:     ldr     r0, [r7]  startPC (Interpreter entry)
      // 28:     str     r0, [r7]  jitEntry (jitEntryOffset = jitEntry - startPC = 4)

      int32_t  jitEntryOffset = getJitEntryOffset(linkageInfo); //((*linkageInfo)>>16) & 0x0000FFFF;
      int32_t *jitEntry = (int32_t *)((uint8_t*)startPC + jitEntryOffset);
      uint32_t target = (uintptr_t)(jitEntry) + (OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC - jitEntryOffset);
      // pc is 8 bytes of ahead of current instruction so must subtract 8 below
      int32_t  jumpBackInstruction  = 0xEA000000 | encodeBranchDistance((uintptr_t) jitEntry, target);
      int32_t  preserved = *jitEntry;

      if(DEBUG_ARM_RECOMP)
         {
         printf("fixUpMethodCode for sampling, jumpBackInst encoding is 0x%x from jitEntry 0x%x and encoding is 0x%x\n",
                jumpBackInstruction,jitEntry,*jitEntry);fflush(stdout);
         }

      // Other thread might try to do the same thing at the same time.
      while ((preserved & 0x0E000000) != 0x0A000000)
         {
         if (DEBUG_ARM_RECOMP)
            {
            printf("\tjitEntry not already a jump\n");fflush(stdout);
            }

         if (TR::Compiler->target.isSMP())
            {
            if (_compareAndSwapSMP(jitEntry, preserved, jumpBackInstruction))
               {
               if (DEBUG_ARM_RECOMP)
                  {
                  printf("\tCmpAndExchangeSMP passes jitEntry address = 0x%x oldInst = 0%x, newInst = 0x%x\n",jitEntry,preserved,jumpBackInstruction);fflush(stdout);
                  }
               *((int32_t *)((int8_t *)startPC+OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)) = preserved;
               armCodeSync((uint8_t *)jitEntry, 4);
               }
            else
               {
               if (DEBUG_ARM_RECOMP)
                  {
                  printf("\tCmpAndExchangeSMP FAILS jitEntry address = 0x%x oldInst = 0%x, newInst = 0x%x\n",jitEntry,preserved,jumpBackInstruction);fflush(stdout);
                  }
               }
            }
         else // ! TR::Compiler->target.isSMP()
            {
            if (_compareAndSwap(jitEntry, preserved, jumpBackInstruction))
               {
               if (DEBUG_ARM_RECOMP)
                  {
                  printf("\tCmpAndExchange passes jitEntry address = 0x%x oldInst = 0%x, newInst = 0x%x\n",jitEntry,preserved,jumpBackInstruction);fflush(stdout);
                  }
               *((int32_t *)((int8_t *)startPC+OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)) = preserved;
               armCodeSync((uint8_t *)jitEntry, 4);
               }
            else
               {
               if (DEBUG_ARM_RECOMP)
                  {
                  printf("\tCmpAndExchange FAILS jitEntry address = 0x%x oldInst = 0%x, newInst = 0x%x\n",jitEntry,preserved,jumpBackInstruction);fflush(stdout);
                  }
               }
            }
         preserved = *jitEntry;
         }
      }
   }

void J9::Recompilation::methodHasBeenRecompiled(void *oldStartPC, void *newStartPC, TR_FrontEnd *fe)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
   int32_t   bytesToSaveAtStart = 0;
   int32_t   *patchAddr, newInstr;
   intptrj_t distance;
   if (DEBUG_ARM_RECOMP)
      {
      printf("\nmethodHasBeenRecompiled: oldStartPC (0x%x) -> newStartPC (0x%x)\n", oldStartPC, newStartPC);fflush(stdout);
      }

   if (debug("traceRecompilation"))
      {
      ;//diagnostic("RC>>Method successfully recompiled: %p -> %p\n", oldStartPC, newStartPC);
      }

   if (linkageInfo->isCountingMethodBody())
      {
      // Turn the instruction before the counting branch in the
      // counting/profiling prologue into a bl to countingPatchCallSite
      // which expects the new startPC.
      patchAddr = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo) + OFFSET_COUNTING_BRANCH_FROM_JITENTRY - 4);
      newInstr = encodeRuntimeHelperBranchAndLink(TR_ARMcountingPatchCallSite, (uint8_t*)patchAddr, fe);
      if(DEBUG_ARM_RECOMP)
         {
         printf("\tcounting recomp, change instruction location (0x%x) above blt to branch encoding 0x%x (to TR_ARMcountingPatchCallSite)\n",
                  patchAddr,newInstr);fflush(stdout);
         }
      *patchAddr = newInstr;
      armCodeSync((uint8_t *)patchAddr, 4);
      bytesToSaveAtStart = getJitEntryOffset(linkageInfo) + OFFSET_COUNTING_BRANCH_FROM_JITENTRY;
      }
   else
      {
      // Turn the call to samplingMethodRecompile into a call to samplingPatchCallSite
      patchAddr = (int32_t *)((uint8_t *)oldStartPC + OFFSET_SAMPLING_BRANCH_FROM_STARTPC);
      newInstr = encodeRuntimeHelperBranchAndLink(TR_ARMsamplingPatchCallSite, (uint8_t*)patchAddr, fe);
      if(DEBUG_ARM_RECOMP)
         {
         printf("\tsampling recomp, change instruction location (0x%x) of sampling branch to branch encoding 0x%x (to TR_ARMsamplingPatchCallSite)\n",
                  patchAddr,newInstr);fflush(stdout);
         }
      *patchAddr = newInstr;
      armCodeSync((uint8_t *)patchAddr, 4);

      // The order of this code sync sequence is important. Don't try to common them
      // up and get out of order. For sync compilation, the old body must have been
      // fixed up already.
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      if (fej9->isAsyncCompilation())
         fixUpMethodCode(oldStartPC);

      bytesToSaveAtStart = getJitEntryOffset(linkageInfo) + 4;
      }

   bool codeMemoryWasAlreadyReleased = linkageInfo->hasBeenRecompiled(); // HCR - can recompile the same body twice
   linkageInfo->setHasBeenRecompiled();

   // asheikh 2005-05-05: Code Cache Reclamation does not work on ppc
   // without counting method bodies.  _countingPatchCallSite still refers
   // to the methodInfo pointer in the snippet area.
   if (linkageInfo->isSamplingMethodBody() && !codeMemoryWasAlreadyReleased)
      {
      if(DEBUG_ARM_RECOMP)
         {
         printf("\tsampling recomp, releasing code memory oldStartPC = 0x%x, bytesToSaveAtStart = 0x%x\n",oldStartPC, bytesToSaveAtStart);
         fflush(stdout);
         }
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      fej9->releaseCodeMemory(oldStartPC, bytesToSaveAtStart);
      }
   }

uint32_t encodeRuntimeHelperBranchAndLink(TR_RuntimeHelper helper, uint8_t *cursor, TR_FrontEnd *fe)
   {
   intptrj_t target = (intptrj_t)runtimeHelperValue(helper);

   if (!TR::Compiler->target.cpu.isTargetWithinBranchImmediateRange(target, (intptrj_t)cursor))
      {
      target = TR::CodeCacheManager::instance()->findHelperTrampoline(helper, (void *)cursor);

      TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinBranchImmediateRange(target, (intptrj_t)cursor),
                "Helper target address is out of range");
      }
   return 0xEB000000 | encodeBranchDistance((uintptr_t) cursor, (uint32_t)target);
   }

void J9::Recompilation::methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   int32_t  *patchAddr, newInstr, distance;
   TR_PersistentJittedBodyInfo *bodyInfo = getJittedBodyInfoFromPC(oldStartPC);
   TR_PersistentMethodInfo   *methodInfo = bodyInfo->getMethodInfo();

   if (bodyInfo->getUsesPreexistence()  // TODO: reconsider whether this is a race cond for info
       || methodInfo->hasBeenReplaced()
       || (linkageInfo->isSamplingMethodBody() && ! fej9->isAsyncCompilation())) // go interpreted for failed recomps in sync mode
      {
      // Patch the first instruction regardless of counting or sampling
      // TODO: We may need to cross-check with Invalidation to avoid racing cond
      patchAddr = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo));
      distance = OFFSET_REVERT_INTP_FIXED_PORTION - 2*getJitEntryOffset(linkageInfo);
      if (linkageInfo->isCountingMethodBody())
         distance -= 12;
      else
         distance += OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC;

      uint32_t target = (uintptr_t)patchAddr + distance;
      if (DEBUG_ARM_RECOMP)
         {
         printf("oldStartPC %x, patchAddr %x, target %x\n", oldStartPC, patchAddr, target);
         fflush(stdout);
         }
      newInstr = 0xEA000000| encodeBranchDistance((uintptr_t) patchAddr, target); //0b11101010[24bit]
      *patchAddr = newInstr;
      armCodeSync((uint8_t *)patchAddr, 4);

      if (!methodInfo->hasBeenReplaced()) // HCR: VM presumably already has the method in its proper state
         fej9->revertToInterpreted(methodInfo->getMethodInfo());

      }
   else if (linkageInfo->isCountingMethodBody())
      {
      // We can do either of two things: 1) reverse to interpreter; 2) bypass the recomp
      // We may need to decide dynamically.

      // bypass the recomp prologue by replacing the first instruction with
      // a branch to the normal prologue
      int32_t *branchLocation = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo) + OFFSET_COUNTING_BRANCH_FROM_JITENTRY);
      patchAddr = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo));
      uint32_t target = (uintptr_t)patchAddr + OFFSET_COUNTING_BRANCH_FROM_JITENTRY + 4;
      newInstr  = 0xEA000000 | encodeBranchDistance((uintptr_t) patchAddr, target);
      *patchAddr = newInstr;
      armCodeSync((uint8_t *)patchAddr, 4);
      if(DEBUG_ARM_RECOMP)
         {
         printf("MethodCannotBeRecompiled counting recomp, branch over counting prologue by patching inst 0x%x at location 0x%x\n",
                 newInstr,patchAddr);fflush(stdout);
         }
      // Make sure that we do not profile any longer in there
      TR_PersistentProfileInfo *profileInfo = bodyInfo->getProfileInfo();
      if (profileInfo)
         {
         profileInfo->setProfilingFrequency(INT_MAX);
         profileInfo->setProfilingCount(-1);
         }
      }
   else
      {
      // For async compilation, the old method is not fixed up anyway
      if (!fej9->isAsyncCompilation())
         {
         // Restore the original instructions
         // Calculate entry point
         char *startByte = (char *) oldStartPC + getJitEntryOffset(linkageInfo);
         if(DEBUG_ARM_RECOMP)
            {
            printf("MethodCannotBeRecompiled sampling recomp sync compilation restoring preserved jitEntry of 0x%x at location 0x%x\n",
          *((uint32_t*)((uint8_t*)oldStartPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)),startByte);fflush(stdout);
            }
         *((uint32_t*)startByte) = *((uint32_t*)((uint8_t*)oldStartPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC));
         armCodeSync((uint8_t *)startByte, 4);
         }
      }
   linkageInfo->setHasFailedRecompilation();
   }

void J9::Recompilation::invalidateMethodBody(void *startPC, TR_FrontEnd *fe)
   {
   // Pre-existence assumptions for this method have been violated. Make the
   // method no-longer runnable and schedule it for sync recompilation
   //
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   //linkageInfo->setInvalidated();
   TR_PersistentJittedBodyInfo* bodyInfo = getJittedBodyInfoFromPC(startPC);
   bodyInfo->setIsInvalidated(); // bodyInfo must exist

   // If the compilation has been attempted before then we are fine (in case of success,
   // each caller is being re-directed to the new method -- in case if failure, all callers
   // are being sent to the interpreter)
   //
   if (linkageInfo->recompilationAttempted())
      return;
   fixUpMethodCode(startPC);
   }

extern bool armCodePatching(void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra);
extern "C" {
void armIndirectCallPatching_unwrapper(void **argsPtr, void **resPtr)
   {
   armCodePatching(argsPtr[0], argsPtr[1], NULL, NULL, argsPtr[2], argsPtr[3]);
   }
}

#if defined(TR_HOST_ARM)
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   int32_t  jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   if (linkageInfo->isSamplingMethodBody())
      {
      *(void **)((int8_t *)startPC + OFFSET_METHODINFO_FROM_STARTPC) = bodyInfo;
      }
   else if (linkageInfo->isCountingMethodBody()) {
      TR_ASSERT(0, "Counting method body\n");
      }
   return;
   }
#endif
