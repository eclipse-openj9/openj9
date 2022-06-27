/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
