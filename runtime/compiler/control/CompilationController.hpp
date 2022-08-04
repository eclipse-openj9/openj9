/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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

#ifndef COMPILATIONCONTROLLER_INCL
#define COMPILATIONCONTROLLER_INCL

#include "compile/CompilationTypes.hpp"
#include "control/OptimizationPlan.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/VMJ9.h"

class TR_OptimizationPlan;
class TR_PersistentJittedBodyInfo;
class TR_PersistentMethodInfo;
struct TR_MethodToBeCompiled;

int32_t printTruncatedSignature(char *sigBuf, int32_t bufLen, J9Method *j9method);

extern "C" {
int32_t returnIprofilerState();
}

//--------------------------- TR_MethodEvent ----------------------------
class TR_MethodEvent
   {
   public:
   enum { InvalidEvent=0,
          InterpreterCounterTripped,
          InterpretedMethodSample,
          JittedMethodSample,
          MethodBodyInvalidated,
          NewInstanceImpl,
          ShareableMethodHandleThunk,
          CustomMethodHandleThunk,
          OtherRecompilationTrigger,
          JitCompilationInducedByDLT,
          HWPRecompilationTrigger,
          CompilationBeforeCheckpoint,
          NumEvents      // must be the last one
   };
   int32_t      _eventType;
   J9Method *   _j9method;
   void *       _oldStartPC;
   void *       _samplePC;
   J9VMThread * _vmThread;
   J9Class *    _classNeedingThunk; // for newInstanceImpl
   TR_Hotness   _nextOptLevel; // Used for HWP-based recompilation
   };


//-------------------------TR::DefaultCompilationStrategy ----------------------
namespace TR
{
class DefaultCompilationStrategy : public TR::CompilationStrategy
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   DefaultCompilationStrategy();
   TR_OptimizationPlan *processEvent(TR_MethodEvent *event, bool *newPlanCreated);
   TR_OptimizationPlan *processInterpreterSample(TR_MethodEvent *event);
   TR_OptimizationPlan *processJittedSample(TR_MethodEvent *event);
   TR_OptimizationPlan *processHWPSample(TR_MethodEvent *event);
   TR_Hotness getInitialOptLevel(J9Method *j9method);
   bool adjustOptimizationPlan(TR_MethodToBeCompiled *entry, int32_t optLevelAdjustment);
   void beforeCodeGen(TR_OptimizationPlan *plan, TR::Recompilation *recomp);
   void postCompilation(TR_OptimizationPlan *plan, TR::Recompilation *recomp);
   void shutdown();
   virtual bool enableSwitchToProfiling() { return !TR::CodeCacheManager::instance()->almostOutOfCodeCache(); }
   private:

   class ProcessJittedSample
      {
      public:

      /**
       * @brief Construct a new Process Jitted Sample object.
       *
       * @param jitConfig The J9JITConfig
       * @param vmThread The J9VMThread of the current thread
       * @param compInfo The TR::CompilationINfo
       * @param fe The TR_J9VMBase front end
       * @param cmdLineOptions The TR::Options object
       * @param j9method The J9Method associated with the sample
       * @param event The TR_MethodEvent associated with the sample
       */
      ProcessJittedSample(J9JITConfig *jitConfig,
                          J9VMThread *vmThread,
                          TR::CompilationInfo *compInfo,
                          TR_J9VMBase *fe,
                          TR::Options *cmdLineOptions,
                          J9Method *j9method,
                          TR_MethodEvent *event);

      /**
       * @brief Process a sample in a JIT method and trigger a recompilation if needed.
       *
       * @return Pointer to a TR_OptimizationPlan if a recomp is triggered, NULL otherwise.
       */
      TR_OptimizationPlan * process();

      private:

      /**
       * @brief Log basic sample info to the buffer.
       */
      void logSampleInfoToBuffer();

      /**
       * @brief Print all the info placed into the buffer to the verbose log.
       */
      void printBufferToVLog();

      /**
       * @brief Yield to application threads if requested.
       */
      void yieldToAppThread();

      /**
       * @brief Find the TR_PersistentJittedBodyInfo associated with this sample.
       *        Depending on the flags in the LinkageInfo, this method may not
       *        set the Body and Method infos.
       */
      void findAndSetBodyAndMethodInfo();

      /**
       * @brief Determine whether to process the sample.
       *
       * @return true if the sample will be processed, false otherwise
       */
      bool shouldProcessSample();

      /**
       * @brief Initialie fields only used if shouldProcessSample above returns true.
       */
      void initializeRecompRelatedFields();

      /**
       * @brief Determine whether to recompile the method if the count in the bodyinfo
       *        hits zero.
       */
      void determineWhetherToRecompileIfCountHitsZero();

      /**
       * @brief Determine whether to recompile the method if the higher opt level
       *        thresholds are met.
       */
      void determineWhetherToRecompileBasedOnThreshold();

      /**
       * @brief Determine whether the method should be recompiled at the hot optimization
       *        level, or if it should recompiled at an even higher level.
       *
       * @param scalingFactor Factor used to scale the thresholds used in determining recomp decisions
       * @param conservativeCase Bool to determine whether to be conservative with the decision
       * @param useAggressiveRecompilations Bool to determine whether be aggressive with recomp decisions
       * @param isBigAppStartup Bool to determine if the JIT is currently optimizing for Big App Startup
       */
      void determineWhetherRecompileIsHotOrScorching(float scalingFactor, bool conservativeCase, bool useAggressiveRecompilations, bool isBigAppStartup);

      /**
       * @brief Determine whether the method should be upgraded even if the previous
       *        criteria was not sufficient.
       */
      void determineWhetherToRecompileLessOptimizedMethods();

      /**
       * @brief Queue the method for recompilation if needed.
       *
       * @return Pointer to a TR_OptimizationPlan if a recomp is triggered, NULL otherwise.
       */
      TR_OptimizationPlan * triggerRecompIfNeeded();

      J9JITConfig                 *_jitConfig;
      J9VMThread                  *_vmThread;
      TR::CompilationInfo         *_compInfo;
      TR_J9VMBase                 *_fe;
      TR::Options                 *_cmdLineOptions;
      J9Method                    *_j9method;
      TR_MethodEvent              *_event;
      void                        *_startPC;
      TR_PersistentJittedBodyInfo *_bodyInfo;
      TR_PersistentMethodInfo     *_methodInfo;
      bool                         _isAlreadyBeingCompiled;

      static const uint32_t MSG_SZ = 450;
      bool _logSampling;
      char _msg[MSG_SZ];
      char *_curMsg;

      int32_t _totalSampleCount;

      // Recomp related fields
      bool _recompile;
      bool _useProfiling;
      bool _dontSwitchToProfiling;
      bool _postponeDecision;
      bool _willUpgrade;
      TR_Hotness _nextOptLevel;
      uint32_t _intervalIncreaseFactor;
      int32_t _scorchingSampleInterval;
      uint8_t _hotSampleInterval;
      int32_t _hotSampleThreshold;
      int32_t _count;
      uint8_t _crtSampleIntervalCount;
      bool _hotSamplingWindowComplete;
      bool _scorchingSamplingWindowComplete;
      int32_t _startSampleCount;
      int32_t _globalSamples;
      int32_t _globalSamplesInHotWindow;
      int32_t _scaledScorchingThreshold;
      int32_t _scaledHotThreshold;
      };

   // statistics regarding the events it receives
   unsigned _statEventType[TR_MethodEvent::NumEvents];
   };
} // namespace TR

//----------------------- TR::ThresholdCompilationStrategy ---------------------
namespace TR
{

class ThresholdCompilationStrategy : public TR::CompilationStrategy
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo); // Do I need this ?
   ThresholdCompilationStrategy();
   TR_OptimizationPlan *processEvent(TR_MethodEvent *event, bool *newPlanCreated);
   TR_OptimizationPlan *processJittedSample(TR_MethodEvent *event);
   TR_Hotness getInitialOptLevel();
   int32_t getSamplesNeeded(TR_Hotness optLevel) { TR_ASSERT(_samplesNeededToMoveTo[optLevel] >= 0, "must be valid entry"); return _samplesNeededToMoveTo[optLevel]; }
   bool getPerformInstrumentation(TR_Hotness optLevel) { return _performInstrumentation[optLevel]; }
   TR_Hotness getNextOptLevel(TR_Hotness curOptLevel) { TR_ASSERT(_nextLevel[curOptLevel] != unknownHotness, "must be valid opt level"); return _nextLevel[curOptLevel]; }

   private:
   TR_Hotness _nextLevel[numHotnessLevels+1];
   int32_t _samplesNeededToMoveTo[numHotnessLevels+1];
   bool    _performInstrumentation[numHotnessLevels+1];
   };
} // namespace TR
#endif // ifndef COMPILATIONCONTROLLER_INCL
