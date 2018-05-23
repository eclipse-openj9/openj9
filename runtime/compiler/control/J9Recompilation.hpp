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

#ifndef J9_RECOMPILATION_INCL
#define J9_RECOMPILATION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_RECOMPILATION_CONNECTOR
#define J9_RECOMPILATION_CONNECTOR
namespace J9 { class Recompilation; }
namespace J9 { typedef J9::Recompilation RecompilationConnector; }
#endif

#include "control/OMRRecompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include <stdint.h>

namespace TR { class DefaultCompilationStrategy; }
class TR_ValueProfiler;
class TR_RecompilationProfiler;

namespace J9
{

class Recompilation : public OMR::RecompilationConnector
   {
   friend class TR::DefaultCompilationStrategy;

public:

   Recompilation(TR::Compilation *comp);

   void setupMethodInfo();

   void createProfilers();

   TR_PersistentJittedBodyInfo *getJittedBodyInfo() { return _bodyInfo; }
   TR_PersistentMethodInfo *getMethodInfo() { return _methodInfo; }
   void setMethodInfo(TR_PersistentMethodInfo *methodInfo) { _methodInfo = methodInfo; }
   void setJittedBodyInfo(TR_PersistentJittedBodyInfo *bodyInfo) { _bodyInfo = bodyInfo; }

   TR_ValueProfiler * getValueProfiler();
   TR_BlockFrequencyProfiler * getBlockFrequencyProfiler();
   TR_RecompilationProfiler * getFirstProfiler() { return _profilers.getFirst(); }
   void removeProfiler(TR_RecompilationProfiler *rp);

   bool isProfilingCompilation() { return _bodyInfo->getIsProfilingBody(); }

   // for replay
   void setIsProfilingCompilation(bool v) { _bodyInfo->setIsProfilingBody(v); }

   TR_PersistentProfileInfo *findOrCreateProfileInfo();

   bool useSampling() {return _useSampling;}

   bool switchToProfiling(uint32_t freq, uint32_t count);
   bool switchToProfiling();
   void switchAwayFromProfiling();

   int32_t getProfilingFrequency();
   int32_t getProfilingCount();

   TR::SymbolReference *getCounterSymRef();
   void *getCounterAddress() { return _bodyInfo->getCounterAddress(); }

   bool isRecompilation() { return !_firstCompile; }

   void startOfCompilation();
   void beforeOptimization();
   void beforeCodeGen();
   void endOfCompilation();

   bool couldBeCompiledAgain();
   bool shouldBeCompiledAgain();

   void preventRecompilation();

   static void shutdown();

   static TR_PersistentJittedBodyInfo  *getJittedBodyInfoFromPC(void *startPC);

   static TR_PersistentMethodInfo *getMethodInfoFromPC(void *startPC)
      {
      TR_PersistentJittedBodyInfo *jbi = getJittedBodyInfoFromPC(startPC);
      return jbi? jbi->getMethodInfo() : NULL;
      }

   static bool countingSupported() { return _countingSupported; }
   static TR_Hotness getNextCompileLevel(void *oldStartPC);
   static void methodHasBeenRecompiled(void *oldStartPC, void *newStartPC, TR_FrontEnd *fe);
   static void fixUpMethodCode(void *startPC);

   // Recompile the method when convenient
   //
   static bool induceRecompilation(TR_FrontEnd *fe, void *startPC, bool *queued, TR_OptimizationPlan *optimizationPlan = NULL);

   static bool isAlreadyBeingCompiled(TR_OpaqueMethodBlock *methodInfo, void *startPC, TR_FrontEnd *fe);
   static void methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe);
   static void invalidateMethodBody(void *startPC, TR_FrontEnd *fe);

   // Called at runtime to sample a method
   //
   static void sampleMethod(void *vmThread, TR_FrontEnd *fe, void *startPC, int32_t codeSize, void *samplePC, void *methodInfo, int32_t tickCount);

   static bool isAlreadyPreparedForRecompile(void *startPC);

   static int32_t globalSampleCount;
   static int32_t hwpGlobalSampleCount;
   static int32_t jitGlobalSampleCount;
   static int32_t jitRecompilationsInduced;

protected:

   virtual TR_PersistentMethodInfo *getExistingMethodInfo(TR_ResolvedMethod *method);

   static int32_t limitMethodsCompiled;
   static int32_t hotThresholdMethodsCompiled;
   static int32_t scorchingThresholdMethodsCompiled;
   static bool _countingSupported;

   bool _firstCompile;
   bool _useSampling;
   bool _doNotCompileAgain;
   TR_Hotness _nextLevel;
   int32_t _nextCounter;
   TR_SingleTimer _timer;

   TR_PersistentMethodInfo *_methodInfo;
   TR_PersistentJittedBodyInfo *_bodyInfo;

   TR_LinkHead<TR_RecompilationProfiler> _profilers;
   };
}


// A way to call induceRecompilation from ASM via jitCallCFunction.
// argsPtr[0] == startPC, argsPtr[1] == vmThread.
// resultPtr is ignored.
//
extern "C" void induceRecompilation_unwrapper(void **argsPtr, void **resultPtr);


#endif
