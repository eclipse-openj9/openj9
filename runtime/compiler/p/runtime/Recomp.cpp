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

#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"

#include <limits.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"
#include "env/VMJ9.h"

extern "C" int32_t _tr_try_lock(int32_t *, int32_t, int32_t);
extern void ppcCodeSync(uint8_t *, uint32_t);

#ifdef TR_HOST_64BIT
#  define  OFFSET_COUNTING_BRANCH_FROM_JITENTRY         (36)
#else
#  define  OFFSET_COUNTING_BRANCH_FROM_JITENTRY         (24)
#endif

// Called at runtime to get the body info from the start PC
//
TR_PersistentJittedBodyInfo *J9::Recompilation::getJittedBodyInfoFromPC(void *startPC)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   int32_t  jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   if (linkageInfo->isSamplingMethodBody())
      {
      return(*(TR_PersistentJittedBodyInfo **)((int8_t *)startPC + OFFSET_SAMPLING_METHODINFO_FROM_STARTPC));
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      int32_t *branchLocation = (int32_t *)((int8_t *)jitEntry + OFFSET_COUNTING_BRANCH_FROM_JITENTRY);
      int32_t  binst = *branchLocation;
      int32_t  toSnippet;

      if ((binst & 0xFF830000) == 0x41800000)   // blt snippet
	 {
	 toSnippet = ((binst<<16) & 0xFFFC0000)>>16;
	 }
      else
	 {
         branchLocation = (int32_t *)((int8_t *)branchLocation + 4);
         binst = *branchLocation;
         toSnippet = ((binst<<6) & 0xFFFFFF00)>>6;
	 }
      return(*(TR_PersistentJittedBodyInfo **)((int8_t *)branchLocation + toSnippet + 4));
      }
   return NULL;
   }

bool J9::Recompilation::isAlreadyPreparedForRecompile(void *startPC)
   {
   int32_t  jitEntryOffset = getJitEntryOffset(TR_LinkageInfo::get(startPC));
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   return ((*jitEntry & 0xff000000) == 0x4b000000);
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
      bodyInfo->setCounter(-1);
      }
   else
      {
      int32_t  jitEntryOffset = getJitEntryOffset(linkageInfo); //((*linkageInfo)>>16) & 0x0000FFFF;
      int32_t *jitEntry = (int32_t *)((uint8_t*)startPC + jitEntryOffset);
      int32_t  newEntry = 0x48000000 | ((OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC-jitEntryOffset) & 0x03fffffc);
      int32_t  preserved = *jitEntry;

      if ((preserved & 0xff000000) != 0x4b000000)
         {
         *((int32_t *)((int8_t *)startPC+OFFSET_SAMPLING_PRESERVED_FROM_STARTPC)) = preserved;
         FLUSH_MEMORY(TR::Compiler->target.isSMP());
         }

      // Other thread might try to do the same thing at the same time.
      while((preserved & 0xff000000) != 0x4b000000)
	 {
	 if (_tr_try_lock(jitEntry, preserved, newEntry))
	    {
              ppcCodeSync((uint8_t *)jitEntry, 4);
              break;
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

   if (linkageInfo->isCountingMethodBody())
      {
      // Turn the instruction before the counting branch in the
      // counting/profiling prologue into a bl to countingPatchCallSite
      // which expects the new startPC.

      patchAddr = (int32_t *)((uint8_t *)oldStartPC + getJitEntryOffset(linkageInfo) + OFFSET_COUNTING_BRANCH_FROM_JITENTRY - 4);
      intptrj_t helperAddress = (intptrj_t)runtimeHelperValue(TR_PPCcountingPatchCallSite);
      if (!TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)patchAddr) ||
          TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines))
	 {
         helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(TR_PPCcountingPatchCallSite, (void *)patchAddr);
         TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)patchAddr),
                         "Helper address is out of range");
	 }

      newInstr = 0x48000001 | ((helperAddress - (intptrj_t)patchAddr) & 0x03FFFFFC);
      *patchAddr = newInstr;
      ppcCodeSync((uint8_t *)patchAddr, 4);
      bytesToSaveAtStart = getJitEntryOffset(linkageInfo) + OFFSET_COUNTING_BRANCH_FROM_JITENTRY;
      }
   else
      {
      // Turn the call to samplingMethodRecompile into a call to samplingPatchCallSite

      patchAddr = (int32_t *)((uint8_t *)oldStartPC + OFFSET_SAMPLING_BRANCH_FROM_STARTPC);
      intptrj_t helperAddress = (intptrj_t)runtimeHelperValue(TR_PPCsamplingPatchCallSite);
      if (!TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)patchAddr) ||
          TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines))
	 {
         helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(TR_PPCsamplingPatchCallSite, (void *)patchAddr);
         TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)patchAddr),
                         "Helper address is out of range");
	 }

      newInstr = 0x48000001 | ((helperAddress - (intptrj_t)patchAddr) & 0x03FFFFFC);
      *patchAddr = newInstr;
      ppcCodeSync((uint8_t *)patchAddr, 4);

      // The order of this code sync sequence is important. Don't try to common them
      // up and get out of order. For sync compilation, the old body must have been
      // fixed up already.
      // update: the above is no longer true when guarded counting recompilations is on.
      fixUpMethodCode(oldStartPC);

      bytesToSaveAtStart = getJitEntryOffset(linkageInfo) + 4;
      }

   bool codeMemoryWasAlreadyReleased = linkageInfo->hasBeenRecompiled(); // HCR - can recompile the same body twice
   linkageInfo->setHasBeenRecompiled();

   // Code Cache Reclamation does not work on ppc
   // without counting method bodies.  _countingPatchCallSite still refers
   // to the methodInfo pointer in the snippet area.
   if (linkageInfo->isSamplingMethodBody() && !codeMemoryWasAlreadyReleased)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
      fej9->releaseCodeMemory(oldStartPC, bytesToSaveAtStart);
      }
   }

void J9::Recompilation::methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   int32_t  *patchAddr, newInstr, distance;
   TR_ASSERT( linkageInfo->isSamplingMethodBody() && !linkageInfo->isCountingMethodBody() ||
          !linkageInfo->isSamplingMethodBody() &&  linkageInfo->isCountingMethodBody(),
          "Jitted body must be either sampling or counting");
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
         distance -= 4;
      else
         distance += OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC;
      newInstr = 0x48000000|(distance & 0x03FFFFFC);
      *patchAddr = newInstr;
      ppcCodeSync((uint8_t *)patchAddr, 4);

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
      if ((*branchLocation & 0xFF830000) == 0x41800000)  // blt snippet
         distance = OFFSET_COUNTING_BRANCH_FROM_JITENTRY + 4;
      else
         distance = OFFSET_COUNTING_BRANCH_FROM_JITENTRY + 8;
      newInstr  = 0x48000000 | distance;
      *patchAddr = newInstr;
      ppcCodeSync((uint8_t *)patchAddr, 4);

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
        *((uint32_t*)startByte) = *((uint32_t*)((uint8_t*)oldStartPC + OFFSET_SAMPLING_PRESERVED_FROM_STARTPC));
        ppcCodeSync((uint8_t *)startByte, 4);
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


extern bool ppcCodePatching(void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra);
extern "C" {
void ppcIndirectCallPatching_unwrapper(void **argsPtr, void **resPtr)
   {
   ppcCodePatching(argsPtr[0], argsPtr[1], NULL, NULL, argsPtr[2], argsPtr[3]);
   }
}

#if defined(TR_HOST_POWER)
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo)
   {
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   int32_t  jitEntryOffset = getJitEntryOffset(linkageInfo);
   int32_t *jitEntry = (int32_t *)((int8_t *)startPC + jitEntryOffset);

   if (linkageInfo->isSamplingMethodBody())
      {
      *(void **)((int8_t *)startPC + OFFSET_SAMPLING_METHODINFO_FROM_STARTPC) = bodyInfo;
      }
   else if (linkageInfo->isCountingMethodBody())
      {
      int32_t *branchLocation = (int32_t *)((int8_t *)jitEntry + OFFSET_COUNTING_BRANCH_FROM_JITENTRY);
      int32_t  binst = *branchLocation;
      int32_t  toSnippet;

      if ((binst & 0xFF830000) == 0x41800000)   // blt snippet
	 {
	 toSnippet = ((binst<<16) & 0xFFFC0000)>>16;
	 }
      else
	 {
         branchLocation = (int32_t *)((int8_t *)branchLocation + 4);
         binst = *branchLocation;
         toSnippet = ((binst<<6) & 0xFFFFFF00)>>6;
	 }
      *(void **)((int8_t *)branchLocation + toSnippet + 4) = bodyInfo;
      }
   return;
   }
#endif
