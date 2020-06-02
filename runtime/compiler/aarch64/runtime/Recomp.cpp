/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/PrivateLinkage.hpp"
#include "control/Recompilation.hpp"
#include "env/VMJ9.h"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"

extern void arm64CodeSync(uint8_t *, uint32_t);

#define DEBUG_ARM64_RECOMP false

#define B_INSTR_MASK 0xFC000000
#define B_COND_INSTR_MASK 0xFF00001F

static int32_t encodeDistanceInBranchInstruction(TR::InstOpCode::Mnemonic op, intptr_t distance)
   {
   TR_ASSERT(op == TR::InstOpCode::b || op == TR::InstOpCode::bl, "Unexpected InstOpCode");
   TR_ASSERT_FATAL(distance <= TR::Compiler->target.cpu.maxUnconditionalBranchImmediateForwardOffset() &&
                   distance >= TR::Compiler->target.cpu.maxUnconditionalBranchImmediateBackwardOffset(),
                   "Target address is out of range");
   return TR::InstOpCode::getOpCodeBinaryEncoding(op) | ((distance >> 2) & 0x3ffffff); /* imm26 */
   }

// Called at runtime to get the body info from the start PC
//
TR_PersistentJittedBodyInfo *J9::Recompilation::getJittedBodyInfoFromPC(void *startPC)
   {
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);
   TR_PersistentJittedBodyInfo *info = NULL;

   if (linkageInfo->isSamplingMethodBody())
      {
      info = *(TR_PersistentJittedBodyInfo **)((int8_t *)startPC + OFFSET_SAMPLING_METHODINFO_FROM_STARTPC);
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      TR_UNIMPLEMENTED();
      }
   return info;
   }

bool J9::Recompilation::isAlreadyPreparedForRecompile(void *startPC)
   {
   int32_t jitEntryOffset = getJitEntryOffset(J9::PrivateLinkage::LinkageInfo::get(startPC));
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   return ((*jitEntry & B_INSTR_MASK) == TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b));  // is jit entry instruction a branch?
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
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   if (linkageInfo->isCountingMethodBody())
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      // Preprologue
      // -24: mov	x8, lr  <= Branch back to this instruction
      // -20: bl	_samplingRecompileMethod
      // -16: .dword	jittedBodyInfo
      //  -8: .word	space for preserving original jitEntry instruction
      //  -4: .word	magic word
      //   0: ldr	x0, [J9SP]  startPC (entry from interpreter)
      //  ...
      //   n: str	x0, [J9SP]  jitEntry (= startPC + jitEntryOffset) <= Patch this instruction

      int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
      int32_t *jitEntry = (int32_t *)((uint8_t *)startPC + jitEntryOffset);
      int32_t distance = OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC - jitEntryOffset;
      int32_t newInstr = encodeDistanceInBranchInstruction(TR::InstOpCode::b, distance);
      int32_t preserved = *jitEntry;

      if (DEBUG_ARM64_RECOMP)
         {
         printf("fixUpMethodCode for sampling, newInstr encoding is 0x%x from jitEntry %p and encoding is 0x%x\n",
                newInstr, jitEntry, *jitEntry); fflush(stdout);
         }

      // Other thread might try to do the same thing at the same time.
      while ((preserved & B_INSTR_MASK) != TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b))
         {
#if defined(__GNUC__)
         if (__sync_bool_compare_and_swap(jitEntry, preserved, newInstr)) // GCC built-in function
            {
            arm64CodeSync((uint8_t *)jitEntry, 4);
            *((int32_t *)((int8_t *)startPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)) = preserved;
            if (DEBUG_ARM64_RECOMP)
               {
               printf("\tCmpAndSwap passes: jitEntry address = %p oldInstr = 0x%x, newInstr = 0x%x\n",
                      jitEntry, preserved, newInstr); fflush(stdout);
               }
            }
         else
            {
            if (DEBUG_ARM64_RECOMP)
               {
               printf("\tCmpAndSwap fails: jitEntry address = %p oldInstr = 0x%x, newInstr = 0x%x\n",
                      jitEntry, preserved, newInstr); fflush(stdout);
               }
            }
#else
#error Not supported yet
#endif
         preserved = *jitEntry;
         }
      }
   }

void J9::Recompilation::methodHasBeenRecompiled(void *oldStartPC, void *newStartPC, TR_FrontEnd *fe)
   {
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(oldStartPC);
   int32_t bytesToSaveAtStart = 0;
   int32_t *patchAddr, newInstr;
   intptr_t helperAddress;

   if (DEBUG_ARM64_RECOMP)
      {
      printf("\nmethodHasBeenRecompiled: oldStartPC (%p) -> newStartPC (%p)\n", oldStartPC, newStartPC); fflush(stdout);
      }

   if (linkageInfo->isCountingMethodBody())
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      // Turn the call to samplingMethodRecompile into a call to samplingPatchCallSite

      patchAddr = (int32_t *)((uint8_t *)oldStartPC + OFFSET_SAMPLING_BRANCH_FROM_STARTPC);

      helperAddress = (intptr_t)runtimeHelperValue(TR_ARM64samplingPatchCallSite);
      if (!TR::Compiler->target.cpu.isTargetWithinUnconditionalBranchImmediateRange(helperAddress, (intptr_t)patchAddr)
          || TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines))
         {
         helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(TR_ARM64samplingPatchCallSite, (void *)patchAddr);
         }
      newInstr = encodeDistanceInBranchInstruction(TR::InstOpCode::bl, helperAddress - (intptr_t)patchAddr);
      if (DEBUG_ARM64_RECOMP)
         {
         printf("\tsampling recomp, change instruction location (%p) of sampling branch to branch encoding 0x%x (to TR_ARM64samplingPatchCallSite)\n",
                  patchAddr, newInstr); fflush(stdout);
         }
      *patchAddr = newInstr;
      arm64CodeSync((uint8_t *)patchAddr, ARM64_INSTRUCTION_LENGTH);

      fixUpMethodCode(oldStartPC);

      bytesToSaveAtStart = getJitEntryOffset(linkageInfo) + ARM64_INSTRUCTION_LENGTH;
      }

   bool codeMemoryWasAlreadyReleased = linkageInfo->hasBeenRecompiled(); // HCR - can recompile the same body twice
   linkageInfo->setHasBeenRecompiled();

   if (linkageInfo->isSamplingMethodBody() && !codeMemoryWasAlreadyReleased)
      {
      if (DEBUG_ARM64_RECOMP)
         {
         printf("\tsampling recomp, releasing code memory oldStartPC = %p, bytesToSaveAtStart = 0x%x\n", oldStartPC, bytesToSaveAtStart); fflush(stdout);
         }
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      fej9->releaseCodeMemory(oldStartPC, bytesToSaveAtStart);
      }
   }

void J9::Recompilation::methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe)
   {
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(oldStartPC);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   int32_t *patchAddr, newInstr;
   intptr_t distance;
   TR_ASSERT( linkageInfo->isSamplingMethodBody() && !linkageInfo->isCountingMethodBody() ||
          !linkageInfo->isSamplingMethodBody() && linkageInfo->isCountingMethodBody(),
          "Jitted body must be either sampling or counting");
   TR_PersistentJittedBodyInfo *bodyInfo = getJittedBodyInfoFromPC(oldStartPC);
   TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();

   if (bodyInfo->getUsesPreexistence()
       || methodInfo->hasBeenReplaced()
       || (linkageInfo->isSamplingMethodBody() && !fej9->isAsyncCompilation())) // go interpreted for failed recomps in sync mode
      {
      // Patch the first instruction regardless of counting or sampling
      patchAddr = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo));
      distance = OFFSET_REVERT_INTP_FIXED_PORTION - 2*getJitEntryOffset(linkageInfo);
      if (linkageInfo->isCountingMethodBody())
         TR_UNIMPLEMENTED();
      else
         distance += OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC;

      if (DEBUG_ARM64_RECOMP)
         {
         uintptr_t target = (uintptr_t)patchAddr + distance;
         printf("oldStartPC %x, patchAddr %p, target %lx\n", oldStartPC, patchAddr, target); fflush(stdout);
         }

      *patchAddr = encodeDistanceInBranchInstruction(TR::InstOpCode::b, distance);
      arm64CodeSync((uint8_t *)patchAddr, ARM64_INSTRUCTION_LENGTH);

      if (!methodInfo->hasBeenReplaced()) // HCR: VM presumably already has the method in its proper state
         fej9->revertToInterpreted(methodInfo->getMethodInfo());
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      // For async compilation, the old method is not fixed up anyway
      if (!fej9->isAsyncCompilation())
         {
         // Restore the original instructions
         // Calculate entry point
         int32_t *startByte = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo));
         if (DEBUG_ARM64_RECOMP)
            {
            printf("MethodCannotBeRecompiled sampling recomp sync compilation restoring preserved jitEntry of 0x%x at location %p\n",
                   *((int32_t *)((uint8_t *)oldStartPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)), startByte); fflush(stdout);
            }
         *startByte = *((int32_t *)((uint8_t *)oldStartPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC));
         arm64CodeSync((uint8_t *)startByte, 4);
         }
      }

   linkageInfo->setHasFailedRecompilation();
   }

void J9::Recompilation::invalidateMethodBody(void *startPC, TR_FrontEnd *fe)
   {
   // Pre-existence assumptions for this method have been violated. Make the
   // method no-longer runnable and schedule it for sync recompilation
   //
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
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

// in Trampoline.cpp
extern bool arm64CodePatching(void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra);

extern "C" {
void arm64IndirectCallPatching_unwrapper(void **argsPtr, void **resPtr)
   {
   arm64CodePatching(argsPtr[0], argsPtr[1], NULL, NULL, argsPtr[2], argsPtr[3]);
   }
}

#if defined(TR_HOST_ARM64)
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo)
   {
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   int32_t jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   if (linkageInfo->isSamplingMethodBody())
      {
      *(void **)((int8_t *)startPC + OFFSET_SAMPLING_METHODINFO_FROM_STARTPC) = bodyInfo;
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      TR_UNIMPLEMENTED();
      }
   return;
   }
#endif
