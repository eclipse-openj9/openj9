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

#include "p/runtime/PPCHWProfiler.hpp"

#include "j9cfg.h"
#include "j9port_generated.h"
#include "util_api.h"
#include "codegen/FrontEnd.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"
#include "env/VMJ9.h"
#include "p/runtime/PPCHWProfilerPrivate.hpp"
#include "control/CompilationRuntime.hpp"

static int32_t baseEventHandler(TR_PPCHWProfilerEBBContext *context);
static int32_t methodHotnessEventHandler(TR_PPCHWProfilerEBBContext *context, int32_t pmu);
static int32_t updatePMC(int32_t pmcNumber, uint32_t pmc[]);

static struct TR_PPCHWProfilerPMUConfig configs[NumProfilingConfigs] =
   {
      { // MethodHotness
         baseEventHandler,
         {
            NULL,
            NULL,
            NULL,
            NULL,
            methodHotnessEventHandler,
            NULL
         },
#ifdef AIXPPC
         {
            NULL,
            NULL,
            NULL,
            NULL,
            "PM_RUN_INST_CMPL",
            NULL
         },
         -1,
#elif defined(LINUXPPC)
         {
            0x0,
            0x0,
            0x0,
            0x0,
            0xFA,       // PM_RUN_INST_CMPL
            0x0
         },
#endif /* LINUXPPC */
         BHRB_IFM1,     // Calls
         {0, 0, 0, 0, 512, 0},
         {0, 0, 0, 0, sizeof(TR_PPCMethodHotnessSample), 0},
         {INVALID_SAMPLERATE, INVALID_SAMPLERATE, INVALID_SAMPLERATE, INVALID_SAMPLERATE, 1000000, INVALID_SAMPLERATE},
         0 // XXX: Re-enable when interpreter is fixed //J9_JIT_TOGGLE_RI_ON_TRANSITION
      },
      { // BlockHotness
         NULL,
         {NULL},
#ifdef AIXPPC
         {NULL},
         0,
#elif defined(LINUXPPC)
         {0},
#endif /* LINUXPPC */
         BHRB_IFM0,
         {0},
         {0},
         {0},
         0
      },
      { // FieldHotness
         NULL,
         {NULL},
#ifdef AIXPPC
         {NULL},
         0,
#elif defined(LINUXPPC)
         {0},
#endif /* LINUXPPC */
         BHRB_IFM0,
         {0},
         {0},
         {0},
         0
      }
   };

TR_PPCHWProfilerPMUConfig* TR_PPCHWProfilerPMUConfig::getPMUConfigs()
   {
   return configs;
   }

static int32_t updatePMC(int32_t pmcNumber, uint32_t pmc[])
   {
   switch (pmcNumber)
      {
      case 1: return tr_pmc_write(PMC1, &pmc[0]);
      case 2: return tr_pmc_write(PMC2, &pmc[1]);
      case 3: return tr_pmc_write(PMC3, &pmc[2]);
      case 4: return tr_pmc_write(PMC4, &pmc[3]);
      case 5: return tr_pmc_write(PMC5, &pmc[4]);
      case 6: return tr_pmc_write(PMC6, &pmc[5]);
      default: return 1;
      }
   }

static int32_t baseEventHandler(TR_PPCHWProfilerEBBContext *context)
   {
   uint32_t pmc[MAX_PMCS];

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   bool counterNegative = false;
#endif /* DEBUG || PPCRI_VERBOSE */

   if (OMR_UNLIKELY(tr_pmc_read_1to4(&pmc[0]))) goto lostpmu;
   if (OMR_UNLIKELY(tr_pmc_read_5to6(&pmc[4]))) goto lostpmu;

   uint64_t mmcr[3];
   if (OMR_UNLIKELY(tr_mmcr_read(mmcr))) goto lostpmu;

   for (int32_t i = 0; i < MAX_PMCS; i++)
      {
      if ((context->counterBitMask & (1 << i)) &&
          pmc[i] >= 0x80000000)
         {
         if (configs[context->currentConfig].eventHandlerTable[i])
            {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
            ++context->eventCounts[i];
#endif
            TR_ASSERT(context->sampleRates[i] != INVALID_SAMPLERATE, "Unexpected sample rate");
            pmc[i] = 0x80000000 - context->sampleRates[i];

            if (configs[context->currentConfig].eventHandlerTable[i](context, i)) goto lostpmu;

            if (updatePMC(i+1, pmc)) goto lostpmu;

            // If the buffer is full we want to disable the current counter, so we freeze it through MMCR2
            if (OMR_UNLIKELY(!context->buffers[i].spaceLeft))
               {
               context->counterBitMask &= (0xFF ^ (1 << i));
               TR_PPCHWProfilerPMUConfig::calcMMCR2ToDisablePMC(mmcr[1], i);
               if (OMR_UNLIKELY(tr_mmcr_write(MMCR2, &mmcr[1]))) goto lostpmu;
               }
            }
         else
            {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
            ++context->eventCounts[i];
            counterNegative = true;
#endif /* DEBUG || PPCRI_VERBOSE */
            pmc[i] = context->sampleRates[i] != INVALID_SAMPLERATE ? 0x80000000 - context->sampleRates[i] : 0;
            if (updatePMC(i+1, pmc)) goto lostpmu;
            }
         }
      }

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
      if (OMR_UNLIKELY(!counterNegative))
         ++context->unknownEventCount;
#endif /* DEBUG || PPCRI_VERBOSE */

   // In order to re-enable EBBs and restart the PMU we need to clear MMCR0[PMAO|FC] and BESCR[PMEO] and set MMCR0[PMAE] and BESCR[GE|PME]
   // however the order is important for a couple of reasons. First, the BESCR bits can't be manipulated until MMCR0[PMAO] is cleared.
   // Second, we have to unfreeze the PMU before returning from the EBB handler, but we don't want instructions that are executed as part of the
   // EBB handler to be counted. To minimize the number of EBB instructions counted we clear MMCR0[FC] as the last step before returning from the
   // assembly EBB handler.

   // First clear MMCR0[PMAO] and set MMCR[PMAE], but leave MMCR[FC] set
   // goto bypasses decl of mmcr0
      {
      const uint64_t mmcr0 = MMCR0_FC | MMCR0_PMAE;
      if (OMR_UNLIKELY(tr_mmcr_write(MMCR0, &mmcr0))) goto lostpmu;
      }

   // Then clear BESCR[PMEO] and set BESCR[PME]
   MTSPR(BESCRR, BESCR_PMEO);
   MTSPR(BESCRSU, BESCRU_PME);

   // MMCR0[FC] will be cleared by the assembly EBB handler
   return 0;

lostpmu:
   // If we've lost access to the PMU while servicing the EBB we can't re-initialize it until we
   // get access back so we set a flag and deal with it later
   context->lostPMU = true;
   return 1;
   }

static int32_t methodHotnessEventHandler(TR_PPCHWProfilerEBBContext *context, int32_t pmu)
   {
   if (OMR_LIKELY(context->buffers[pmu].spaceLeft))
      {
      TR_PPCMethodHotnessSample *buffer = (TR_PPCMethodHotnessSample *)context->buffers[pmu].start;
      TR_PPCMethodHotnessSample *sample = &buffer[configs[context->currentConfig].bufferSize[pmu] - context->buffers[pmu].spaceLeft];
      sample->sia = context->ebbrr;
#ifdef HAVE_BHRBES
      readBHRB(sample->calls);
      CLRBHRB();
#endif /* HAVE_BHRBES */
      --context->buffers[pmu].spaceLeft;
      }

   return 0;
   }

void TR_PPCHWProfilerPMUConfig::calcMMCR2ForConfig(uint64_t &mmcr2, TR_PPCHWProfilingConfigs c)
   {
   uint64_t fcnp = 0;
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (configs[c].sampleRates[i] == INVALID_SAMPLERATE)
         fcnp |= MMCR2_FCnP << ((5 - i) * 9 + 10);
      }
   mmcr2 = fcnp;
   }

void TR_PPCHWProfilerPMUConfig::calcMMCR2ToDisablePMC(uint64_t &mmcr2, int32_t pmcNumber)
   {
   TR_ASSERT(pmcNumber >= 0 && pmcNumber < MAX_PMCS, "Trying to disable invalid pmcNumber");
   uint64_t fcnp = 0;
   fcnp |= MMCR2_FCnP << ((5 - pmcNumber) * 9 + 10);
   mmcr2 |= fcnp;
   }

TR_PPCHWProfiler::TR_PPCHWProfiler(J9JITConfig *jitConfig)
   : TR_HWProfiler(jitConfig),
     _ppcHWProfilerBufferMemoryAllocated(0), _ppcHWProfilerBufferMaximumMemory(TR::Options::_hwprofilerRIBufferPoolSize)
   {}

bool
TR_PPCHWProfiler::processBuffers(J9VMThread *vmThread, TR_J9VMBase *fe)
   {
   TR_ASSERT(IS_THREAD_RI_INITIALIZED(vmThread), "processBuffers() called on uninitialized thread");
   TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS), "Must have vm access!");

   const uint64_t freeze = MMCR0_FC;
   if (OMR_UNLIKELY(tr_mmcr_write(MMCR0, &freeze)))
      return false;

   TR_PPCHWProfilerEBBContext *context = (TR_PPCHWProfilerEBBContext *)vmThread->riParameters->controlBlock;

   if (OMR_UNLIKELY(context->lostPMU))
      {
      // TODO: If we lost PMU access in the EBB we need to re-initialize
      VERBOSE("J9VMThread=%p lost PMU access while handling an EBB, resetting PMU.");
      }

   bool isRIEnabled = IS_THREAD_RI_ENABLED(vmThread);

   bool pmuFrozen = false;
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (!context->buffers[i].start)
         continue;
      else
         context->counterBitMask |= (1 << i);

      if (!context->buffers[i].spaceLeft)
         pmuFrozen = true;
      }

   if (pmuFrozen)
      {
      uint64_t mmcr2;
      TR_PPCHWProfilerPMUConfig::calcMMCR2ForConfig(mmcr2, context->currentConfig);
      if (OMR_UNLIKELY(tr_mmcr_write(MMCR2, &mmcr2)))
         return false;
      }

   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (!context->buffers[i].start)
         continue;
      uint32_t bufferSize = configs[context->currentConfig].bufferSize[i];
      uint32_t spaceLeft = context->buffers[i].spaceLeft;
      float    bufferSpaceLeftPercentage = (float)spaceLeft / (float)bufferSize * 100.0f;
      if (bufferSpaceLeftPercentage > (100 - TR::Options::_hwprofilerRIBufferThreshold))
         continue;

      // The data tag for the buffer is the PMC index in the high 16 bits and the RI config in the low 16 bits.
      // processBufferRecords() will use this to figure out what kind of samples the buffer contains.
      uint32_t dataTag = i << 16 | context->currentConfig;
      uint32_t bufferElementSize = configs[context->currentConfig].bufferElementSize[i];
      uint32_t bufferSizeInBytes = bufferSize * bufferElementSize;
      uint32_t bufferFilledSize = bufferSize - spaceLeft;
      uint32_t bufferFilledSizeInBytes = bufferFilledSize * bufferElementSize;

#ifdef TOSS_HW_SAMPLES
      // For testing purposes, throw away the collected samples
      context->buffers[i].spaceLeft = bufferSize;
#else /* !TOSS_HW_SAMPLES */

      _numRequests++;

      uint8_t *newBuffer = swapBufferToWorkingQueue((U_8*)context->buffers[i].start,
                                                     bufferSizeInBytes,
                                                     bufferFilledSizeInBytes,
                                                     dataTag);
      if (OMR_LIKELY(newBuffer != NULL))
         {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
         VERBOSE("Swapped buffers %p (old) and %p (new) for J9VMThread=%p.", context->buffers[i].start, newBuffer, vmThread);
#endif /* DEBUG || PPCRI_VERBOSE */
         context->buffers[i].start = newBuffer;
         context->buffers[i].spaceLeft = bufferSize;
         }
      else
#ifdef AVOID_PROCESSING_ON_APPTHREAD
      // Only process on the app thread if the buffer is near full and we can't swap for a free one, otherwise keep collecting samples
      if (bufferSpaceLeftPercentage < 10)
#endif /* AVOID_PROCESSING_ON_APPTHREAD */
         {
         if (TR::Options::getCmdLineOptions()->getOption(TR_DisableHWProfilerThread) ||
             (100*_numRequestsSkipped) >= ((uint64_t)TR::Options::_hwProfilerBufferMaxPercentageToDiscard * _numRequests))
            {
            // Process buffer by application thread and reuse the buffer
            processBufferRecords(vmThread, (U_8*)context->buffers[i].start,
                                 bufferSizeInBytes,
                                 bufferFilledSizeInBytes,
                                 dataTag);
            _STATS_BuffersProcessedByAppThread++;
            }
         else
            {
            _numRequestsSkipped++;
            }
         context->buffers[i].spaceLeft = bufferSize;
         }
#endif /* !TOSS_HW_SAMPLES */
      }

   if (isRIEnabled)
      {
      const uint64_t unfreeze = MMCR0_PMAE;
      tr_mmcr_write(MMCR0, &unfreeze);
      }

   return false;
   }

static void incrementMethodHotness(J9JITExceptionTable *metaData)
   {
#if 0
   TR_PersistentJittedBodyInfo *bodyInfo = (TR_PersistentJittedBodyInfo *)metaData->bodyInfo;
   if (!bodyInfo)
      return;

   TR_PersistentMethodInfo     *methodInfo = bodyInfo->getMethodInfo();
   if (!methodInfo)
      return;

   if (methodInfo->_samples != UINT_MAX)
      ++methodInfo->_samples;
#endif
   }

static void incrementCallHotness(J9JITExceptionTable *callerMetaData, J9JITExceptionTable *calleeMetaData)
   {
#if 0
   TR_PersistentJittedBodyInfo *callerBodyInfo = (TR_PersistentJittedBodyInfo *)callerMetaData->bodyInfo;
   if (!callerBodyInfo)
      return;

   TR_PersistentMethodInfo     *callerMethodInfo = callerBodyInfo->getMethodInfo();
   if (!callerMethodInfo)
      return;

   TR_PersistentJittedBodyInfo *calleeBodyInfo = (TR_PersistentJittedBodyInfo *)calleeMetaData->bodyInfo;
   if (!calleeBodyInfo)
      return;

   TR_PersistentMethodInfo     *calleeMethodInfo = calleeBodyInfo->getMethodInfo();
   if (!calleeMethodInfo)
      return;

   TR_SampledCallerInfo *sampledCallerInfo = calleeMethodInfo->_sampledCallers;
   TR_SampledCallerInfo *prevSampledCallerInfo = NULL;
   while (sampledCallerInfo)
      {
      if (sampledCallerInfo->_methodInfo == callerMethodInfo)
         {
         if (sampledCallerInfo->_samples != UINT_MAX)
            ++sampledCallerInfo->_samples;
         return;
         }
      prevSampledCallerInfo = sampledCallerInfo;
      sampledCallerInfo = sampledCallerInfo->getNext();
      }

   TR_SampledCallerInfo *newCaller = (TR_SampledCallerInfo *)TR_Memory::jitPersistentAlloc(sizeof(TR_SampledCallerInfo));
   newCaller->_methodInfo = callerMethodInfo;
   newCaller->_samples = 1;
   newCaller->setNext(NULL);
   if (!prevSampledCallerInfo)
      calleeMethodInfo->_sampledCallers = newCaller;
   else
      prevSampledCallerInfo->setNext(newCaller);
#endif
   }

// This helps when you have several consecutive samples in a buffer hitting the same method,
// however even with just a few hits it's probably a net win considering how much work is
// done for a single metadata search.
#define CACHE_LAST_METADATA

static void processMethodHotness(J9VMThread *vmThread, J9JITConfig *jitConfig, TR_HWProfiler *hwProfiler, TR_PPCMethodHotnessSample *samples, uint32_t numSamples)
   {
   uint32_t numJittedSamples = 0;
   uint32_t numJit2JitCalls = 0;
   TR_FrontEnd *fe           = TR_J9VMBase::get(jitConfig, vmThread);
   bool recompilationEnabled = false;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationRecompilationEnabled()
       && vmThread != NULL
       && fe != NULL)
      {
      recompilationEnabled = true;
      }

#ifdef CACHE_LAST_METADATA
   J9JITExceptionTable *lastMetaData = NULL;
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   uint32_t             lastMetaDataHit = 0;
#endif /* DEBUG || PPCRI_VERBOSE */
#endif /* CACHE_LAST_METADATA */
   J9JITExceptionTable *metaData;
   for (uint32_t i = 0; i < numSamples; ++i)
      {
#ifdef CACHE_LAST_METADATA
      if (lastMetaData && samples[i].sia >= lastMetaData->startPC && samples[i].sia <= lastMetaData->endPC)
         {
         metaData = lastMetaData;
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
         ++lastMetaDataHit;
#endif /* DEBUG || PPCRI_VERBOSE */
         }
      else
#endif /* CACHE_LAST_METADATA */
         {
         metaData = jit_artifact_search(jitConfig->translationArtifacts, samples[i].sia);
         if (!metaData)
            {
            continue;
            }
#ifdef CACHE_LAST_METADATA
         lastMetaData = metaData;
#endif /* CACHE_LAST_METADATA */
         }

      ++numJittedSamples;
      incrementMethodHotness(metaData);

      TR::Recompilation::hwpGlobalSampleCount++;
      if (recompilationEnabled && metaData->bodyInfo != NULL)
         {
         TR_PersistentJittedBodyInfo *bodyInfo = (TR_PersistentJittedBodyInfo *) metaData->bodyInfo;

         bodyInfo->_hwpInstructionCount++;
         if (hwProfiler->recompilationLogic(bodyInfo,
                                            (void *) metaData->startPC,
                                            bodyInfo->_hwpInstructionStartCount,
                                            bodyInfo->_hwpInstructionCount,
                                            TR::Recompilation::hwpGlobalSampleCount,
                                            fe,
                                            vmThread))
            {
            // Start a new interval
            bodyInfo->_hwpInstructionStartCount   = TR::Recompilation::hwpGlobalSampleCount;
            bodyInfo->_hwpInstructionCount        = 0;
            }
         }

#ifdef HAVE_BHRBES
      uintptr_t callerAddr = 0;
      uintptr_t calleeAddr = 0;
      for (uint32_t j = 0; j < MAX_BHRBES; ++j)
         {
         uintptr_t bhrbEntry = samples[i].calls[j];

         // No more entries
         if (!bhrbEntry)
            break;

         uintptr_t bhrbEntryEA = bhrbEntry & BHRBE_EA_MASK;
         if (OMR_LIKELY(bhrbEntryEA))
            {
            if (bhrbEntry & BHRBE_T_MASK)
               {
               // Indirect call target address
               calleeAddr = bhrbEntryEA;
               }
            else
               {
               // Call address
               callerAddr = bhrbEntryEA;

               if (!configs[MethodHotness].vmFlags)  // If no VM flags are set then samples can come from anywhere and we need to be more careful
                  {
                  // Need to check if the caller address is in a JIT codecache before we dereference it, otherwise
                  // we risk accessing memory that's been unmapped since the instruction was executed
                  J9MemorySegment *codeCache = jitConfig->codeCache;
                  while (codeCache)
                     {
                     if (callerAddr >= (uintptr_t)codeCache->heapBase && callerAddr < (uintptr_t)codeCache->heapTop)
                        break;
                     codeCache = codeCache->nextSegment;
                     }
                  if (!codeCache)
                     continue;
                  }

               uint32_t callInsn = *(uint32_t *)callerAddr;
               uint32_t op = callInsn >> 26;

               // If free block recycling is not disabled the data in the BHRB could be stale
               if (OMR_LIKELY(!TR::Options::getCmdLineOptions()->getOption(TR_DisableFreeCodeCacheBlockRecycling)))
                  {
                  if (op != 16 && op != 18 && op != 19)
                     continue;
                  if ((callInsn & 1) == 0)
                     continue;
                  }

               if (OMR_LIKELY(op == 18))                 // I-form, bl is the most common type of call
                  {
                  if (OMR_UNLIKELY(callInsn & 2))        // Don't care about absolute calls
                     continue;
                  int32_t offset = callInsn & 0x03FFFFFC;
                  offset = offset << 6 >> 6;
                  calleeAddr = callerAddr + offset;
                  }
               else if (op == 19)                    // X-form, bctrl/blrl
                  {
                  if (OMR_UNLIKELY((callInsn & 0x000007FE) >> 1 != 528)) // Only care about bctrl
                     continue;
                  if (OMR_UNLIKELY(!calleeAddr))         // HW wasn't able to capture the target address
                     continue;
                  }
               else if (OMR_UNLIKELY(op == 16))          // B-form, bcl is fairly uncommon and is never used to call Java methods anyway
                  {
#if 0
                  if (OMR_UNLIKELY(callInsn & 2))        // Don't care about absolute calls
                     continue;
                  int32_t offset = callInsn & 0x0000FFFC;
                  offset = (int16_t)offset;
                  calleeAddr = callerAddr + offset;
#else
                  // We don't use bcl for Java calls, just skip these
                  continue;
#endif
                  }
               else
                  {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
                  VERBOSE("Unrecognized call instruction %x (BHRBE[%u]=%p).", callInsn, j, bhrbEntry);
#endif /* DEBUG || PPCRI_VERBOSE  */
                  continue;
                  }

               J9JITExceptionTable *callerMetaData = jit_artifact_search(jitConfig->translationArtifacts, callerAddr);
               if (!callerMetaData)
                  continue;
               J9JITExceptionTable *calleeMetaData = jit_artifact_search(jitConfig->translationArtifacts, calleeAddr);
               if (!calleeMetaData)
                  continue;

               // Skip calls via snippets.
               // Could also be a recursive call, but those are relatively
               // uncommon and probably not worth trying to detect.
               if (callerMetaData == calleeMetaData)
                  continue;

               ++numJit2JitCalls;
               incrementCallHotness(callerMetaData, calleeMetaData);
               }
            }
         else if (bhrbEntry & ~BHRBE_EA_MASK)
            {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
            VERBOSE("BHRBE has no EA, but has low bits=%u.", bhrbEntry & ~BHRBE_EA_MASK);
#endif /* DEBUG || PPCRI_VERBOSE  */
            if (bhrbEntry & 1)
               calleeAddr = 0; // HW wasn't able to enter an address, clear this to make sure we don't leave any stale info lying around
            }
         }
#endif /* HAVE_BHRBES */
      }

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
#ifdef CACHE_LAST_METADATA
   VERBOSE("Processed buffer %p on J9VMThread=%p, samples=%u, jitted samples=%u, jit2jit calls=%u, lastMetaDataHit=%u.",
           samples, vmThread, numSamples, numJittedSamples, numJit2JitCalls, lastMetaDataHit);
#else /* !CACHE_LAST_METADATA */
   VERBOSE("Processed buffer %p on J9VMThread=%p, samples=%u, jitted samples=%u, jit2jit calls=%u.",
           samples, vmThread, numSamples, numJittedSamples, numJit2JitCalls);
#endif /* !CACHE_LAST_METADATA */
#endif /* DEBUG || PPCRI_VERBOSE  */
   }

void
TR_PPCHWProfiler::processBufferRecords(J9VMThread *vmThread, uint8_t *bufferStart, uintptrj_t size, uintptrj_t bufferFilledSize, uint32_t dataTag)
   {
   TR_PPCHWProfilingConfigs config = (TR_PPCHWProfilingConfigs)(dataTag & 0xFFFF);
   uint32_t                 pmc = dataTag >> 16;

   switch (config)
      {
      case MethodHotness:
         {
         if (configs[config].eventHandlerTable[pmc] == methodHotnessEventHandler)
            {
            TR_PPCMethodHotnessSample *samples = (TR_PPCMethodHotnessSample *)bufferStart;
            uint32_t                   numSamples = bufferFilledSize / sizeof(TR_PPCMethodHotnessSample);

            _STATS_TotalEntriesProcessed += numSamples;
            processMethodHotness(vmThread, _jitConfig, this, samples, numSamples);
            }
         break;
         }
      }
   if (bufferFilledSize >= size)
      _numBuffersCompletelyFilled++;

   _bufferSizeSum += size;
   _bufferFilledSum += bufferFilledSize;
   ++_STATS_TotalBuffersProcessed;
   }

void *
TR_PPCHWProfiler::allocateBuffer(uint64_t size)
   {
   void * temp = NULL;

   if (_hwProfilerMonitor)
      {
#ifdef ALLOCATE_BUFFER_NO_TRY
      _hwProfilerMonitor->enter();
#else /* ALLOCATE_BUFFER_TRY */
      if (_hwProfilerMonitor->try_enter())
         return NULL;
#endif /* ALLOCATE_BUFFER_TRY */

      // First try to get a buffer from the free list
      HWProfilerBuffer *newHWProfilerBuffer = _freeBufferList.pop();
      if (newHWProfilerBuffer)
         {
         temp = (void *)newHWProfilerBuffer->getBuffer();
         TR_Memory::jitPersistentFree(newHWProfilerBuffer);
         }
      // Try to allocate a buffer from jitPersistentAlloc
      else if (_ppcHWProfilerBufferMemoryAllocated + size < _ppcHWProfilerBufferMaximumMemory)
         {
         _ppcHWProfilerBufferMemoryAllocated += size;
         temp = (void*)TR_Memory::jitPersistentAlloc(size, TR_Memory::HWProfile);
         }

      _hwProfilerMonitor->exit();
      }

   return temp;
   }

void
TR_PPCHWProfiler::freeBuffer(void *buffer, uint64_t size)
   {
   if (_hwProfilerMonitor)
      {
      _hwProfilerMonitor->enter();

      // Put the buffers into the free list for another thread
      HWProfilerBuffer *newHWProfilerBuffer = (HWProfilerBuffer*)TR_Memory::jitPersistentAlloc(sizeof(HWProfilerBuffer));
      if (newHWProfilerBuffer)
         {
         newHWProfilerBuffer->setBuffer((U_8*)buffer);
         newHWProfilerBuffer->setSize(size);
         newHWProfilerBuffer->setIsInvalidated(false);

         _freeBufferList.add(newHWProfilerBuffer);
         }

      _hwProfilerMonitor->exit();
      }
   }

void
TR_PPCHWProfiler::printStats()
   {
   printf ("\n");
   TR_HWProfiler::printStats();
   }
