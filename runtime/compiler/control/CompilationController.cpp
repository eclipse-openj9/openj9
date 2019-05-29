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

#include "control/CompilationController.hpp"

#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Monitor.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/ut_j9jit.h"
#include "env/CompilerEnv.hpp"

// NOTE: TR::CompilationController is actually defined in control/OptimizationPlan.hpp

TR::CompilationStrategy *TR::CompilationController::_compilationStrategy = NULL;
TR::CompilationInfo *    TR::CompilationController::_compInfo = 0;
int32_t                 TR::CompilationController::_verbose = 0;
bool                    TR::CompilationController::_useController = false;



//------------------------------------ init -----------------------------------
// Initializes the compilationController.
// Return false if it fails
//-----------------------------------------------------------------------------
bool TR::CompilationController::init(TR::CompilationInfo *compInfo)
   {
   _useController = false; // Default to failure
   _compilationStrategy = 0; // Default to failure
   TR::Options *options = TR::Options::getCmdLineOptions();
   char *strategyName = options->getCompilationStrategyName();

   if (strategyName && strcmp(strategyName, "none"))
      {
      _compInfo = compInfo;
      if (strcmp(strategyName, "default") == 0)
         _compilationStrategy = new (PERSISTENT_NEW) TR::DefaultCompilationStrategy();
      else if (strcmp(strategyName, "threshold") == 0)
         _compilationStrategy = new (PERSISTENT_NEW) TR::ThresholdCompilationStrategy();
      else // if no match, use default
      {
         _compilationStrategy = new (PERSISTENT_NEW) TR::DefaultCompilationStrategy();
      }

      if (_compilationStrategy)
         {
         TR_OptimizationPlan::_optimizationPlanMonitor = TR::Monitor::create("OptimizationPlanMonitor");
         _useController = (TR_OptimizationPlan::_optimizationPlanMonitor != 0);
         if (_useController)
            {
            static char *verboseController = feGetEnv("TR_VerboseController");
            if (verboseController)
               setVerbose(atoi(verboseController));
            if (verbose() >= LEVEL1)
               fprintf(stderr, "Using %s comp strategy\n", strategyName);
            }
         }
      }
   //TR_ASSERT(_useController, "Must use compilation controller");
//#ifdef COMP_YIELD_ANALYSIS
   if (options->getOption(TR_EnableCompYieldStats))
      TR::Compilation::allocateCompYieldStatsMatrix();
   tlsAlloc(OMR::compilation);
   return _useController;
   }


//-------------------------------- shutdown ---------------------------------
// Called at shutdown time after compilation thread has been stopped
// --------------------------------------------------------------------------
void TR::CompilationController::shutdown()
   {
   tlsFree(OMR::compilation);
   if (!_useController)
      return;
   // would like to free all entries in the pool of compilation plans
   int32_t remainingPlans = TR_OptimizationPlan::freeEntirePool();
      // print some stats
   if (verbose() >= LEVEL1)
      {
      fprintf(stderr, "Remaining optimizations plans in the system: %d\n", remainingPlans);
      }
   _compilationStrategy->shutdown();
   }


//======================== DefaultCompilationStrategy ==========================



TR::DefaultCompilationStrategy::DefaultCompilationStrategy()
   {
   // initialize the statistics
   for (int32_t i=0; i < TR_MethodEvent::NumEvents; i++)
      _statEventType[i] = 0;
   }


void TR::DefaultCompilationStrategy::shutdown()
   {
   // printing stats
   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL1)
      {
      fprintf(stderr, "Stats for type of events:\n");
      for (int32_t i=0; i < TR_MethodEvent::NumEvents; i++)
         fprintf(stderr, "EventType:%d cases:%u\n", i, _statEventType[i]);
      }
   }


TR_Hotness TR::DefaultCompilationStrategy::getInitialOptLevel(J9Method *j9method)
   {
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
   return TR::Options::getInitialHotnessLevel(J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? true : false);
   }


//------------------------------- processEvent ------------------------
// If the function returns NULL, then the value of *newPlanCreated is
// undefined and should not be tested
//---------------------------------------------------------------------
TR_OptimizationPlan *TR::DefaultCompilationStrategy::processEvent(TR_MethodEvent *event, bool *newPlanCreated)
   {
   TR_OptimizationPlan *plan = NULL, *attachedPlan = NULL;
   TR_Hotness hotnessLevel;
   TR_PersistentJittedBodyInfo *bodyInfo;
   TR_PersistentMethodInfo *methodInfo;
   TR::CompilationInfo *compInfo = TR::CompilationController::getCompilationInfo();

   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL3)
      fprintf(stderr, "Event %d\n", event->_eventType);

   // first decode the event type
   switch (event->_eventType)
      {
      case TR_MethodEvent::JittedMethodSample:
         compInfo->_stats._sampleMessagesReceived++;
         plan = processJittedSample(event);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::InterpretedMethodSample:
         compInfo->_stats._sampleMessagesReceived++;
         plan = processInterpreterSample(event);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::InterpreterCounterTripped:
         TR_ASSERT(event->_oldStartPC == 0, "oldStartPC should be 0 for an interpreted method");
         compInfo->_stats._methodsCompiledOnCount++;
         // most likely we need to compile the method, unless it's already being compiled
         // even if the method is already queued for compilation we must still invoke
         // compilemethod because we may need to do a async compilation and the thread
         // needs to block

         // use the counts to determine the first level of compilation
         // the level of compilation can be changed later on if option subsets are present
         hotnessLevel = TR::DefaultCompilationStrategy::getInitialOptLevel(event->_j9method);
         if (hotnessLevel == veryHot && // we probably want to profile
            !TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling) &&
             TR::Recompilation::countingSupported() &&
            !TR::CodeCacheManager::instance()->almostOutOfCodeCache())
            plan = TR_OptimizationPlan::alloc(hotnessLevel, true, false);
         else
            plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         // the optimization plan needs to include opt level and if we do profiling
         // these may change
         break;
      case TR_MethodEvent::JitCompilationInducedByDLT:
         hotnessLevel = TR::DefaultCompilationStrategy::getInitialOptLevel(event->_j9method);
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         if (plan)
            plan->setInducedByDLT(true);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::OtherRecompilationTrigger: // sync recompilation through fixMethodCode or recomp triggered from jitted code (like counting recompilation)
         // For sync re-compilation we have attached a plan to the persistentBodyInfo
         bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(event->_oldStartPC);
         methodInfo = bodyInfo->getMethodInfo();

         if (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToInlinedMethodRedefinition ||
             (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToJProfiling && !bodyInfo->getIsProfilingBody())) // if the recompilation is triggered from a JProfiling block but not in a profiled compilation keep the current compilation level unchanged
            {
            hotnessLevel = bodyInfo->getHotness();
            plan = TR_OptimizationPlan::alloc(hotnessLevel);
            *newPlanCreated = true;
            }
         else
            {
            hotnessLevel = TR::Recompilation::getNextCompileLevel(event->_oldStartPC);
            plan = TR_OptimizationPlan::alloc(hotnessLevel);
            *newPlanCreated = true;
            }
         
         TR_OptimizationPlan::_optimizationPlanMonitor->enter();
         attachedPlan = methodInfo->_optimizationPlan;
         if (attachedPlan)
            {
            TR_ASSERT(!TR::CompilationController::getCompilationInfo()->asynchronousCompilation(),
                   "This case should happen only for sync recompilation");
            plan->clone(attachedPlan); // override
            }
         TR_OptimizationPlan::_optimizationPlanMonitor->exit();
         break;
      case TR_MethodEvent::NewInstanceImpl:
         // use the counts to determine the first level of compilation
         // the level of compilation can be changed later on if option subsets are present
         hotnessLevel = TR::Options::getInitialHotnessLevel(false);
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::ShareableMethodHandleThunk:
      case TR_MethodEvent::CustomMethodHandleThunk:
         // TODO: methodInfo->setWasNeverInterpreted()
         hotnessLevel = TR::DefaultCompilationStrategy::getInitialOptLevel(event->_j9method);
         if (hotnessLevel < warm && event->_eventType == TR_MethodEvent::CustomMethodHandleThunk)
            hotnessLevel = warm; // Custom thunks benefit a LOT from warm opts like preexistence and repeated inlining passes
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         // plan->setIsForcedCompilation(); // TODO:JSR292: Seems reasonable, but somehow it crashes
         plan->setUseSampling(false);  // We don't yet support sampling-based recompilation of MH thunks
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::MethodBodyInvalidated:
         // keep the same optimization level
         bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(event->_oldStartPC);
         TR_ASSERT(bodyInfo, "A recompilable method should have jittedBodyInfo");
         hotnessLevel = bodyInfo->getHotness();
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         bodyInfo->getMethodInfo()->incrementNumberOfInvalidations();

         // the following is just for compatibility with older implementation
         //bodyInfo->getMethodInfo()->setNextCompileLevel(hotnessLevel, false); // no profiling
         break;
      case TR_MethodEvent::HWPRecompilationTrigger:
         {
         plan = processHWPSample(event);
         }
         break;
      default:
         TR_ASSERT(0, "Bad event type %d", event->_eventType);
      }

   _statEventType[event->_eventType]++;  // statistics

   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL2)
      fprintf(stderr, "Event %d created plan %p\n", event->_eventType, plan);

   return plan;
   }



//--------------------- processInterpreterSample ----------------------
TR_OptimizationPlan *
TR::DefaultCompilationStrategy::processInterpreterSample(TR_MethodEvent *event)
   {
   // Sampling an interpreted method. The method could have been already
   // compiled (but we got a sample in the old interpreted body).
   //
   TR_OptimizationPlan *plan = 0;
   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   J9Method *j9method = event->_j9method;
   J9JITConfig *jitConfig = event->_vmThread->javaVM->jitConfig;
   TR::CompilationInfo *compInfo = 0;
   if (jitConfig)
      compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, event->_vmThread);

   int32_t totalSampleCount = TR::Recompilation::globalSampleCount;
   char msg[350];  // size should be big enough to hold the whole one-line msg
   msg[0] = 0;
   char *curMsg = msg;
   bool logSampling = fe->isLogSamplingSet() || TrcEnabled_Trc_JIT_Sampling_Detail;
#define SIG_SZ 150
   char sig[SIG_SZ];  // hopefully the size is good for most cases

   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
   bool loopy = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? true : false;

   if (logSampling || TrcEnabled_Trc_JIT_Sampling)
      {
      fe->printTruncatedSignature(sig, SIG_SZ, (TR_OpaqueMethodBlock*)j9method);

      if (logSampling)
         curMsg += sprintf(curMsg, "(%d)\tInterpreted %s\t", totalSampleCount, sig);
      if (TrcEnabled_Trc_JIT_Sampling && ((totalSampleCount % 4) == 0))
         Trc_JIT_Sampling(getJ9VMThreadFromTR_VM(fe), "Interpreted", sig, 0);
      }

      compInfo->_stats._interpretedMethodSamples++;

      if (!TR::CompilationInfo::isCompiled(j9method))
         {
         int32_t count = TR::CompilationInfo::getInvocationCount(j9method);
         // the count will be -1 for JNI or if extra is negative
         if (!cmdLineOptions->getOption(TR_DisableInterpreterSampling))
            {
            // If the method is an interpreted non-JNI method, the last slot in
            // the RAM method is an invocation count. See if it is reasonable
            // to reduce the invocation count since this method has been sampled.
            //
            if (count > 0)
               {
               int32_t threshold, divisor;
               /* Modify thresholds for JSR292 methods */
               bool isJSR292Method = _J9ROMMETHOD_J9MODIFIER_IS_SET((J9_ROM_METHOD_FROM_RAM_METHOD(j9method)), J9AccMethodHasMethodHandleInvokes );
               if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
                  {
                  threshold = isJSR292Method ? TR::Options::_interpreterSamplingThresholdInJSR292 : TR::Options::_interpreterSamplingThresholdInStartupMode;
                  divisor = TR::Options::_interpreterSamplingDivisorInStartupMode;
                  }
                else
                  {
                  threshold = isJSR292Method ? TR::Options::_interpreterSamplingThresholdInJSR292 : TR::Options::_interpreterSamplingThreshold;
                  divisor = TR::Options::_interpreterSamplingDivisor;
                  }
               int32_t activeThreadsThreshold = TR::Options::_activeThreadsThreshold;
               if (activeThreadsThreshold == -1) // -1 means we want to determine this dynamically
                  activeThreadsThreshold = compInfo->getNumAppThreadsActive();

               if (count <= threshold && count > activeThreadsThreshold)
                  {
                  // This is an interpreted method that can be compiled.
                  // Reduce the invocation count.
                  //
                  int32_t newCount = count / divisor;
                  // Don't decrement more than the number of active threads
                  if (newCount < activeThreadsThreshold)
                      newCount = activeThreadsThreshold;
                  if (TR::CompilationInfo::setInvocationCount(j9method, count, newCount))
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " reducing count %d --> %d", count, newCount);
                     if (cmdLineOptions->getOption(TR_UseSamplingJProfilingForInterpSampledMethods))
                        compInfo->getInterpSamplTrackingInfo()->addOrUpdate(j9method, count - newCount);
                     }
                  else
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " count = %d, already changed", count);
                     }

                  // If the method is ready to be compiled and we are using a separate
                  // compilation thread, get a head start by scheduling the compilation
                  // now
                  //
                  if (newCount == 0 && fe->isAsyncCompilation())
                     {
                     if (TR::Options::_compilationDelayTime <= 0 ||
                        compInfo->getPersistentInfo()->getElapsedTime() >= 1000 * TR::Options::_compilationDelayTime)
                        plan = TR_OptimizationPlan::alloc(getInitialOptLevel(j9method));
                     }
                  }
               else if (returnIprofilerState() == IPROFILING_STATE_OFF)
                  {
                  int32_t newCount = 0;
                  if (cmdLineOptions->getOption(TR_SubtractMethodCountsWhenIprofilerIsOff))
                     newCount = count - TR::Options::_IprofilerOffSubtractionFactor;
                  else
                     newCount = count / TR::Options::_IprofilerOffDivisionFactor;

                  if (newCount < 0)
                     newCount = 0;

                  if (TR::CompilationInfo::setInvocationCount(j9method, count, newCount))
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " reducing count %d --> %d", count, newCount);
                     if (cmdLineOptions->getOption(TR_UseSamplingJProfilingForInterpSampledMethods))
                        compInfo->getInterpSamplTrackingInfo()->addOrUpdate(j9method, count - newCount);
                     }
                  else
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " count = %d, already changed", count);
                     }
                  }
               else if (loopy && count > activeThreadsThreshold)
                  {
                  int32_t newCount = 0;
                  if (cmdLineOptions->getOption(TR_SubtractLoopyMethodCounts))
                     newCount = count - TR::Options::_LoopyMethodSubtractionFactor;
                  else
                     newCount = count / TR::Options::_LoopyMethodDivisionFactor;

                  if (newCount < 0)
                     newCount = 0;
                   if (newCount < activeThreadsThreshold)
                      newCount = activeThreadsThreshold;
                  if (TR::CompilationInfo::setInvocationCount(j9method, count, newCount))
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " reducing count %d --> %d", count, newCount);
                     if (cmdLineOptions->getOption(TR_UseSamplingJProfilingForInterpSampledMethods))
                        compInfo->getInterpSamplTrackingInfo()->addOrUpdate(j9method, count - newCount);
                     }
                  else
                     {
                     if (logSampling)
                        curMsg += sprintf(curMsg, " count = %d, already changed", count);
                     }
                  }
               else
                  {
                  if (logSampling)
                     curMsg += sprintf(curMsg, " count = %d / %d", count, threshold);
                  }
               }
            else if (count == 0)
               {
               // Possible scenario: a long activation method receives a MIL count of 1.
               // The method gets invoked and the count becomes 0 (but the compilation is not
               // triggered now, only when the counter would become negative).
               // The method receives a sample while still being interpreted. We should probably
               // schedule a compilation
               if (logSampling)
                  curMsg += sprintf(curMsg, " count = 0 (long running?)");
               if (fe->isAsyncCompilation())
                  {
                  if (TR::Options::_compilationDelayTime <= 0 ||
                     compInfo->getPersistentInfo()->getElapsedTime() >= 1000 * TR::Options::_compilationDelayTime)
                     plan = TR_OptimizationPlan::alloc(getInitialOptLevel(j9method));
                  }
               }
            else // count==-1
               {
               if (TR::CompilationInfo::getJ9MethodVMExtra(j9method) == J9_JIT_QUEUED_FOR_COMPILATION)
                  {
                  if (logSampling)
                      curMsg += sprintf(curMsg, " already queued");
                  if (compInfo &&
                      (compInfo->compBudgetSupport() || compInfo->dynamicThreadPriority()))
                     {
                     fe->acquireCompilationLock();
                     int32_t n = compInfo->promoteMethodInAsyncQueue(j9method, 0);
                     fe->releaseCompilationLock();
                     if (logSampling)
                        {
                        if (n > 0)
                           curMsg += sprintf(curMsg, " promoted from %d", n);
                        else if (n == 0)
                           curMsg += sprintf(curMsg, " comp in progress");
                        else
                           curMsg += sprintf(curMsg, " already in the right place %d", n);
                        }
                     }
                  }
               else
                  {
                  if (logSampling)
                     curMsg += sprintf(curMsg, " cannot be compiled, extra field is %08X", TR::CompilationInfo::getJ9MethodExtra(j9method));
                  }
               }
            TR::Recompilation::globalSampleCount++;
            }
         else if (logSampling)
            {
            if (count >= 0)
               curMsg += sprintf(curMsg, " %d invocations before compiling", count);
            else
               curMsg += sprintf(curMsg, " cannot be compiled");
            }
         }
      else // sampling interpreted body, but method was compiled
         {
         // Unlikely scenario, unless the method has long running activations.
         // Create an activation length record for this method
         //
         //if(TR::Options::getCmdLineOptions()->getFixedOptLevel() == -1)
         //   compInfo->getPersistentInfo()->getActivationTable()->insert(j9method, totalSampleCount, fe);

         TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(j9method->extra);
         if (bodyInfo)
            bodyInfo->_longRunningInterpreted = true;

         if (logSampling)
            curMsg += sprintf(curMsg, " counter = XX (long running?)");
         // Note that we do not increment globalSampleCount here
         }
      if (fe->isLogSamplingSet())
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING,"%s", msg);
         }
      Trc_JIT_Sampling_Detail(getJ9VMThreadFromTR_VM(fe), msg);
      return plan;
}


TR_OptimizationPlan *
TR::DefaultCompilationStrategy::processJittedSample(TR_MethodEvent *event)
{
   TR_OptimizationPlan *plan = 0;
   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   J9Method *j9method = event->_j9method;
   J9JITConfig *jitConfig = event->_vmThread->javaVM->jitConfig;
   TR::CompilationInfo *compInfo = 0;
   if (jitConfig)
      compInfo = TR::CompilationInfo::get(jitConfig);

   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, event->_vmThread);
   int32_t totalSampleCount = ++ TR::Recompilation::globalSampleCount;
   uint64_t crtTime = compInfo->getPersistentInfo()->getElapsedTime();

#define MSG_SZ 450
   char msg[MSG_SZ];  // size should be big enough to hold the whole one-line msg
   msg[0] = 0;
   char *curMsg = msg;
   void *startPC = event->_oldStartPC;
   bool logSampling = fe->isLogSamplingSet() || TrcEnabled_Trc_JIT_Sampling_Detail;
   if (logSampling || TrcEnabled_Trc_JIT_Sampling)
      {
#define SIG_SZ 150
      char sig[SIG_SZ];  // hopefully the size is good for most cases
      fe->printTruncatedSignature(sig, SIG_SZ, (TR_OpaqueMethodBlock*)j9method);
      int32_t pcOffset = (uint8_t *)(event->_samplePC) - (uint8_t *)startPC;
      if (logSampling)
         curMsg += sprintf(curMsg, "(%d)\tCompiled %s\tPC=" POINTER_PRINTF_FORMAT "\t%+d\t", totalSampleCount, sig, startPC, pcOffset);
      if (TrcEnabled_Trc_JIT_Sampling && ((totalSampleCount % 4) == 0))
         Trc_JIT_Sampling(getJ9VMThreadFromTR_VM(fe), "Compiled", sig, 0); // TODO put good pcOffset
      }

   TR::Recompilation::jitGlobalSampleCount++;

   // Insert an yield point if compilation queue size is too big and CPU utilization is close to 100%
   // QueueSize changes all the time, so threads may experience cache misses
   // trying to access it. It's better to have a variable defined in compInfo
   // which says by how much we need to delay application threads. This variable
   // will be changed by the sampling thread, every 0.5 seconds
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableAppThreadYield))
      {
      int32_t sleepNano = compInfo->getAppSleepNano(); // determine how much I need to sleep
      if (sleepNano != 0) // If I need to sleep at all
         {
         if (sleepNano == 1000000)
            {
            j9thread_sleep(1); // param in ms
            }
         else
            {
            if (fe->shouldSleep()) // sleep every other sample point
               j9thread_sleep(1); // param in ms
            }
         }
      }
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   TR_PersistentJittedBodyInfo *bodyInfo = NULL;

      compInfo->_stats._compiledMethodSamples++;

   if (linkageInfo->hasFailedRecompilation())
      {
      compInfo->_stats._compiledMethodSamplesIgnored++;
      if (logSampling)
         curMsg += sprintf(curMsg, " has already failed a recompilation attempt");
      }
   else if (!linkageInfo->isSamplingMethodBody())
      {
      compInfo->_stats._compiledMethodSamplesIgnored++;
      if (logSampling)
         curMsg += sprintf(curMsg, " does not use sampling");
      }
   else if (debug("disableSamplingRecompilation"))
      {
      compInfo->_stats._compiledMethodSamplesIgnored++;
      if (logSampling)
         curMsg += sprintf(curMsg, " sampling disabled");
      }
   else
      bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);

   if (bodyInfo && bodyInfo->getDisableSampling())
      {
      compInfo->_stats._compiledMethodSamplesIgnored++;
      if (logSampling)
         curMsg += sprintf(curMsg, " uses sampling but sampling disabled (last comp. with prex)");
      bodyInfo = NULL;
      }

   if (bodyInfo)
      {
      bool getOut = false;
      TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
      fe->acquireCompilationLock();
      bool isAlreadyBeingCompiled;
      TR_OpaqueMethodBlock *j9m = methodInfo->getMethodInfo();
      void *currentStartPC = TR::CompilationInfo::isCompiled((J9Method*)j9m) ? (void *)TR::Compiler->mtd.startPC(j9m) : NULL;

      // See if the method has already been compiled but we get a sample in the old body
      if (currentStartPC != startPC) // rare case
         getOut = true;
      else if (TR::Options::getCmdLineOptions()->getFixedOptLevel() != -1
               || TR::Options::getAOTCmdLineOptions()->getFixedOptLevel() != -1) // prevent recompilation when opt level is specified
         {
         getOut = true;
         }
      else
         {
         isAlreadyBeingCompiled = TR::Recompilation::isAlreadyBeingCompiled(methodInfo->getMethodInfo(), startPC, fe);
         // If we already decided to recompile this body, and we haven't yet
         // queued the method don't bother continuing. Very small window of time.
         //
         if (bodyInfo->getSamplingRecomp() && // flag needs to be tested after getting compilationMonitor
            !isAlreadyBeingCompiled)
            {
            if (logSampling)
               curMsg += sprintf(curMsg, " uses sampling but a recomp decision has already been taken");
            getOut = true;
            }
         }
      if (getOut)
         {
         fe->releaseCompilationLock();
         // and do nothing
         }
      else
         {
         bool recompile = false;
         TR_Hotness nextOptLevel;
         bool useProfiling = false;

         // Profiling compilations that precede scorching ones are quite taxing
         // on large multicore machines. Thus, we want to observe the hotness of a
         // method for longer, rather than rushing into a profiling very-hot compilation.
         // We can afford to do so because scorching methods accumulate samples at a
         // higher rate than hot ones.
         // The goal here is to implement a rather short decision window (sampling interval)
         // for decisions to upgrade to hot, but a larger decision window for decisions
         // to go to scorching. This is based on density of samples observed in the JVM:
         // the larger the density of samples, the larger the scorching decision window.
         // scorchingSampleInterval will be a multiple of hotSampleInterval
         // When a hotSampleInterval ends, if the method looks scorching we postpone any
         // recompilation decision until a scorchingSampleInterval finishes. If the method
         // only looks hot, then we decide to recompile at hot at the end of the hotSampleInterval

         uint32_t intervalIncreaseFactor = compInfo->getJitSampleInfoRef().getIncreaseFactor();
         // possibly larger sample interval for scorching compilations
         int32_t scorchingSampleInterval = TR::Options::_sampleInterval * intervalIncreaseFactor;

         // Hot recompilation decisions use the regular sized sampling interval
         uint8_t hotSampleInterval  = TR::Options::_sampleInterval;
         int32_t hotSampleThreshold = TR::Options::_sampleThreshold;

         int32_t count = bodyInfo->decCounter();
         uint8_t crtSampleIntervalCount = bodyInfo->incSampleIntervalCount(scorchingSampleInterval);
         bool hotSamplingWindowComplete = (crtSampleIntervalCount % hotSampleInterval) == 0;
         bool scorchingSamplingWindowComplete = (crtSampleIntervalCount == 0);

         int32_t startSampleCount = bodyInfo->getStartCount();
         int32_t globalSamples = totalSampleCount - startSampleCount;
         int32_t globalSamplesInHotWindow = globalSamples - bodyInfo->getHotStartCountDelta();

         int32_t scaledScorchingThreshold = 0, scaledHotThreshold = 0;

         if (logSampling)
            curMsg += sprintf(curMsg, " cnt=%d ncl=%d glblSmplCnt=%d startCnt=%d[-%u,+%u] samples=[%d %d] windows=[%d %u] crtSmplIntrvlCnt=%u",
               count, methodInfo->getNextCompileLevel(), totalSampleCount, startSampleCount,
               bodyInfo->getOldStartCountDelta(), bodyInfo->getHotStartCountDelta(),
               globalSamples, globalSamplesInHotWindow,
               scorchingSampleInterval, hotSampleInterval, crtSampleIntervalCount);

         bool dontSwitchToProfiling = false;
         if (count <= 0)
            {
            if (!isAlreadyBeingCompiled)
               {
               // do not allow scorching compiles based on count reaching 0
               if (methodInfo->getNextCompileLevel() > hot)
                  {
                  // replenish the counter with a multiple of sampleInterval
                  bodyInfo->setCounter(hotSampleInterval);
                  // even if the count reached 0, we still need to check if we can
                  // promote this method through sample thresholds
                  }
               else // allow transition to HOT through exhaustion of count
                  {
                  recompile = true;
                  TR::Recompilation::limitMethodsCompiled++;
                  // Currently the counter can be decremented because (1) the method was
                  // sampled; (2) EDO; (3) PIC miss; (4) megamorphic interface call profile.
                  // EDO will have its own recompilation snippet, but in cases (3) and (4)
                  // the counter just reaches 0, and only the next sample will trigger
                  // recompilation. These cases can be identified by the the negative counter
                  // (we decrement the counter above in sampleMethod()). In contrast, if the
                  // counter is decremented through sampling, only the first thread that sees
                  // the counter 0 will recompile the method, and all the others will be
                  // prevented from reaching this point due to isAlreadyBeingCompiled

                  if (count < 0 && !methodInfo->disableMiscSamplingCounterDecrementation())
                     {
                     // recompile at same level
                     nextOptLevel = bodyInfo->getHotness();

                     // mark this special situation
                     methodInfo->setDisableMiscSamplingCounterDecrementation();
                     // write a message in the vlog to know the reason of recompilation
                     if (logSampling)
                        curMsg += sprintf(curMsg, " PICrecomp");
                     methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToMegamorphicCallProfile);
                     }
                  else
                     {
                     nextOptLevel = methodInfo->getNextCompileLevel();
                     methodInfo->setReasonForRecompilation(bodyInfo->getIsPushedForRecompilation() ?
                                  TR_PersistentMethodInfo::RecompDueToRecompilationPushing : TR_PersistentMethodInfo::RecompDueToCounterZero);
                     // It's possible that a thread decrements the counter to 0 and another
                     // thread decrements it further to -1 which will trigger a compilation
                     // at same level. The following line will prevent that.
                     methodInfo->setDisableMiscSamplingCounterDecrementation();
                     }
                  }
               }

            if (recompile) // recompilation due to count reaching 0
               {
               bodyInfo->setOldStartCountDelta(totalSampleCount - startSampleCount);// Should we handle overflow?
               bodyInfo->setHotStartCountDelta(0);
               bodyInfo->setStartCount(totalSampleCount);
               }
            }

         bool postponeDecision = false;
         if (!recompile && hotSamplingWindowComplete && totalSampleCount > startSampleCount)
            {
            compInfo->_stats._methodsReachingSampleInterval++;

            // Goal: based on codeSize, scale the original Threshold by no more than +/-x%
            // 'x' will be called sampleThresholdVariationAllowance
            // When codeSize==avgCodeSize, we want the scaling factor to be 1.0
            // The scaling of the threshold can be turned off by having
            // the sampleThresholdVariationAllowance equal to 0
            J9JITExceptionTable *metaData = jitConfig->jitGetExceptionTableFromPC(event->_vmThread, (UDATA)startPC);
            int32_t codeSize = 0; // TODO eliminate the overhead; we already have metadata
            if (metaData)
               codeSize = compInfo->calculateCodeSize(metaData);

            // Scale the recompilation thresholds based on method size
            int32_t avgCodeSize = (TR::Compiler->target.cpu.isI386() || TR::Compiler->target.cpu.isPower()) ? 1500 : 3000; // experimentally determined

            TR_ASSERT(codeSize != 0, "startPC!=0 ==> codeSize!=0");

            float scalingFactor = 0.01*((100 - TR::Options::_sampleThresholdVariationAllowance) +
                                 (avgCodeSize << 1)*TR::Options::_sampleThresholdVariationAllowance /
                                 (float)(avgCodeSize + codeSize));
            curMsg += sprintf(curMsg, " SizeScaling=%.1f", scalingFactor);
            scaledHotThreshold = (int32_t)(hotSampleThreshold*scalingFactor);

            // Do not use aggressive recompilations for big applications like websphere.
            // WebSphere loads more than 14000 classes, typical small apps more like 1000-2000 classes.
            // ==> use a reasonable value like 5000 to determine if the application is big
            bool useAggressiveRecompilations = !cmdLineOptions->getOption(TR_DisableAggressiveRecompilations) &&
                                               (bodyInfo->decAggressiveRecompilationChances() > 0 ||
                                               compInfo->getPersistentInfo()->getNumLoadedClasses() < TR::Options::_bigAppThreshold);

            bool conservativeCase = TR::Options::getCmdLineOptions()->getOption(TR_ConservativeCompilation) &&
                                    compInfo->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold;

            if (conservativeCase)
               {
               scaledHotThreshold >>= 1; // halve the threshold for a more conservative comp decision
               useAggressiveRecompilations = true; // force it, to allow recomp at original threshold,
                                                   // but double the sample interval (60 samples)
               }
            // For low number of processors become more conservative during startup
            if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
                TR::Compiler->target.numberOfProcessors() <= 2)
               scaledHotThreshold >>= 2;

            // Used to make recompilations less aggressive during WebSphere startup,
            // avoiding costly hot, and very hot compilation
            bool isBigAppStartup = (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP
                                    && TR::Options::sharedClassCache()
                                    && compInfo->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold
                                    && TR::Options::_bigAppSampleThresholdAdjust > 0);
            if (isBigAppStartup)
               {
               scaledHotThreshold >>= TR::Options::_bigAppSampleThresholdAdjust; //adjust to avoid hot recomps
               useAggressiveRecompilations = false; //also to avoid potential hot recomps, this could have been set
               }

            // We allow hot compilations at a lower CPU, but for a longer period of time (scorching window)
            bool secondCriteriaHot = false;
            // Check for non first hot interval
            if (useAggressiveRecompilations)
               {
               int32_t samplesInSelf = scorchingSamplingWindowComplete ? scorchingSampleInterval : crtSampleIntervalCount;
               // Alternative: Here we may want to do something only if a scorchingSampleWindow is complete
               if (samplesInSelf > hotSampleInterval)
                  {
                  // 0.5*targetCPU < crtCPU
                  if (((globalSamples*hotSampleInterval) >> 1) < (scaledHotThreshold * samplesInSelf))
                     secondCriteriaHot = true;
                  }
               }

            // TODO: if the scorching window is complete, should we look at CPU over the larger window?
            if (globalSamplesInHotWindow <= scaledHotThreshold || secondCriteriaHot)
               {
               // The method is hot, but is it actually scorching?
               //
               // If the scorching interval is done, perform normal scorching test
               // If the scorching interval is not done, performs a sniff test for a shorter interval
               //    1. If the method looks scorching during this small interval, do not
               //       do anything; just wait for the scorching interval to finish
               //    2. If the method does not look scorching, perform a hot compilation
               //
               // First let's do some scaling based on size, startup, bigApp, numProc, etc
               scaledScorchingThreshold = (int32_t)(TR::Options::_scorchingSampleThreshold * scalingFactor);
               if (conservativeCase)
                  {
                  scaledScorchingThreshold >>= 1; // halve the threshold for a more conservative comp decision
                  if (TR::Compiler->target.numberOfProcessors() != 1)
                     useAggressiveRecompilations = true; // to allow recomp at original threshold,
                  else                                   // but double the sample interval (60 samples)
                     useAggressiveRecompilations = false;
                  }

               if (isBigAppStartup)
                  {
                  scaledScorchingThreshold >>= TR::Options::_bigAppSampleThresholdAdjust; //adjust to avoid scorching recomps
                  useAggressiveRecompilations = false; //this could have been set, so disable to avoid
                  }

               if (!scorchingSamplingWindowComplete)
                  {
                  // Perform scorching recompilation sniff test using a shorter sample interval
                  // TODO: relax the thresholds a bit, maybe we can go directly to scorching next time
                  if (globalSamplesInHotWindow <= scaledScorchingThreshold)
                     postponeDecision = true;
                  }
               else // scorching sample interval is done
                  {
                  // Adjust the scorchingSampleThreshold because the sample window is larger
                  scaledScorchingThreshold = scaledScorchingThreshold * intervalIncreaseFactor;

                  // Make the scorching compilation less likely as time goes by
                  // The bigger the number of scorching intervals, the smaller scaledScorchingThreshold
                  if (bodyInfo->getNumScorchingIntervals() > 3)
                     scaledScorchingThreshold >>= 1;

                  // secondCriteria looks at hotness over a period of time that is double
                  // than normal (60 samples). This is why we have to increase scaledScorchingThreshold
                  // by a factor of 2. If we want to become twice as aggressive we need to double
                  // scaledScorchingThreshold yet again
                  //
                  bool secondCriteriaScorching = useAggressiveRecompilations &&
                     (totalSampleCount - bodyInfo->getOldStartCount() <= (scaledScorchingThreshold << 2));
                  // Scorching test
                  if ((globalSamples <= scaledScorchingThreshold) || secondCriteriaScorching)
                     {
                     // Determine whether or not the method is to be profiled before
                     // being compiled as scorching hot.
                     // For profiling the platform must support counting recompilation.
                     //
                     if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling) &&
                        TR::Recompilation::countingSupported() && !TR::CodeCacheManager::instance()->almostOutOfCodeCache() &&
                        !(methodInfo->profilingDisabled()))
                        {
                        nextOptLevel = veryHot;
                        useProfiling = true;
                        }
                     else
                        {
                        nextOptLevel = scorching;
                        }
                     recompile = true;
                     compInfo->_stats._methodsSelectedToRecompile++;
                     TR::Recompilation::scorchingThresholdMethodsCompiled++;
                     }
                  }
               // Should we proceed with the hot compilation?
               if (!recompile && !postponeDecision && bodyInfo->getHotness() <= warm)
                  {
                  nextOptLevel = hot;
                  // Decide whether to deny optimizer to switch to profiling on the fly
                  if (globalSamplesInHotWindow > TR::Options::_sampleDontSwitchToProfilingThreshold &&
                     !TR::Options::getCmdLineOptions()->getOption(TR_AggressiveOpts))
                     dontSwitchToProfiling = true;
                  recompile = true;
                  compInfo->_stats._methodsSelectedToRecompile++;
                  TR::Recompilation::hotThresholdMethodsCompiled++;
                  }
               }
            // If the method is truly cold, replenish the counter to avoid
            // recompilation through counter decrementation
            else if (globalSamplesInHotWindow >= TR::Options::_resetCountThreshold)
               {
               compInfo->_stats._methodsSampleWindowReset++;
               bodyInfo->setCounter(count + hotSampleInterval);
               if (logSampling)
                  curMsg += sprintf(curMsg, " is cold, reset cnt to %d", bodyInfo->getCounter());
               }
            // The hot sample interval is done. Prepare for next interval.
            if (scorchingSamplingWindowComplete)
               {
               // scorching sample interval is done
               bodyInfo->setStartCount(totalSampleCount);
               bodyInfo->setOldStartCountDelta(totalSampleCount - startSampleCount);
               bodyInfo->setHotStartCountDelta(0);
               }
            else
               {
               int32_t hotStartCountDelta = totalSampleCount - startSampleCount;
               TR_ASSERT(hotStartCountDelta >= 0, "hotStartCountDelta should not be negative\n");
               if (hotStartCountDelta > 0xffff)
                  hotStartCountDelta = 0xffff;
               bodyInfo->setHotStartCountDelta(hotStartCountDelta);
               }

            if (recompile)
               {
               // One more test
               if (!isAlreadyBeingCompiled)
                  {
                  methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToThreshold);
                  }
               else // the method is already being compiled; maybe we need to update the opt level
                  {
                  recompile = false; // do not need to recompile the method
                  if ((int32_t)nextOptLevel > (int32_t)methodInfo->getNextCompileLevel())
                     {
                     // search the queue to update the optimization plan.
                     //
                     TR::IlGeneratorMethodDetails details(j9method);
                     TR_MethodToBeCompiled *entry =
                        compInfo->adjustCompilationEntryAndRequeue(details, methodInfo, nextOptLevel,
                                                                   useProfiling,
                                                                   CP_ASYNC_NORMAL, fe);
                     if (entry)
                        {
                        if (logSampling)
                           curMsg += sprintf(curMsg, " adj opt lvl to %d", (int32_t)(entry->_optimizationPlan->getOptLevel()));
                        int32_t measuredCpuUtil = crtSampleIntervalCount == 0 ? // scorching interval done?
                                                   scorchingSampleInterval * 1000 / globalSamples :
                                                   hotSampleInterval * 1000 / globalSamplesInHotWindow;
                        entry->_optimizationPlan->setPerceivedCPUUtil(measuredCpuUtil);
                        }
                     }
                  }
               }
            }

         // try to upgrade some of the less optimized compilations
         bool willUpgrade = false;
         if (!recompile)
            {
            if (bodyInfo->getFastRecompilation() && !isAlreadyBeingCompiled)
               {
               // Allow profiling even if we are about to exhaust the code cache
               // because this case is used for diagnostic only
               if (bodyInfo->getFastScorchingRecompilation())
                  {
                  if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling) &&
                      TR::Recompilation::countingSupported() &&
                      !(methodInfo->profilingDisabled()))
                     {
                     nextOptLevel = veryHot;
                     useProfiling = true;
                     }
                  else
                     {
                     nextOptLevel = scorching;
                     }
                  }
               else
                  {
                  nextOptLevel = hot;
                  }
               recompile = true;
               methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToThreshold);//lie
               }
            else if (!postponeDecision &&
               !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpgrades) &&
               // case (1) methods downgraded to cold
               ((bodyInfo->getHotness() < warm &&
                (methodInfo->isOptLevelDowngraded() || cmdLineOptions->getOption(TR_EnableUpgradingAllColdCompilations))) ||
               // case (2) methods taken from shared cache
               bodyInfo->getIsAotedBody()))
                // case (3) cold compilations for bootstrap methods, even if not downgraded
               {
               // test other conditions for upgrading

               uint32_t threshold = TR::Options::_coldUpgradeSampleThreshold;
               // Pick a threshold based on method size (higher thresholds for bigger methods)
               if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
                   compInfo->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold)
                  {
                  threshold += (uint32_t)(TR::CompilationInfo::getMethodBytecodeSize(j9method) >> 8);
                  // sampleIntervalCount goes from 0 to _sampleInterval-1
                  // Very big methods (bigger than 6K bytecodes) will have a threshold bigger than this
                  // and never be upgraded, which is what we want
                  }
               if ((uint32_t)crtSampleIntervalCount >= threshold &&
                   compInfo->getMethodQueueSize() <= TR::CompilationInfo::SMALL_QUEUE &&
                   !compInfo->getPersistentInfo()->isClassLoadingPhase() &&
                   !isAlreadyBeingCompiled &&
                   !cmdLineOptions->getOption(TR_DisableUpgradingColdCompilations))
                  {
                  recompile = true;
                  if (!bodyInfo->getIsAotedBody())
                     {
                     // cold-nonaot compilations can only be upgraded to warm
                     nextOptLevel = warm;
                     }
                  else // AOT bodies
                     {
                     if (!TR::Options::isQuickstartDetected())
                        {
                        // AOT upgrades are performed at warm
                        // We may want to look at how expensive the method is though
                        nextOptLevel = warm;
                        }
                     else // -Xquickstart (and AOT)
                        {
                        nextOptLevel = cold;
                        // Exception: bootstrap class methods that are cheap should be upgraded directly at warm
                        if (cmdLineOptions->getOption(TR_UpgradeBootstrapAtWarm) && fe->isClassLibraryMethod((TR_OpaqueMethodBlock *)j9method))
                           {
#ifndef PUBLIC_BUILD
#if defined(HINTS_IN_SHAREDCACHE_OBJECT)
                           bool expensiveComp = (TR_J9SharedCache *)(((TR_J9VMBase *) fe)->sharedCache())->isHint(j9method, TR_HintLargeMemoryMethodW);
#else
                           bool expensiveComp = ((TR_J9VMBase *) fe)->isSharedCacheHint(j9method, TR_HintLargeMemoryMethodW);
#endif
                           if (!expensiveComp)
#endif //!PUBLIC_BUILD
                              nextOptLevel = warm;
                           }
                        }
                     }
                  methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToOptLevelUpgrade);
                  // reset the flag to avoid upgrading repeatedly
                  methodInfo->setOptLevelDowngraded(false);
                  willUpgrade = true;
                  }
               }
            }

         // if we don't take any recompilation decision, let's see if we can
         // schedule a compilation from the low priority queue
         if (!recompile && compInfo && compInfo->getLowPriorityCompQueue().hasLowPriorityRequest() &&
             compInfo->canProcessLowPriorityRequest())
            {
            // wake up the compilation thread
            compInfo->getCompilationMonitor()->notifyAll();
            }
         if (recompile)
            {
            // Method is being recompiled because it is truly hot;
            bodyInfo->setSamplingRecomp();
            }
         fe->releaseCompilationLock();
         if (recompile)
            {
            //induceRecompilation(fe, startPC);
            bool useSampling = (nextOptLevel != scorching && !useProfiling);
            plan = TR_OptimizationPlan::alloc(nextOptLevel, useProfiling, useSampling);
            if (plan)
               {
               int32_t measuredCpuUtil = crtSampleIntervalCount == 0 ? // scorching interval done?
                  (globalSamples != 0 ? scorchingSampleInterval * 1000 / globalSamples : 0) :
                  (globalSamplesInHotWindow != 0 ? hotSampleInterval * 1000 / globalSamplesInHotWindow : 0);
               plan->setPerceivedCPUUtil(measuredCpuUtil);
               plan->setIsUpgradeRecompilation(willUpgrade);
               plan->setDoNotSwitchToProfiling(dontSwitchToProfiling);
               if (crtSampleIntervalCount == 0 && // scorching compilation decision can be taken
                   globalSamples <= TR::Options::_relaxedCompilationLimitsSampleThreshold) // FIXME: needs scaling
                  plan->setRelaxedCompilationLimits(true);
               if (logSampling)
                  {
                  float cpu = measuredCpuUtil / 10.0;
                  if (useProfiling)
                     curMsg += sprintf(curMsg, " --> recompile at level %d, profiled CPU=%.1f%%", nextOptLevel, cpu);
                  else
                     curMsg += sprintf(curMsg, " --> recompile at level %d CPU=%.1f%%", nextOptLevel, cpu);

                  if (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToThreshold)
                     {
                     curMsg += sprintf(curMsg, " scaledThresholds=[%d %d]", scaledScorchingThreshold, scaledHotThreshold);
                     }
                  }
               }
            else // OOM
               {
               if (logSampling)
                  curMsg += sprintf(curMsg, " --> not recompiled: OOM");
               }
            }
         else if (logSampling)
            {
            if (isAlreadyBeingCompiled)
               curMsg += sprintf(curMsg, " - is already being recompiled");
            else if (!hotSamplingWindowComplete)
               curMsg += sprintf(curMsg, " not recompiled, smpl interval not done");
            else
               {
               float measuredCpuUtil = 0.0;
               if (crtSampleIntervalCount == 0) // scorching interval done
                  {
                  if (globalSamples)
                     measuredCpuUtil = scorchingSampleInterval * 100.0 / globalSamples;
                  }
               else
                  {
                  if (globalSamplesInHotWindow)
                     measuredCpuUtil = hotSampleInterval * 100.0 / globalSamplesInHotWindow;
                  }
               curMsg += sprintf(curMsg, " not recompiled, CPU=%.1f%% %s scaledThresholds=[%d %d]",
                  measuredCpuUtil, postponeDecision ? " postpone decision" : "",
                  scaledScorchingThreshold, scaledHotThreshold);
               }
            }
         }
      } // endif (bodyInfo)

   if (logSampling)
      {
      bool bufferOverflow = ((curMsg - msg) >= MSG_SZ); // check for overflow at runtime
      if (fe->isLogSamplingSet())
         {
         TR_VerboseLog::vlogAcquire();
         TR_VerboseLog::writeLine(TR_Vlog_SAMPLING,"%s", msg);
         if (bufferOverflow)
            TR_VerboseLog::writeLine(TR_Vlog_SAMPLING,"Sampling line is too big: %d characters", curMsg-msg);
         TR_VerboseLog::vlogRelease();
         }
      Trc_JIT_Sampling_Detail(getJ9VMThreadFromTR_VM(fe), msg);
      if (bufferOverflow)
         Trc_JIT_Sampling_Detail(getJ9VMThreadFromTR_VM(fe), "Sampling line will cause buffer overflow");
      // check for buffer overflow and write a message
      }
   return plan;
}

TR_OptimizationPlan *
TR::DefaultCompilationStrategy::processHWPSample(TR_MethodEvent *event)
   {
   TR_OptimizationPlan *plan = NULL;
   TR_Hotness hotnessLevel;
   TR_PersistentJittedBodyInfo *bodyInfo;
   TR_PersistentMethodInfo *methodInfo;

   bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(event->_oldStartPC);

   TR_ASSERT(bodyInfo, "bodyInfo should not be NULL!\n");
   if (!bodyInfo)
      return NULL;

   methodInfo = bodyInfo->getMethodInfo();
   hotnessLevel = bodyInfo->getHotness();
   if (bodyInfo->getIsProfilingBody() && !bodyInfo->getUsesJProfiling())
      {
      // We rely on a count-based recompilation for profiled methods.
      return NULL;
      }

   TR_Hotness nextOptLevel = event->_nextOptLevel;
   if (nextOptLevel > hotnessLevel ||
       (bodyInfo->getIsAotedBody() && !TR::Options::getCmdLineOptions()->getOption(TR_DontRIUpgradeAOTWarmMethods)))
      {
      J9JITConfig *jitConfig = event->_vmThread->javaVM->jitConfig;
      TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, event->_vmThread);
      fe->acquireCompilationLock();
      bool isAlreadyBeingCompiled = TR::Recompilation::isAlreadyBeingCompiled((TR_OpaqueMethodBlock *) event->_j9method, event->_oldStartPC, fe);
      fe->releaseCompilationLock();
      if (!isAlreadyBeingCompiled)
         {
         if (nextOptLevel == scorching &&
            !TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling) &&
            TR::Recompilation::countingSupported() &&
            !bodyInfo->_methodInfo->profilingDisabled())
            {
            plan = TR_OptimizationPlan::alloc(veryHot, true, false);
            }
         else
            {
            plan = TR_OptimizationPlan::alloc(nextOptLevel, false, true);
            }

         if (plan)
            methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToRI);
         }
      }
   return plan;
   }

//-------------------------- adjustOptimizationPlan ---------------------------
// Input: structure with information about the method to be compiled
// Output: returns true if the optimization plan has been changed. In that case
//         the optimization level will be changed and also 2 flags in the
//         optimization plan may be changed (OptLevelDowngraded, AddToUpgradeQueue)
//----------------------------------------------------------------------------
bool TR::DefaultCompilationStrategy::adjustOptimizationPlan(TR_MethodToBeCompiled *entry, int32_t optLevelAdjustment)
   {
   // Run SmoothCompilation to see if we need to change the opt level and/or priority
   bool shouldAddToUpgradeQueue = false;
   TR::CompilationInfo *compInfo = TR::CompilationController::getCompilationInfo();
   if (optLevelAdjustment == 0) // unchanged opt level (default)
      {
      shouldAddToUpgradeQueue = compInfo->SmoothCompilation(entry, &optLevelAdjustment);
      }

   // Recompilations are treated differently
   if (entry->_oldStartPC != 0)
      {
      // Downgrade the optimization level of invalidation requests
      // if too many invalidations are present into the compilation queue
      // Here we access _numInvRequestsInCompQueue outside the protection of compilation queue monitor
      // This is fine because it's just a heuristic
      if (entry->_entryIsCountedAsInvRequest &&
         compInfo->getNumInvRequestsInCompQueue() >= TR::Options::_numQueuedInvReqToDowngradeOptLevel &&
         entry->_optimizationPlan->getOptLevel() > cold &&
         !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeToCold))
         {
         entry->_optimizationPlan->setOptLevel(cold);
         // Keep the optLevel in sync between the optPlan and methodInfo
         TR_PersistentMethodInfo* methodInfo = TR::Recompilation::getMethodInfoFromPC(entry->_oldStartPC);
         TR_ASSERT(methodInfo, "methodInfo must exist because we recompile");
         methodInfo->setNextCompileLevel(entry->_optimizationPlan->getOptLevel(), entry->_optimizationPlan->insertInstrumentation());
         // DO NOT mark this as optLevelDowngraded
         // entry->_optimizationPlan->setOptLevelDowngraded(true);
         return true;
         }
      return false;
      }

   if (optLevelAdjustment == 0)
      return false;

   // Must check if we really downgrade this method (for fixed opt level we do not do it)
   TR_Hotness hotnessLevel = entry->_optimizationPlan->getOptLevel();
   bool optLevelDowngraded = false;

   if (true)
      {
      if (TR::Options::getCmdLineOptions()->allowRecompilation()) // don't do it for fixed level
         {
         if (optLevelAdjustment > 0) // would like to increase the opt level
            {
            if (hotnessLevel == warm || hotnessLevel == cold || hotnessLevel == noOpt)
               hotnessLevel = (TR_Hotness)((int)hotnessLevel + 1);
            }
         else // would like to decrease the opt level
            {
            if (optLevelAdjustment < -1)
               {
               hotnessLevel = noOpt;
               optLevelDowngraded = true;
               }
            else if (hotnessLevel == warm || hotnessLevel == hot)
               {
               hotnessLevel = (TR_Hotness)((int)hotnessLevel - 1);
               optLevelDowngraded = true;
               }
            }
         }
      }

   // If change in hotness level
   if (entry->_optimizationPlan->getOptLevel() != hotnessLevel)
      {
      entry->_optimizationPlan->setOptLevel(hotnessLevel);
      entry->_optimizationPlan->setOptLevelDowngraded(optLevelDowngraded);
      // Set the flag to add to the upgrade queue
      if (optLevelDowngraded  && shouldAddToUpgradeQueue)
         entry->_optimizationPlan->setAddToUpgradeQueue();
      return true;
      }
   else
      {
      return false;
      }
   }


void TR::DefaultCompilationStrategy::beforeCodeGen(TR_OptimizationPlan *plan, TR::Recompilation *recomp)
   {
   // Set up the opt level and counter for the next compilation. This will
   // also decide if there is going to be a next compilation. If there is no
   // next compilation, remove any counters that have been inserted into the code
   // Ideally, we should have a single step after the compilation
   if (! recomp->_doNotCompileAgain)
      {
      int32_t level;
      int32_t countValue;

      // do not test plan->insertInstrumentation() because we might have switched to profiling
      TR_Hotness current = recomp->_compilation->getMethodHotness();
      if (recomp->isProfilingCompilation() && current < scorching)
         {
         // Set the level for the next compilation.  This will be higher than
         // the level at which we are compiling the current method for profiling.
         //
         level = current+1;
         countValue = PROFILING_INVOCATION_COUNT - 1; // defined in Profiler.hpp
         }
      else
         {
         // figure out the next opt level and the next count
         TR::Compilation *comp = recomp->_compilation;
         bool mayHaveLoops = comp->mayHaveLoops();
         TR::Options *options = comp->getOptions();
         if (recomp->_bodyInfo->getUsesGCR())
            {
            level = warm;  // GCR recompilations should be performed at warm
            // If a GCR count was specified, used that
            if (options->getGCRCount() > 0)
               {
               countValue = options->getGCRCount();
               }
            else // find the count corresponding to the warm level (or next available)
               {
               countValue = options->getCountValue(mayHaveLoops, (TR_Hotness) level);
               if (countValue < 0)
                  {
                  // Last resort: use some sensible values
                  countValue = mayHaveLoops ? options->getInitialBCount() : options->getInitialCount();
                  }
               }
            }
         else
            {
            level = options->getNextHotnessLevel(mayHaveLoops, plan->getOptLevel());
            countValue = options->getCountValue(mayHaveLoops, (TR_Hotness) level);
            }
         }

      if ((countValue > 0) || (recomp->isProfilingCompilation() && current < scorching) || plan->isOptLevelDowngraded() || recomp->_bodyInfo->getUsesGCR())
         {
         recomp->_nextLevel = (TR_Hotness)level; // There may be another compilation
         }
      else
         {
         // There will not be another compilation - remove any counters that
         // have been inserted into the code.
         //
         recomp->preventRecompilation();
         //recomp->_useSampling = false; // wrong, because the codegen will generate a counting body
         recomp->_bodyInfo->setDisableSampling(true);
         // also turn off sampling for this body
         }
      recomp->_nextCounter  = countValue;
      }
   }

void TR::DefaultCompilationStrategy::postCompilation(TR_OptimizationPlan *plan, TR::Recompilation *recomp)
   {
   if (!TR::CompilationController::getCompilationInfo()->asynchronousCompilation())
      {
      TR_OptimizationPlan::_optimizationPlanMonitor->enter();
      recomp->getMethodInfo()->_optimizationPlan = NULL;
      TR_OptimizationPlan::_optimizationPlanMonitor->exit();
      }
   }





//============================= ThresholdCompilationStrategy ====================


TR::ThresholdCompilationStrategy::ThresholdCompilationStrategy()
   {
        // To be safe, clear everything out before setting anything
   for (int32_t level=noOpt; level <= numHotnessLevels; level++)
      {
      _nextLevel[level] = unknownHotness;
      _samplesNeededToMoveTo[level] = -1;
      _performInstrumentation[level] = false;
      }

   // Now, initialize our strategy threshold based strategy
   //
   // These could easily be set from command line options or any other
   // way (maybe from the existing options string?)
   //
   // The current strategy uses only noOpt -> warm -> scorching.   (and veryHot if instrumentation-based profiling is used)
   _samplesNeededToMoveTo[noOpt] = 1;
   _samplesNeededToMoveTo[warm] = 6;
   int32_t SCORCHING_THRESH = 20;
   _samplesNeededToMoveTo[scorching] = SCORCHING_THRESH;

   // If we are doing instrumentation-based profiling
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling))
      {
      // Insert instrumentation in VeryHot
      _samplesNeededToMoveTo[veryHot] = SCORCHING_THRESH;
      _performInstrumentation[veryHot] = 1;  // Yes, perform profiling at this level

      _samplesNeededToMoveTo[scorching] = SCORCHING_THRESH + 1;  // Sampling is disable during instrumentation-based profiling,
                                                                // so this is really just a place holder
      }

   // Use the information above to setup the "next" pointers.
   // Go through list backwards, and for each "active" level, set where you'll jump to next.
   int32_t prevActiveLevel = unknownHotness;
   for (int32_t curLevel = numHotnessLevels;
        curLevel >= noOpt;  // should be "> minHotness" if it existed
        curLevel--)
      {
      if (_samplesNeededToMoveTo[curLevel] > 0)
         {
         // curLevel is an active level
         _nextLevel[curLevel] = (TR_Hotness) prevActiveLevel;
         prevActiveLevel = curLevel;
         }
      }
   // Finally, check unknownHotness (which represents the method still being interpreted) last
   _nextLevel[unknownHotness] = (TR_Hotness) prevActiveLevel;

   }



TR_Hotness TR::ThresholdCompilationStrategy::getInitialOptLevel()
   {
   return noOpt;
   }


TR_OptimizationPlan *TR::ThresholdCompilationStrategy::processEvent(TR_MethodEvent *event, bool *newPlanCreated)
{
   TR_OptimizationPlan *plan = NULL;
   TR_Hotness hotnessLevel;
   TR_PersistentJittedBodyInfo *bodyInfo;
   TR_PersistentMethodInfo *methodInfo;
   *newPlanCreated = false;

   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL3)
      fprintf(stderr, "Received event %d\n", event->_eventType);

   // first decode the event type
   switch (event->_eventType)
      {
      case TR_MethodEvent::InterpretedMethodSample:
         // do nothing
         break;
      case TR_MethodEvent::InterpreterCounterTripped:
         TR_ASSERT(event->_oldStartPC == 0, "oldStartPC should be 0 for an interpreted method");
         // use the counts to determine the first level of compilation
         // the level of compilation can be changed later on if option subsets are present
         hotnessLevel = TR::ThresholdCompilationStrategy::getInitialOptLevel();
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::OtherRecompilationTrigger: // sync recompilation through fixMethodCode
         // For sync re-compilation we attach the plan to the persistentBodyInfo
         bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(event->_oldStartPC);
         methodInfo = bodyInfo->getMethodInfo();

         if (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToInlinedMethodRedefinition)
            {
            methodInfo->incrementNumberOfInlinedMethodRedefinition();
            hotnessLevel = bodyInfo->getHotness();
            plan = TR_OptimizationPlan::alloc(hotnessLevel);
            *newPlanCreated = true;
            }
         else if (methodInfo->getOptimizationPlan())
            {
            TR_ASSERT(!TR::CompilationController::getCompilationInfo()->asynchronousCompilation(), "This case should happen only for sync recompilation");
            plan = methodInfo->getOptimizationPlan();
            }
         else
            {
            //hotnessLevel = TR::Recompilation::getNextCompileLevel(event->_oldStartPC);
            hotnessLevel = getNextOptLevel(bodyInfo->getHotness());
            plan = TR_OptimizationPlan::alloc(hotnessLevel);
            *newPlanCreated = true;
            }
         break;
      case TR_MethodEvent::NewInstanceImpl:
         hotnessLevel = getInitialOptLevel();
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         break;
      case TR_MethodEvent::MethodBodyInvalidated:
         // keep the same optimization level
         bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(event->_oldStartPC);
         TR_ASSERT(bodyInfo, "A recompilable method should have jittedBodyInfo");
         hotnessLevel = bodyInfo->getHotness();
         plan = TR_OptimizationPlan::alloc(hotnessLevel);
         *newPlanCreated = true;
         // the following is just for compatibility with older implementation
         bodyInfo->getMethodInfo()->setNextCompileLevel(hotnessLevel, false); // no profiling
         break;
      case TR_MethodEvent::JittedMethodSample:
         plan = processJittedSample(event);
         *newPlanCreated = true;
         break;

      default:
         TR_ASSERT(0, "Bad event type %d", event->_eventType);
      }

   if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL2)
      fprintf(stderr, "Event %d created plan %p\n", event->_eventType, plan);

   return plan;
}


TR_OptimizationPlan *
TR::ThresholdCompilationStrategy::processJittedSample(TR_MethodEvent *event)
   {
   TR_OptimizationPlan *plan = NULL;
   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   J9Method *j9method = event->_j9method;
   J9JITConfig *jitConfig = event->_vmThread->javaVM->jitConfig;
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, event->_vmThread);
   void *startPC = event->_oldStartPC;
   // here we may need to write into the vlog


   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   TR_PersistentJittedBodyInfo *bodyInfo = NULL;

   if (linkageInfo->hasFailedRecompilation())
      {
         //if (logSampling)
         //   msgLen += sprintf(msg + msgLen, " has already failed a recompilation attempt");
      }
   else if (!linkageInfo->isSamplingMethodBody())
      {
         //if (logSampling)
         // msgLen += sprintf(msg + msgLen, " does not use sampling");
      }
   else if (debug("disableSamplingRecompilation"))
      {
         //if (logSampling)
         //msgLen += sprintf(msg + msgLen, " sampling disabled");
      }
   else
      bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);

   if (bodyInfo && bodyInfo->getDisableSampling())
      {
         //if (logSampling)
         //msgLen += sprintf(msg + msgLen, " uses sampling but sampling disabled (last comp. with prex)");
         bodyInfo = NULL;
      }

   if (bodyInfo)
      {
      TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
      fe->acquireCompilationLock();
      void *currentStartPC = (void *)TR::Compiler->mtd.startPC((TR_OpaqueMethodBlock *) methodInfo->getMethodInfo());
      if (currentStartPC != startPC) // rare case; sampling an old body
         {
         fe->releaseCompilationLock();
         // do nothing
         }
      else if (TR::Options::getCmdLineOptions()->getFixedOptLevel() != -1
               || TR::Options::getAOTCmdLineOptions()->getFixedOptLevel() != -1) // prevent recompilation when opt level is specified
         {
         fe->releaseCompilationLock();
         // do nothing
         }
      else
         {
         // increment the CPOcount and see if we need to recompile
         int32_t sampleCount = methodInfo->cpoIncCounter();
         fe->releaseCompilationLock();
         TR_Hotness curOptLevel = bodyInfo->getHotness();
         TR_Hotness nextOptLevel = getNextOptLevel(curOptLevel);

         if ((nextOptLevel != unknownHotness) && (sampleCount == getSamplesNeeded(nextOptLevel)))
            {
            bool useSampling = (getNextOptLevel(nextOptLevel) != unknownHotness);
            plan = TR_OptimizationPlan::alloc(nextOptLevel,
                                           getPerformInstrumentation(nextOptLevel), useSampling);
            }
         }
      }
      return plan;
   }

