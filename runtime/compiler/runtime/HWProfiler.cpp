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

#include "runtime/HWProfiler.hpp"

#include "vmaccess.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/CompilationController.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Monitor.hpp"
#include "optimizer/Inliner.hpp"
#include "runtime/J9VMAccess.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"

uint32_t TR_HWProfiler::_STATS_TotalBuffersProcessed = 0;
uint32_t TR_HWProfiler::_STATS_BuffersProcessedByAppThread = 0;
uint64_t TR_HWProfiler::_STATS_TotalEntriesProcessed = 0;
uint32_t TR_HWProfiler::_STATS_TotalInstructionsTracked = 0;
uint32_t TR_HWProfiler::_STATS_NumCompDowngradesDueToRI = 0;
uint32_t TR_HWProfiler::_STATS_NumUpgradesDueToRI = 0;

TR_HWProfiler::TR_HWProfiler(J9JITConfig *jitConfig) :
   _hwProfilerOSThread(NULL), _hwProfilerThread(NULL),
   _hwProfilerThreadAttachAttempted(false), _hwProfilerThreadExitFlag(false),
   _workingBufferTail(NULL), _currentBufferBeingProcessed(NULL), _numOutstandingBuffers(0),
   _numRecompilationsInduced(0), _recompilationInterval(TR::Options::_hwprofilerRecompilationInterval),
   _hwProfilerProcessBufferState(TR::Options::_hwProfilerRIBufferProcessingFrequency), _bufferFilledSum(0),
   _numBuffersCompletelyFilled(0), _bufferSizeSum(1),
   _recompDecisionsTotal(0), _recompDecisionsYes(0), _recompDecisionsTotalStart(0), _recompDecisionsYesStart(0),
   _numDowngradesSinceTurnedOff(0), _qszThresholdToDowngrade(TR::Options::_qszMaxThresholdToRIDowngrade), _expired(false),
   _numRequests(0), _numRequestsSkipped(0), _totalMemoryUsedByMetadataMapping(0), _jitConfig(jitConfig), _numReducedWarmRecompilationsInduced(0),
   _numReducedWarmRecompilationsUpgraded(0)
   {
   _compInfo = TR::CompilationInfo::get(jitConfig);
   _isHWProfilingAvailable = true;

   _lastOptLevel = (TR_Hotness)TR::Options::_hwprofilerLastOptLevel;

   _warmOptLevelThreshold         = TR::Options::_hwprofilerWarmOptLevelThreshold / 10000.0f;
   _reducedWarmOptLevelThreshold  = TR::Options::_hwprofilerReducedWarmOptLevelThreshold / 10000.0f;
   _aotWarmOptLevelThreshold      = TR::Options::_hwprofilerAOTWarmOptLevelThreshold / 10000.0f;
   _hotOptLevelThreshold          = TR::Options::_hwprofilerHotOptLevelThreshold / 10000.0f;
   _scorchingOptLevelThreshold    = TR::Options::_hwprofilerScorchingOptLevelThreshold / 10000.0f;

   _hwProfilerMonitor = TR::Monitor::create("JIT-hwProfilerMonitor");
   if (!_hwProfilerMonitor)
      {
      // This will also internally call setRuntimeInstrumentationRecompilationEnabled(false)
      _compInfo->getPersistentInfo()->setRuntimeInstrumentationEnabled(false);
      _isHWProfilingAvailable = false;
      }
   }

bool
TR_HWProfiler::isHWProfilingAvailable(J9VMThread *vmThread)
   {
   // Either we assume HW profiling is available, or the thread itself says it is initialized!
   // On zLinux, we cannot tell if OS support is available until we try to initialize the call.
   return _isHWProfilingAvailable || IS_THREAD_RI_INITIALIZED(vmThread);
   }

void
TR_HWProfiler::setHWProfilingAvailable(bool supported)
   {
   _isHWProfilingAvailable = supported;
   }

int32_t
TR_HWProfiler::IAHash(uintptrj_t pc)
   {
   return (int32_t)(((pc >> 1) & 0x7FFFFFFF) % HASH_TABLE_SIZE);
   }

uintptrj_t
TR_HWProfiler::getPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   if (byteCodeIndex >= TR::Compiler->mtd.bytecodeSize(method))
      return (uintptrj_t)NULL;

   return (uintptrj_t)(TR::Compiler->mtd.bytecodeStart(method) + byteCodeIndex);
   }

/**
 * Main method for the hardware profiling thread.
 * @param entryarg the jitConfig
 * @return the error code
 */
static int32_t
J9THREAD_PROC hwProfilerThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm           = jitConfig->javaVM;
   TR_HWProfiler *hwProfiler = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->hwProfiler;
   J9VMThread *hwProfilerThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &hwProfilerThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  hwProfiler->getHWProfilerOSThread());
   hwProfiler->getHWProfilerMonitor()->enter();
   hwProfiler->setAttachAttempted(true);
   if (rc == JNI_OK)
      hwProfiler->setHWProfilerThread(hwProfilerThread);
   hwProfiler->getHWProfilerMonitor()->notifyAll();
   hwProfiler->getHWProfilerMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the HW Profiler thread failed

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOnWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOnWithReasonFunc)(hwProfilerThread, J9_JNI_OFFLOAD_SWITCH_JIT_HARDWARE_PROFILER_THREAD);
#endif

   j9thread_set_name(j9thread_self(), "JIT Hardware Profiler");

   hwProfiler->processWorkingQueue();

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   hwProfiler->setHWProfilerThread(NULL);
   hwProfiler->getHWProfilerMonitor()->enter();

   // free the special buffer because we don't need it anymore
   if (hwProfiler->getCurrentBufferBeingProcessed())
      {
      TR_Memory::jitPersistentFree(hwProfiler->getCurrentBufferBeingProcessed());
      hwProfiler->setCurrentBufferBeingProcessed(NULL);
      }
   hwProfiler->setHWProfilerThreadExitFlag();
   hwProfiler->getHWProfilerMonitor()->notifyAll();

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOffNoEnvWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOffNoEnvWithReasonFunc)(vm, j9thread_self(), J9_JNI_OFFLOAD_SWITCH_JIT_HARDWARE_PROFILER_THREAD);
#endif

   j9thread_exit((J9ThreadMonitor*)hwProfiler->getHWProfilerMonitor()->getVMMonitor());
   return 0;
   }

void
TR_HWProfiler::startHWProfilerThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   if (_hwProfilerMonitor)
      {
      // Try to create thread
      if (javaVM->internalVMFunctions->createThreadWithCategory(&_hwProfilerOSThread,
                                                                TR::Options::_profilerStackSize << 10,
                                                                priority, 0,
                                                                &hwProfilerThreadProc,
                                                                javaVM->jitConfig,
                                                                J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         {
         TR::Options::getCmdLineOptions()->setOption(TR_DisableHWProfilerThread);
         }
      else
         {
         _hwProfilerMonitor->enter();
         while (!getAttachAttempted())
            _hwProfilerMonitor->wait();
         _hwProfilerMonitor->exit();
         }
      }
   }

void
TR_HWProfiler::stopHWProfilerThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   if (!_hwProfilerMonitor)
      return;

   if (!getHWProfilerThread())
      return;

   _hwProfilerMonitor->enter();

   // Install a special buffer to signal to profiler thread to stop
   HWProfilerBuffer *specialProfilingBuffer = NULL;
   if (!_freeBufferList.isEmpty())
      {
      specialProfilingBuffer = _freeBufferList.pop();
      }
   else if (!_workingBufferList.isEmpty())
      {
      specialProfilingBuffer = _workingBufferList.pop();
      _numOutstandingBuffers--;
      if (_workingBufferList.isEmpty())
         _workingBufferTail = NULL;
      }
   else
      {
      specialProfilingBuffer = (HWProfilerBuffer*)TR_Memory::jitPersistentAlloc(sizeof(HWProfilerBuffer));
      if (specialProfilingBuffer)
         specialProfilingBuffer->setBuffer(NULL);
      }

   // Deallocate all outstanding buffers
   while (!_workingBufferList.isEmpty())
      {
      HWProfilerBuffer *profilingBuffer = _workingBufferList.pop();
      _numOutstandingBuffers--;
      _freeBufferList.add(profilingBuffer);
      }
   _workingBufferTail = NULL;

   // Add the special buffer, which when processed, will cause
   // hwProfilerThreadProc to exit
   if (specialProfilingBuffer != NULL)
      {
      // Aligned address is used instead of the one returned by malloc. Pending FIX.
      // if (NULL != specialProfilingBuffer->getBuffer())
         // TR_Memory::jitPersistentFree(specialProfilingBuffer->getBuffer());

      specialProfilingBuffer->setBuffer(NULL);
      specialProfilingBuffer->setSize(0);
      _workingBufferList.add(specialProfilingBuffer);
      _workingBufferTail = specialProfilingBuffer;
      // wait for the profiler thread to stop
      while (!_hwProfilerThreadExitFlag)
         {
         _hwProfilerMonitor->notifyAll();
         _hwProfilerMonitor->wait();
         }
      }

   _hwProfilerMonitor->exit();
   }

void
TR_HWProfiler::processWorkingQueue()
   {
   _hwProfilerMonitor->enter();
   while (true)
      {
      while (_workingBufferList.isEmpty())
         {
         _hwProfilerMonitor->wait();
         }
      // We have some buffer to process
      // Dequeue the buffer to be processed
      //
      _currentBufferBeingProcessed = _workingBufferList.pop();
      if (_workingBufferList.isEmpty())
         _workingBufferTail = NULL;

      // We don't need the monitor now while processing data.
      _hwProfilerMonitor->exit();

      if (_currentBufferBeingProcessed->getSize() > 0)
         {
         // process the buffer after acquiring VM access
         acquireVMAccessNoSuspend(_hwProfilerThread);   // blocking. Will wait for the entire GC
         if (_currentBufferBeingProcessed->isValid())
            {
            processBufferRecords(_hwProfilerThread,
                                 _currentBufferBeingProcessed->getBuffer(),
                                 _currentBufferBeingProcessed->getSize(),
                                 _currentBufferBeingProcessed->getBufferFilledSize(),
                                 _currentBufferBeingProcessed->getType());
            }
         releaseVMAccessNoSuspend(_hwProfilerThread);
         }
      else // Special
         {
         break;
         }

      // attach processed buffer to free list.
      _hwProfilerMonitor->enter();
      _freeBufferList.add(_currentBufferBeingProcessed);
      _currentBufferBeingProcessed = NULL;
      _numOutstandingBuffers--;
      }
   }

void
TR_HWProfiler::invalidateProfilingBuffers() // called for class unloading
   {
   // GC has exclusive access, but needs to acquire _hwProfilerMonitor
   if (!_hwProfilerMonitor)
      return;

   if (!getHWProfilerThread())
      return;

   _hwProfilerMonitor->enter();

   HWProfilerBuffer *specialProfilingBuffer = NULL;
   if (_currentBufferBeingProcessed && _currentBufferBeingProcessed->getSize() > 0)
      {
      // mark this buffer as invalid
      _currentBufferBeingProcessed->setIsInvalidated(true); // set with exclusive VM access
      }
   while (!_workingBufferList.isEmpty())
      {
      HWProfilerBuffer *profilingBuffer = _workingBufferList.pop();
      if (profilingBuffer->getSize() > 0)
         {
         // attach the buffer to the buffer pool
         _freeBufferList.add(profilingBuffer);
         _numOutstandingBuffers--;
         }
      else // When the hwprofiler thread sees this special buffer it will exit
         {
         specialProfilingBuffer = profilingBuffer;
         }
      }
   _workingBufferTail = NULL; // queue should be empty now

   if (specialProfilingBuffer)
      {
      // Put this buffer back
      _workingBufferList.add(specialProfilingBuffer);
      _workingBufferTail = specialProfilingBuffer;
      }
   _hwProfilerMonitor->exit();
   }


U_8 *
TR_HWProfiler::swapBufferToWorkingQueue(const U_8* dataStart, UDATA size,  UDATA bufferFilledSize, uint32_t dataTag, bool allocateNewBuffer)
   {
   if (!_hwProfilerMonitor)
      return NULL;

   if (!getHWProfilerThread())
      return NULL;

   if (_hwProfilerMonitor->try_enter())
      return NULL;

   // If the profiling thread has already been destroyed, delegate the processing to the java thread
   if (_hwProfilerThreadExitFlag)
      {
      _hwProfilerMonitor->exit();
      return NULL;
      }

   // Grab a free buffer to associate with given data.
   HWProfilerBuffer *newHWProfilerBuffer = NULL;
   if (allocateNewBuffer)
       newHWProfilerBuffer = _freeBufferList.pop();

   if (NULL == newHWProfilerBuffer)
      {
      /*
      // This may only be needed when HW Profiling is on but the HW Thread is disabled
      if(_numOutstandingBuffers >= TR::Options::_hwprofilerNumOutstandingBuffers)
         {
         _hwProfilerMonitor->exit();
         return NULL;   //Delegate to application thread.
         }
      */

      U_8* newBuffer = NULL;
      if (allocateNewBuffer)
         newBuffer = (U_8*)allocateBuffer(size);

      if (NULL == newBuffer && allocateNewBuffer)
         {
         _hwProfilerMonitor->exit();
         return NULL;
         }

      newHWProfilerBuffer = (HWProfilerBuffer*)TR_Memory::jitPersistentAlloc(sizeof(HWProfilerBuffer));
      if (NULL == newHWProfilerBuffer)
         {
         if (allocateNewBuffer)
            freeBuffer(newBuffer, size);
         _hwProfilerMonitor->exit();
         return NULL;
         }
      newHWProfilerBuffer->setBuffer(newBuffer);
      }

   // Save pointer to new buffer to be stored.
   U_8 *newDataStart = (U_8*)newHWProfilerBuffer->getBuffer();

   //--- link current buffer to the processing list
   newHWProfilerBuffer->setBuffer((U_8*)dataStart);
   newHWProfilerBuffer->setSize(size);
   newHWProfilerBuffer->setBufferFilledSize(bufferFilledSize);
   newHWProfilerBuffer->setIsInvalidated(false);
   newHWProfilerBuffer->setType(dataTag);
   _workingBufferList.insertAfter(_workingBufferTail, newHWProfilerBuffer);
   _workingBufferTail = newHWProfilerBuffer;

   _numOutstandingBuffers++;

   _hwProfilerMonitor->notifyAll();

   _hwProfilerMonitor->exit();
   return newDataStart;
   }

void TR_HWProfiler::turnBufferProcessingOffTemporarily()
   {
   setProcessBufferState(-1);
   _numDowngradesSinceTurnedOff = 0;
   }

void TR_HWProfiler::restoreBufferProcessingFunctionality()
   {
   setProcessBufferState(TR::Options::_hwProfilerRIBufferProcessingFrequency);
   }

bool TR_HWProfiler::checkAndTurnBufferProcessingOff()
   {
   if (TR::Options::getCmdLineOptions()->getOption(TR_UseRIOnlyForLargeQSZ) &&
       _compInfo->getMethodQueueSize() > TR::Options::_qszMinThresholdToRIDowngrade)
      {
      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "t=%6u RI continue because QSZ is large: %d\n",
                                        (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                                        _compInfo->getMethodQueueSize());
         }
      return false; // keep RI on because the queue is large and we will be downgrading
      }

   uint64_t newRecompDecisionsTotal = _recompDecisionsTotal - _recompDecisionsTotalStart;
   uint64_t newRecompDecisionsYes   = _recompDecisionsYes   - _recompDecisionsYesStart;

   if (newRecompDecisionsTotal >= TR::Options::_hwProfilerRecompDecisionWindow)
      {
      // memorize the new state
      _recompDecisionsTotalStart = _recompDecisionsTotal;
      _recompDecisionsYesStart   = _recompDecisionsYes;
      // If density of decisions to recompile is low, signal to stop RI processing
      if (TR::Options::_hwProfilerRecompFrequencyThreshold * newRecompDecisionsYes < newRecompDecisionsTotal) // avoid float arithmetic
         {
         turnBufferProcessingOffTemporarily();
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
            {
            float recompFrequency = (float)newRecompDecisionsYes / (float)newRecompDecisionsTotal;
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "t=%6u RI buffer processing disabled because recomp frequency is %.4f newRecompDecisionsTotal=%llu\n",
                                           (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                                           recompFrequency, newRecompDecisionsTotal);
            }
         return true;
         }
      else // continue to have RI on; TODO: delete this code
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
            {
            float recompFrequency = (float)newRecompDecisionsYes / (float)newRecompDecisionsTotal;
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "t=%6u RI continue. recomp frequency is %.4f newRecompDecisionsTotal=%llu\n",
                                           (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                                           recompFrequency, newRecompDecisionsTotal);
            }
         }
      }
   return false;
   }

// caller must check whether RI is on and state is OFF
bool TR_HWProfiler::checkAndTurnBufferProcessingOn()
   {
   if (TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeWhenRIIsTemporarilyOff))
      {
      // Turn RI on if queue size grows too much
      if (_compInfo->getMethodQueueSize() > TR::Options::_qszThresholdToTurnRION)
         {
         restoreBufferProcessingFunctionality();
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "RI buffer processing re-enabled because Q_SZ=%d\n", _compInfo->getMethodQueueSize());
            }
         return true;
         }
      }
   else // Must look at the number of downgraded compilations since we turned RI off
      {
      if (_numDowngradesSinceTurnedOff > TR::Options::_numDowngradesToTurnRION)
         {
         restoreBufferProcessingFunctionality();
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER,
                                           "RI buffer processing re-enabled because we downgraded %d methods at cold since RI was turned off\n",
                                           _numDowngradesSinceTurnedOff);
            }
         return true;
         }
      }

   return false;
   }

bool
TR_HWProfiler::recompilationLogic(TR_PersistentJittedBodyInfo *bodyInfo,
                                  void *startPC,
                                  uint64_t startCount,
                                  uint64_t count,
                                  uint64_t totalCount,
                                  TR_FrontEnd *fe, J9VMThread *vmThread)
   {
   uint64_t profiledInterval = totalCount - startCount;

   // Check if we already induced a recompilation for this method
   if (bodyInfo->_hwpInducedRecompilation && !bodyInfo->_hwpReducedWarmCompileRequested)
      return true;

   // Check whether we reached a decision point
   if (profiledInterval < _recompilationInterval)
      return false;
   // We reached a decision point
   _recompDecisionsTotal++;

   // Check if sampling is disabled
   if (bodyInfo->getDisableSampling())
      {
      return true;
      }

   if (bodyInfo->getHotness() >= _lastOptLevel && !bodyInfo->getReducedWarm() &&
       !(bodyInfo->getIsAotedBody() && !TR::Options::getCmdLineOptions()->getOption(TR_DontRIUpgradeAOTWarmMethods)))
      {
      return true;
      }

   TR_ASSERT(count!=0 && totalCount!=0, "Number of RI samples cannot be 0");
   TR_ASSERT(profiledInterval != 0, "RI profiling interval cannot be 0");

   TR_Hotness nextOptLevel = noOpt;
   bool doHWPReducedWarmCompile = false;

   // Check how hot the method is
   float hotness = (float) count / (float) profiledInterval;
   if (_lastOptLevel == warm)
      {
      if (bodyInfo->getHotness() <= cold || bodyInfo->getReducedWarm())
         {
         if (hotness > _warmOptLevelThreshold)
            {
            nextOptLevel = warm;
            }
         else if (hotness > _reducedWarmOptLevelThreshold &&
                  !TR::Options::getCmdLineOptions()->getOption(TR_DisableHardwareProfilerReducedWarm))
            {
            nextOptLevel = warm;
            doHWPReducedWarmCompile = true;
            }
         else
            {
            return true;
            }
         }
      else if (bodyInfo->getIsAotedBody() && hotness > _aotWarmOptLevelThreshold)
         {
         nextOptLevel = warm;
         }
      else
         {
         return true;
         }
      }
   else
      {
      return true; // Start a new interval
      }

   // TODO: Why method info can be null with AOT?
   if (bodyInfo->getMethodInfo() == NULL || bodyInfo->getMethodInfo()->getMethodInfo() == NULL)
      return true; // Start a new interval
   // Above I don't increment _noRecompDecisions because this is an error case that should not happen

   TR_OpaqueMethodBlock *method = bodyInfo->getMethodInfo()->getMethodInfo();

   // Check if we need to upgrade the queued reduced warm compile to warm
   if (bodyInfo->_hwpInducedRecompilation &&
       bodyInfo->_hwpReducedWarmCompileRequested)
      {
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableHardwareProfilerReducedWarmUpgrades) &&
          !doHWPReducedWarmCompile &&
          bodyInfo->_hwpReducedWarmCompileInQueue)
         {
         TR_MethodToBeCompiled *cur = NULL;
         TR::IlGeneratorMethodDetails details((J9Method *)method);

         _compInfo->acquireCompMonitor(vmThread);
         // Check again in case another thread has already upgraded this request
         if (bodyInfo->_hwpReducedWarmCompileInQueue)
            {
            for (cur = _compInfo->getMethodQueue(); cur; cur = cur->_next)
               {
               if (cur->getMethodDetails().sameAs(details, fe))
                  break;
               }

            if (cur)
               {
               cur->_optimizationPlan->setIsHwpDoReducedWarm(false);
               bodyInfo->_hwpReducedWarmCompileRequested = false;
               _numReducedWarmRecompilationsUpgraded++;
               }

            bodyInfo->_hwpReducedWarmCompileInQueue = false;
            }
         _compInfo->releaseCompMonitor(vmThread);
         }

      return true;
      }

   TR_MethodEvent event;
   event._eventType = TR_MethodEvent::HWPRecompilationTrigger;
   event._j9method = (J9Method *) method;
   event._oldStartPC = startPC;
   event._samplePC = NULL;
   event._vmThread = vmThread;
   event._classNeedingThunk = 0;
   event._nextOptLevel = nextOptLevel;

   bool newPlanCreated;
   bool queued = false;

   TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
   if (plan)
      {
      if (doHWPReducedWarmCompile)
         plan->setIsHwpDoReducedWarm(true);

      _recompDecisionsYes++;
      bool rc = TR::Recompilation::induceRecompilation(fe, startPC, &queued, plan);
      if (!queued && newPlanCreated)
         TR_OptimizationPlan::freeOptimizationPlan(plan);

      if (rc)
         {
         bodyInfo->_hwpInducedRecompilation = true;

         if (doHWPReducedWarmCompile)
            {
            bodyInfo->_hwpReducedWarmCompileRequested = true;
            bodyInfo->_hwpReducedWarmCompileInQueue = true;
            _numReducedWarmRecompilationsInduced++;
            }

         _numRecompilationsInduced++;
         _STATS_NumUpgradesDueToRI++;
         }
      }

   return true; // Start a new interval
   }


uintptrj_t
TR_HWProfiler::getPCFromBCInfo(TR::Node *node, TR::Compilation *comp)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_OpaqueMethodBlock* method = getMethodFromBCInfo(bcInfo, comp);

   return getPCFromMethodAndBCIndex(method, bcInfo.getByteCodeIndex(), comp);
   }

TR_OpaqueMethodBlock *
TR_HWProfiler::getMethodFromBCInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   TR_OpaqueMethodBlock *method = NULL;

   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      if (bcInfo.getCallerIndex() >= 0)
         method = (TR_OpaqueMethodBlock *)(((TR_AOTMethodInfo *)comp->getInlinedCallSite(bcInfo.getCallerIndex())._vmMethodInfo)->resolvedMethod->getNonPersistentIdentifier());
      else
         method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getNonPersistentIdentifier());
      }
   else
      {
      if (bcInfo.getCallerIndex() >= 0)
         method = (TR_OpaqueMethodBlock *)(comp->getInlinedCallSite(bcInfo.getCallerIndex())._vmMethodInfo);
      else
         method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());
      }

   return method;
   }

TR_HWPBytecodePCToIAMap
TR_HWProfiler::createBCMap(uint8_t *ia, uint32_t bcIndex, TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   TR_HWPBytecodePCToIAMap map;
   map._instructionAddr = (void *)ia;
   map._bytecodePC      = (void *)getPCFromMethodAndBCIndex(method, bcIndex, comp);
   return map;
   }

uintptrj_t
TR_HWProfiler::getBytecodePCFromIA(J9VMThread *vmThread, uint8_t *IA)
   {
   if (vmThread)
      {
      J9JITExceptionTable *metaData = _jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA) IA);
      if (metaData &&
          metaData->riData &&
          ((TR_HWPBytecodePCToIAMap *)metaData->riData)->_bytecodePC == (void *)METADATA_MAPPING_EYECATCHER)
         {
         TR_HWPBytecodePCToIAMap *cursor = (TR_HWPBytecodePCToIAMap *)metaData->riData;
         uintptr_t arraySize = (uintptr_t)cursor->_instructionAddr;

         cursor++;
         for (uint32_t i = 0; i < arraySize; i++, cursor++)
            {
            if ((uint8_t *)cursor->_instructionAddr == IA)
               {
#if defined (RI_VP_Verbose)
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler) &&
                   metaData->bodyInfo)
                  {
                  TR_PersistentJittedBodyInfo *bodyInfo = (TR_PersistentJittedBodyInfo *)metaData->bodyInfo;
                  if (bodyInfo->getMethodInfo() && bodyInfo->getMethodInfo()->getMethodInfo())
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Found bytecodePC=0x%p from metadata=0x%p of j9method=0x%p",
                                                    cursor->_bytecodePC,
                                                    metaData,
                                                    bodyInfo->getMethodInfo()->getMethodInfo());
                     }
                  }
#endif
               return (uintptrj_t)cursor->_bytecodePC;
               }
            }
         }
#if defined (RI_VP_Verbose)
      else
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "metaData=0x%p, metaData->raData=0x%p, or eyecatcher invalid",
                                           metaData,
                                           metaData ? metaData->riData : NULL);
            }
         }
#endif
      }

   return 0;
   }

void
TR_HWProfiler::registerRecords(J9JITExceptionTable *metaData, TR::Compilation *comp)
   {
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHardwareProfileIndirectDispatch) &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableMetadataBytecodePCToIAMap) &&
       metaData->riData)
      {
      void *bytecodePCToIAMapLocation         = metaData->riData;
      TR_HWPBytecodePCToIAMap* cursor         = (TR_HWPBytecodePCToIAMap *)bytecodePCToIAMapLocation;
      TR_Array<TR_HWPBytecodePCToIAMap> *maps = comp->getHWPBCMap();
      uint32_t arraySize                      = maps->size();

      // Initialize the special first element
      cursor->_bytecodePC = (void *)METADATA_MAPPING_EYECATCHER;
      cursor->_instructionAddr = (void *)(uintptrj_t)arraySize;
      cursor++;

      for (uint32_t i = 0; i < arraySize; i++, cursor++)
         {
         *cursor = maps->element(i);
         }

      // Keep track of the amount of memory used by the Metadata Mapping
      updateMemoryUsedByMetadataMapping((arraySize + 1) * sizeof(TR_HWPBytecodePCToIAMap));
      }
   }

void
TR_HWProfiler::createRecords(TR::Compilation *comp)
   {
   if (!comp->getPersistentInfo()->isRuntimeInstrumentationEnabled() ||
       comp->isProfilingCompilation() ||
       comp->getMethodHotness() == scorching)
      {
      return;
      }

   TR_Array<TR_HWPInstructionInfo> *hwpInstructionInfos = comp->getHWPInstructions();
   TR::CodeGenerator *cg = comp->cg();

   for (int32_t i = 0; i < hwpInstructionInfos->size(); i++)
      {
      TR::Instruction                  *instruction            = (TR::Instruction *) hwpInstructionInfos->element(i)._instruction;
      TR_HWPInstructionInfo::type       hwpInstructionType     = hwpInstructionInfos->element(i)._type;
      TR::Node                         *node                   = instruction->getNode();
      uint8_t                          *ia                     = instruction->getBinaryEncoding();
      TR_ExternalRelocationTargetKind   relocationTargetKind   = TR_NoRelocation;
      uint32_t                          bcIndex                = node->getByteCodeIndex();
      TR_OpaqueMethodBlock             *method                 = node->getOwningMethod();
      uint8_t                          *target                 = (uint8_t *) &node->getByteCodeInfo();
      uint8_t                          *target2                = NULL;

      switch (hwpInstructionType)
         {
         case TR_HWPInstructionInfo::valueProfileInstructions:
            {

            relocationTargetKind = TR_EmitClass;
            target2 = (uint8_t *) ((intptrj_t) node->getInlinedSiteIndex());
            TR_HWPBytecodePCToIAMap map = {(void *)getPCFromMethodAndBCIndex(method, bcIndex, comp),
                                           (void *)ia};
            comp->addHWPBCMap(map);
            }
            break;
         default:
            TR_ASSERT (false, "Unknown or Unsupported HWP Instruction Type");
         }

      TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fej9());
      if (!TR::Options::getCmdLineOptions()->getOption(TR_HWProfilerDisableAOT) &&
          fej9->hardwareProfilingInstructionsNeedRelocation())
         {
         TR::ExternalRelocation *relocation =
               new (comp->trHeapMemory()) TR::ExternalRelocation(ia,
                                                                target,
                                                                target2,
                                                                relocationTargetKind,
                                                                cg);

         cg->addExternalRelocation(relocation, __FILE__, __LINE__, node);
         }
      }
   }

void
TR_HWProfiler::printStats()
   {
   printf("Number of recompilations induced = %llu\n",                   _numRecompilationsInduced);
   printf("Number of reduced warm recompilations induced = %llu\n",      _numReducedWarmRecompilationsInduced);
   printf("Number of reduced warm recompilations upgraded = %llu\n",     _numReducedWarmRecompilationsUpgraded);
   printf("Number of recompilations induced due to jitSampling = %d\n",   TR::Recompilation::jitRecompilationsInduced);
   printf("TR::Recompilation::jitGlobalSampleCount = %d\n",               TR::Recompilation::jitGlobalSampleCount);
   printf("TR::Recompilation::hwpGlobalSampleCount = %d\n",               TR::Recompilation::hwpGlobalSampleCount);
   printf("Number of buffers completely filled = %llu\n",                _numBuffersCompletelyFilled);
   printf("Average buffer filled percentage = %f\n",                     _bufferSizeSum ? (((float)_bufferFilledSum) / ((float)_bufferSizeSum) * 100) : 0);
   printf("Number of requests = %llu\n",                                 _numRequests);
   printf("Number of requests skipped = %llu\n",                         _numRequestsSkipped);
   printf("Memory used by metadata bytecodePC to IA mapping = %llu B\n", _totalMemoryUsedByMetadataMapping);
   printf("Total buffers processed = %llu\n",                            _STATS_TotalBuffersProcessed);
   printf("Total buffers processed by App Thread= %llu\n",               _STATS_BuffersProcessedByAppThread);
   printf("Total event records: %llu\n",                                 _STATS_TotalEntriesProcessed);
   printf("Total instructions tracked: %u\n",                            _STATS_TotalInstructionsTracked);
   printf("Total downgrades due to RI: %u\n",                            _STATS_NumCompDowngradesDueToRI);
   printf("Total upgrades due to RI: %u\n",                              _STATS_NumUpgradesDueToRI);
   printf("\n");
   }
