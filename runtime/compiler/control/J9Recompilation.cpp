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

#include "AtomicSupport.hpp"
#include "codegen/CodeGenerator.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/VMJ9.h"
#include "runtime/J9Profiler.hpp"
#include "exceptions/RuntimeFailure.hpp"

bool J9::Recompilation::_countingSupported = false;



J9::Recompilation::Recompilation(TR::Compilation *comp) :
      OMR::RecompilationConnector(comp),
      _firstCompile(comp->getCurrentMethod()->isInterpreted()),
      _useSampling(TR::Options::getSamplingFrequency() != 0 && !comp->getOption(TR_MimicInterpreterFrameShape)),
      _doNotCompileAgain(comp->getOption(TR_NoRecompile) || !comp->allowRecompilation()),
      _methodInfo(0),
      _bodyInfo(0),
      _nextLevel(warm),
      _nextCounter(0)
   {
   _timer.initialize(0, comp->trMemory());
   }


void
J9::Recompilation::setupMethodInfo()
   {
   // FIXME: this code should move into the constructor
   // the problem is - if someone gets the bodyinfo/method info before
   // this method is done - there will be a bug.

   // NOTE: by the end of this method we must have determined whether or
   // not this method is going to be recompiled using sampling or counting.
   //
   TR_OptimizationPlan * optimizationPlan =  _compilation->getOptimizationPlan();

   if (_firstCompile)
      {
      // Create the persistent method information
      // If the previous compiled version of the method is AOTed, then we need to create a new persistent method information
      //
      _methodInfo = new (PERSISTENT_NEW) TR_PersistentMethodInfo(_compilation);

      if (!_methodInfo)
         {
         _compilation->failCompilation<std::bad_alloc>("Unable to allocate method info");
         }

      // During compilation methodInfo->nextOptLevel is the current optlevel
      // ie. we pretend that the previous (nonexistent) compile has set this up
      // for us.  Mark this as a non-profiling compile.  Only sampleMethod
      // even enables profiling.
      //
      // During bootstrapping of TR::Options, _compilation->getMethodHotness
      // will be setup properly from the count strings
      //
      _methodInfo->setNextCompileLevel(optimizationPlan->getOptLevel(),
                                       (optimizationPlan->insertInstrumentation() != 0));
      _methodInfo->setWasNeverInterpreted(!comp()->fej9()->methodMayHaveBeenInterpreted(comp()));
      }
   else // this is a recompilation
      {
      // There already is a persistent method info for this method which already
      // has a compilation level set in it (by virtue of a previous compilation)
      //
      _methodInfo = getExistingMethodInfo(_compilation->getCurrentMethod());

      if (!comp()->fej9()->canRecompileMethodWithMatchingPersistentMethodInfo(comp()) &&
          !comp()->isGPUCompilation())
         {
         // TODO: Why does this assume sometimes fail in HCR mode?
         TR_ASSERT(_compilation->getMethodHotness() == _methodInfo->getNextCompileLevel(),
                   "discrepancy in the method compile level %d %d  while compiling %s",
                   _compilation->getMethodHotness(),
                   _methodInfo->getNextCompileLevel(),
                   _compilation->signature());
         }
      }

   // TR_Controller is being used and has provided a compilation plan.  Use it to setup the compilation
   //
   _bodyInfo = TR_PersistentJittedBodyInfo::allocate(
         _methodInfo,
         _compilation->getMethodHotness(),
         (optimizationPlan->insertInstrumentation() != 0),
         _compilation);

   if (!_bodyInfo)
      {
      _compilation->failCompilation<std::bad_alloc>("Unable to allocate body info");
      }

   if (!optimizationPlan->getUseSampling())
      {
      _bodyInfo->setDisableSampling(true);
      }

   if (_compilation->getOption(TR_EnableFastHotRecompilation) ||
       _compilation->getOption(TR_EnableFastScorchingRecompilation))
      {
      // The body must be able to receive samples and recompilation should be enabled
      //
      if (!_bodyInfo->getDisableSampling() && !_doNotCompileAgain)
         {
         if (_compilation->getOption(TR_EnableFastHotRecompilation) && _bodyInfo->getHotness() < hot)
            {
            _bodyInfo->setFastHotRecompilation(true);
            }

         if (_compilation->getOption(TR_EnableFastScorchingRecompilation) && _bodyInfo->getHotness() < scorching)
            {
            _bodyInfo->setFastScorchingRecompilation(true);
            }
         }
      }
   }


int32_t
J9::Recompilation::getProfilingFrequency()
   {
   return self()->findOrCreateProfileInfo()->getProfilingFrequency();
   }


int32_t
J9::Recompilation::getProfilingCount()
   {
   return self()->findOrCreateProfileInfo()->getProfilingCount();
   }

/**
 * Find or create profiling information for the current jitted
 * body. If creating information, this will update the recent
 * profile information on the current method info.
 */
TR_PersistentProfileInfo *
J9::Recompilation::findOrCreateProfileInfo()
   {
   // Determine whether this bodyInfo already has profiling information
   TR_PersistentProfileInfo *profileInfo = _bodyInfo->getProfileInfo();
   if (!profileInfo)
      {
      // Create a new profiling info
      profileInfo = new (PERSISTENT_NEW) TR_PersistentProfileInfo(DEFAULT_PROFILING_FREQUENCY, DEFAULT_PROFILING_COUNT);
      _methodInfo->setRecentProfileInfo(profileInfo);
      _bodyInfo->setProfileInfo(profileInfo);

      // If running with the profiling thread, add to its list
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableJProfilerThread))
         {
         TR::CompilationInfo::get(NULL)->getJProfilerThread()->addProfileInfo(profileInfo);
         }
      }
   return profileInfo;
   }

void
J9::Recompilation::startOfCompilation()
   {
   if (!_firstCompile && _compilation->getOption(TR_FailRecompile))
      {
      _compilation->failCompilation<TR::CompilationException>("failRecompile");
      }

   if (!_compilation->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      _timer.startTiming(_compilation);
      }
   }


void
J9::Recompilation::beforeOptimization()
   {
   // If the method is to be compiled with profiling info, force it to use
   // counting and not sampling. Also make sure there is a persistent
   // profile info block.
   //
   if (self()->isProfilingCompilation()) // this asks the bodyInfo
      {
      // JProfiling should not use either sampling or counting mechanism to trip recompilation.
      // Even though _useSampling is set to true here, at the end of compilation we disable sampling
      // causing us to rely only on the in-body recompilation test that relies on block frequency counter
      // to trip recompilation making sure we have enough profiling data to use in recompilation.
      _useSampling = _compilation->getProfilingMode() != JitProfiling;
      self()->findOrCreateProfileInfo()->setProfilingCount     (DEFAULT_PROFILING_COUNT);
      self()->findOrCreateProfileInfo()->setProfilingFrequency (DEFAULT_PROFILING_FREQUENCY);
      }

   // Create profilers
   //
   if (self()->couldBeCompiledAgain())
      {
      if (_compilation->getProfilingMode() == JProfiling)
         self()->createProfilers();
      else if (!self()->useSampling())
         {
         if (_compilation->getMethodHotness() == cold)
            {
            _profilers.add(new (_compilation->trHeapMemory()) TR_LocalRecompilationCounters(_compilation, self()));
            }
         else if (self()->isProfilingCompilation())
            {
            self()->createProfilers();
            }
         else if (!_compilation->getOption(TR_FullSpeedDebug))
            {
            _profilers.add(new (_compilation->trHeapMemory()) TR_GlobalRecompilationCounters(_compilation, self()));
            }
         }
      else
         {
         if (!debug("disableCatchBlockProfiler"))
            {
            _profilers.add(new (_compilation->trHeapMemory()) TR_CatchBlockProfiler(_compilation, self(), true));
            }
         }
      }
   }


void
J9::Recompilation::beforeCodeGen()
   {
   // Set up the opt level and counter for the next compilation. This will
   // also decide if there is going to be a next compilation.
   TR::CompilationController::getCompilationStrategy()->beforeCodeGen(_compilation->getOptimizationPlan(), self());
   }


void
J9::Recompilation::endOfCompilation()
   {
   // Perform any platform-dependant tasks if needed
   //
   self()->postCompilation();

   // We will free the optimizationPlan at the end of compilation,
   // do we need to clear the corresponding field in
   //
   TR::CompilationController::getCompilationStrategy()->postCompilation(_compilation->getOptimizationPlan(), self());

   if (self()->couldBeCompiledAgain())
      {
      _bodyInfo->setCounter(_nextCounter);

      // set the start counter to the current timestamp
      //
      _bodyInfo->setStartCount(globalSampleCount);
      _bodyInfo->setOldStartCountDelta(TR::Options::_sampleThreshold);
      _bodyInfo->setHotStartCountDelta(0);
      _bodyInfo->setSampleIntervalCount(0);

      // if the only possible next compilation of this method is
      // unnatural, _nextLevel will not be set, recompile at the
      // current level
      //
      if (!self()->shouldBeCompiledAgain())
         {
         _nextLevel = _compilation->getMethodHotness();
         }

      _methodInfo->setNextCompileLevel(_nextLevel, false); // profiling can only be triggered from sampleMethod

      _bodyInfo->setHasLoops        (_compilation->mayHaveLoops());
      _bodyInfo->setUsesPreexistence(_compilation->usesPreexistence());

      // if the only future compilation can be a forced one (preexistence)
      // do not sample this method.  If this is a counting based compilation
      // disable sampling as well
      //
      if (!self()->shouldBeCompiledAgain() || !_useSampling || _compilation->getProfilingMode() == JProfiling)
         {
         _bodyInfo->setDisableSampling(true);
         }
      }
   }


/**
  *   @brief
  *      Switches the current compilation to a profiling mode in which profiling group optimizations will
  *      run and potentially insert profiling trees into the compiled body.
  *
  *   @param f
  *      The profiling frequency for the persistent profiling info to use.
  *   @param c
  *      The profiling count for the persistent profiling info to use.
  *
  *   @return
  *      True if the compilation was successfully switched to a profiling compilation; false otherwise.
  */
bool
J9::Recompilation::switchToProfiling(uint32_t f, uint32_t c)
   {
   if (_compilation->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      return false;
      }

   if (!_methodInfo)
      {
      return false;
      }

   if (_methodInfo->profilingDisabled())
      {
      return false;
      }

   if (!self()->countingSupported())
      {
      return false;
      }

   if (self()->isProfilingCompilation())
      {
      return true;
      }

   if (!TR::CompilationController::getCompilationStrategy()->enableSwitchToProfiling())
      {
      return false;
      }

   if (comp()->getOptimizationPlan()->getDoNotSwitchToProfiling())
      {
      return false;
      }

   if (_compilation->isOptServer() && !TR::Options::getCmdLineOptions()->getOption(TR_AggressiveOpts))
      {
      // can afford to switch to profiling under aggressive; needed for BigDecimal opt
      //
      return false;
      }

   if (!_bodyInfo->getIsProfilingBody())
      {
      if (!performTransformation(comp(), "\nSwitching the compile to do profiling (isProfilingCompile=1)\n"))
          return false;
      }

   _bodyInfo->setIsProfilingBody(true);

   // If profiling will use JProfiling instrumentation and this is post JProfilingBlock opt pass, trigger restart
   if (_compilation->getProfilingMode() == JProfiling && _compilation->getSkippedJProfilingBlock())
      {
      TR::DebugCounter::incStaticDebugCounter(_compilation, TR::DebugCounter::debugCounterName(_compilation,
         "jprofiling.restartCompile/(%s)", _compilation->signature()));
      if (TR::Options::getVerboseOption(TR_VerboseProfiling))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PROFILING, "Restarting compilation due to late switch to profiling");
      comp()->failCompilation<J9::EnforceProfiling>("Enforcing profiling compilation");
      }

   _useSampling = _compilation->getProfilingMode() != JitProfiling;
   self()->findOrCreateProfileInfo()->setProfilingFrequency(f);
   self()->findOrCreateProfileInfo()->setProfilingCount(c);
   self()->createProfilers();
   return true;
   }


bool
J9::Recompilation::switchToProfiling()
   {
   return self()->switchToProfiling(DEFAULT_PROFILING_FREQUENCY, DEFAULT_PROFILING_COUNT);
   }


void
J9::Recompilation::switchAwayFromProfiling()
   {
   _bodyInfo->setIsProfilingBody(false);
   _useSampling = true;
   }


void
J9::Recompilation::createProfilers()
   {
   if (!self()->getValueProfiler())
      _profilers.add(new (_compilation->trHeapMemory()) TR_ValueProfiler(_compilation, self(),
         _compilation->getProfilingMode() == JProfiling ? HashTableProfiler : LinkedListProfiler ));

   if (!self()->getBlockFrequencyProfiler() && _compilation->getProfilingMode() != JProfiling)
      _profilers.add(new (_compilation->trHeapMemory()) TR_BlockFrequencyProfiler(_compilation, self()));
   }


bool
J9::Recompilation::couldBeCompiledAgain()
   {
   return
      self()->shouldBeCompiledAgain() ||
      _compilation->usesPreexistence() ||
      _compilation->getOption(TR_EnableHCR);
   }


bool
J9::Recompilation::shouldBeCompiledAgain()
   {
   return TR::Options::canJITCompile() && !_doNotCompileAgain;
   }


void
J9::Recompilation::preventRecompilation()
   {
   _nextCounter = 0;
   _doNotCompileAgain = true;

   for (TR_RecompilationProfiler * rp = getFirstProfiler(); rp; rp = rp->getNext())
      {
      if (rp->getHasModifiedTrees())
         {
         rp->removeTrees();
         rp->setHasModifiedTrees(false);
         }
      }
   }


TR_PersistentMethodInfo *
J9::Recompilation::getExistingMethodInfo(TR_ResolvedMethod *method)
   {
   // Default (null) implementation
   //
   TR_ASSERT(0, "This method needs to be implemented");
   return 0;
   }

/**
 * This method can extract a value profiler from the current list of
 * recompilation profilers.
 * 
 * \return The first TR_ValueProfiler in the current list of profilers, NULL if there are none.
 */
TR_ValueProfiler *
J9::Recompilation::getValueProfiler()
   {
   for (TR_RecompilationProfiler * rp = getFirstProfiler(); rp; rp = rp->getNext())
      {
      TR_ValueProfiler *vp = rp->asValueProfiler();
      if (vp)
         return vp;
      }

   return NULL;
   }

/**
 * This method can extract a block profiler from the current list of
 * recompilation profilers.
 *
 * \return The first TR_BlockFrequencyProfiler in the current list of profilers, NULL if there are none.
 */
TR_BlockFrequencyProfiler *
J9::Recompilation::getBlockFrequencyProfiler()
   {
   for (TR_RecompilationProfiler * rp = getFirstProfiler(); rp; rp = rp->getNext())
      {
      TR_BlockFrequencyProfiler *vp = rp->asBlockFrequencyProfiler();
      if (vp)
         return vp;
      }

   return NULL;
   }

/**
 * This method can remove a specified recompilation profiler from the
 * current list. Useful when modifying the profiling strategy.
 *
 * \param rp The recompilation profiler to remove.
 */
void
J9::Recompilation::removeProfiler(TR_RecompilationProfiler *rp)
   {
   _profilers.remove(rp);
   }

TR_Hotness
J9::Recompilation::getNextCompileLevel(void *oldStartPC)
   {
   TR_PersistentMethodInfo *methodInfo = TR::Recompilation::getMethodInfoFromPC(oldStartPC);
   return methodInfo->getNextCompileLevel();
   }


TR::SymbolReference *
J9::Recompilation::getCounterSymRef()
   {
   return _compilation->getSymRefTab()->findOrCreateRecompilationCounterSymbolRef(_bodyInfo->getCounterAddress());
   }


/////////////////////////
// DEBUG
//////////////////////////

void
J9::Recompilation::shutdown()
   {
   static bool TR_RecompilationStats = feGetEnv("TR_RecompilationStats")?1:0;
   if (TR_RecompilationStats)
      {
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"Methods recompiled via count = %d", limitMethodsCompiled);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"Methods recompiled via hot threshold = %d", hotThresholdMethodsCompiled);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"Methods recompiled via scorching threshold = %d", scorchingThresholdMethodsCompiled);
      }
   }




TR_PersistentMethodInfo::TR_PersistentMethodInfo(TR::Compilation *comp) :
   _methodInfo((TR_OpaqueMethodBlock *)comp->getCurrentMethod()->resolvedMethodAddress()),
   _flags(0),
   _nextHotness(unknownHotness),
   _recentProfileInfo(0),
   _bestProfileInfo(0),
   _optimizationPlan(0),
   _numberOfInvalidations(0),
   _numberOfInlinedMethodRedefinition(0),
   _numPrexAssumptions(0)
   {
   if (comp->getOption(TR_EnableHCR) && !comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      // If the method gets replaced, we'll want _methodInfo to get updated
      //
      comp->cg()->jitAddPicToPatchOnClassRedefinition((void*)_methodInfo, (void*)&_methodInfo, false);
      }

   if (comp->getOption(TR_DisableProfiling))
      {
      setDisableProfiling();
      }

   // Start cpoSampleCounter at 1.  Because the method sample count
   // is stored in the compiled method info, we can't start counting
   // until already compiled once, thus we missed the first sample.
   // (not particularly clean solution.  Should really attach the
   // counter to the method, not the compiled-method)
   //
   _cpoSampleCounter = 1;

   uint64_t tempTimeStamp = comp->getPersistentInfo()->getElapsedTime();
   if (tempTimeStamp < (uint64_t)0x0FFFF)
      _timeStamp = (uint16_t)tempTimeStamp;
   else
      _timeStamp = (uint16_t)0xFFFF;
   }

TR_PersistentMethodInfo::TR_PersistentMethodInfo(TR_OpaqueMethodBlock *methodInfo) :
   _methodInfo(methodInfo),
   _flags(0),
   _nextHotness(unknownHotness),
   _recentProfileInfo(0),
   _bestProfileInfo(0),
   _optimizationPlan(0),
   _numberOfInvalidations(0),
   _numberOfInlinedMethodRedefinition(0),
   _numPrexAssumptions(0)
   {
   }

TR_PersistentMethodInfo *
TR_PersistentMethodInfo::get(TR::Compilation *comp)
   {
   TR::Recompilation * recomp = comp->getRecompilationInfo();
   return recomp ? recomp->getMethodInfo() : 0;
   }

TR_PersistentMethodInfo *
TR_PersistentMethodInfo::get(TR_ResolvedMethod * feMethod)
   {
   if (feMethod->isInterpreted() || feMethod->isJITInternalNative())
      return 0;

   void *startPC = (void *)feMethod->startAddressForInterpreterOfJittedMethod();
   TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
   return bodyInfo ? bodyInfo->getMethodInfo() : 0;
   }

TR_PersistentJittedBodyInfo::TR_PersistentJittedBodyInfo(
      TR_PersistentMethodInfo *methodInfo,
      TR_Hotness hotness,
      bool profile,
      TR::Compilation *comp) :
   _methodInfo(methodInfo),
   _counter(INT_MAX),
   _mapTable(comp == 0 || comp->getOption(TR_DoNotUseFastStackwalk) ? 0 : (void *)-1),
   _hotness(hotness),
   _flags(0),
   _sampleIntervalCount(0),
   _startCount(0),
   _isInvalidated(false),
   _aggressiveRecompilationChances((uint8_t)TR::Options::_aggressiveRecompilationChances),
   _startPCAfterPreviousCompile(0),
   _longRunningInterpreted(false),
   _numScorchingIntervals(0),
   _profileInfo(0)
   ,_hwpInstructionStartCount(0),
    _hwpInstructionCount(0),
    _hwpInducedRecompilation(false),
    _hwpReducedWarmCompileRequested(false),
    _hwpReducedWarmCompileInQueue(false)
   {
   setIsProfilingBody(profile);
   }

TR_PersistentJittedBodyInfo *
TR_PersistentJittedBodyInfo::allocate(
      TR_PersistentMethodInfo *methodInfo,
      TR_Hotness hotness,
      bool profile,
      TR::Compilation *comp)
   {
   return new (PERSISTENT_NEW) TR_PersistentJittedBodyInfo(methodInfo, hotness, profile, comp);
   }

/**
 * Increment reference count for shared profile info.
 *
 * This function and setForSharedInfo use the pointer's low bit as a lock,
 * preventing other accesses to the persistent information through this pointer.
 * This is necessary as the process of incrementing the reference count isn't
 * atomic with the pointer updates.
 *
 * When locked, its able to safely dereference the pointer and increment
 * its reference count. Without the lock, the information could have been
 * deallocated in the interim.
 *
 * \param ptr Pointer shared across several threads.
 */
TR_PersistentProfileInfo *
TR_PersistentMethodInfo::getForSharedInfo(TR_PersistentProfileInfo** ptr)
   {
   uintptr_t locked;
   uintptr_t unlocked;

   // Lock the ptr
   do {
      locked = ((uintptr_t) *ptr) | (1ULL);
      unlocked = ((uintptr_t) *ptr) & ~(1ULL);
      if (unlocked == 0)
         return NULL;
      }
   while (unlocked != VM_AtomicSupport::lockCompareExchange((uintptr_t*)ptr, unlocked, locked));

   // Inc ref count
   TR_PersistentProfileInfo::incRefCount((TR_PersistentProfileInfo*)unlocked);

   // Unlock the ptr
   // Assumes no updates to the ptr whilst locked
   VM_AtomicSupport::set((uintptr_t*)ptr, unlocked);

   return *ptr;
   }

/**
 * Update a shared pointer to reference a new persistent profile info.
 *
 * This should be used with getForSharedInfo, to ensure a shared persistent profile info is
 * updated correctly.
 *
 * \param ptr Pointer shared across several threads.
 * \param newInfo New persistent profile info to use. Should have a non-zero reference count in caller.
 */
void
TR_PersistentMethodInfo::setForSharedInfo(TR_PersistentProfileInfo** ptr, TR_PersistentProfileInfo *newInfo)
   {
   uintptr_t oldPtr;

   // Before it can be accessed, inc ref count on new info
   if (newInfo)
      TR_PersistentProfileInfo::incRefCount(newInfo);
   
   // Update ptr as if it was unlocked
   // Doesn't matter what the old info was, as long as it was unlocked
   do {
      oldPtr = ((uintptr_t) *ptr) & ~(1ULL);
      }
   while (oldPtr != VM_AtomicSupport::lockCompareExchange((uintptr_t*)ptr, oldPtr, (uintptr_t)newInfo));

   // Now that it can no longer be accessed, dec ref count on old info
   if (oldPtr)
      TR_PersistentProfileInfo::decRefCount((TR_PersistentProfileInfo*)oldPtr);
   }
