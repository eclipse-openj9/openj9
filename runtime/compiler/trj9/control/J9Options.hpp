/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef J9_OPTIONS_INCL
#define J9_OPTIONS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_OPTIONS_CONNECTOR
#define J9_OPTIONS_CONNECTOR
namespace J9 { class Options; }
namespace J9 { typedef J9::Options OptionsConnector; }
#endif

#include "control/OMROptions.hpp"

#include <stdint.h>
#include "control/OptionsUtil.hpp"
#include "env/jittypes.h"

namespace J9
{

class OMR_EXTENSIBLE Options : public OMR::OptionsConnector
   {
   public:

   Options() : OMR::OptionsConnector() {}

   Options(TR_Memory * m,
           int32_t index,
           int32_t lineNumber,
           TR_ResolvedMethod *compilee,
           void *oldStartPC,
           TR_OptimizationPlan *optimizationPlan,
           bool isAOT=false,
           int32_t compThreadID=-1)
      : OMR::OptionsConnector(m,index,lineNumber,compilee,oldStartPC,optimizationPlan,isAOT,compThreadID)
      {}

   Options(TR::Options &other) : OMR::OptionsConnector(other) {}



   static bool _doNotProcessEnvVars;

   static int32_t _samplingFrequencyInIdleMode;
   static int32_t getSamplingFrequencyInIdleMode() {return _samplingFrequencyInIdleMode;}

   static int32_t _samplingFrequencyInDeepIdleMode;
   static int32_t getSamplingFrequencyInDeepIdleMode() {return _samplingFrequencyInDeepIdleMode;}
   
   static int32_t _maxCheckcastProfiledClassTests;
   static int32_t getCheckcastMaxProfiledClassTests() {return _maxCheckcastProfiledClassTests;}

   static int32_t _maxOnsiteCacheSlotForInstanceOf;
   /** \brief
    *     Returns the _maxOnsiteCacheSlotForInstanceOf
    *
    *  \details
    *     maxOnsiteCacheSlotForInstanceOf node defines number of onsite cache slots to generate per site of instanceOf
    *     Set this value to 0 to disable generation of onsite cache test for instanceOf
    *
    */
   static int32_t getMaxOnsiteCacheSlotForInstanceOf() {return _maxOnsiteCacheSlotForInstanceOf;}
   /** \brief
    *     Setter for _maxOnsiteCacheSlotForInstanceOf
    *
    */
   static int32_t setMaxOnsiteCacheSlotForInstanceOf(int32_t m) {return _maxOnsiteCacheSlotForInstanceOf = m;}

   static int32_t _resetCountThreshold;

   static int32_t _scorchingSampleThreshold;
   static int32_t _conservativeScorchingSampleThreshold; // used when many CPUs

   static int32_t _upperBoundNumProcForScaling;
   static int32_t _lowerBoundNumProcForScaling;
   static int32_t _veryHotSampleThreshold;
   static int32_t _relaxedCompilationLimitsSampleThreshold;
   static int32_t _sampleThresholdVariationAllowance;
   static int32_t _cpuEntitlementForConservativeScorching;

   static int32_t _sampleHeartbeatInterval;
   static int32_t getSamplingHeartbeatInterval() { return _sampleHeartbeatInterval;}

   static int32_t _sampleDontSwitchToProfilingThreshold;
   static int32_t _stackSize;
   static int32_t _profilerStackSize;
   static int32_t _smallMethodBytecodeSizeThreshold; // used on setting invocation counts
   static int32_t _smallMethodBytecodeSizeThresholdForCold; // used in GCR filtering

   static int32_t _countForMethodsCompiledDuringStartup;
   static int32_t getCountForMethodsCompiledDuringStartup() { return _countForMethodsCompiledDuringStartup; }

   // fast JNI option
   static TR::SimpleRegex *_jniAccelerator;
   static TR::SimpleRegex * getJniAccelerator() { return _jniAccelerator; }
   static void setJniAccelerator(TR::SimpleRegex *ja) { _jniAccelerator = ja; }

   static int32_t _classLoadingPhaseInterval;
   static int32_t _experimentalClassLoadPhaseInterval;
   static int32_t _classLoadingPhaseThreshold;
   static int32_t _classLoadingPhaseVariance;
   static int32_t _classLoadingRateAverage;
   static int32_t _secondaryClassLoadingPhaseThreshold;
   static int32_t _numClassLoadPhaseQuiesceIntervals;
   static int32_t _bigAppSampleThresholdAdjust; //shift value assigned to certain big app's ho and scorching threshold
   static int32_t _availableCPUPercentage;
   static int32_t _userClassLoadingPhaseThreshold;
   static bool _userClassLoadingPhase;

   static int32_t _cpuCompTimeExpensiveThreshold;
   int32_t getCpuCompTimeExpensiveThreshold() { return _cpuCompTimeExpensiveThreshold; }

   static uintptrj_t _compThreadAffinityMask;
   static int32_t _interpreterSamplingThreshold;
   static int32_t _interpreterSamplingDivisor;
   static int32_t _interpreterSamplingThresholdInStartupMode;
   static int32_t _interpreterSamplingThresholdInJSR292;
   static int32_t _activeThreadsThreshold; // -1 means 'determine dynamically', 0 means feature disabled
   static int32_t _samplingThreadExpirationTime;
   static int32_t _compilationExpirationTime;
   static int32_t _catchSamplingSizeThreshold;
   static int32_t _compilationThreadPriorityCode; // a number between 0 and 4
   static int32_t _disableIProfilerClassUnloadThreshold;
   static int32_t _iprofilerReactivateThreshold;
   static int32_t _iprofilerIntToTotalSampleRatio;
   static int32_t _iprofilerSamplesBeforeTurningOff;
   static int32_t _iprofilerNumOutstandingBuffers;
   static int32_t _iprofilerBufferMaxPercentageToDiscard;
   static int32_t _iProfilerBufferInterarrivalTimeToExitDeepIdle; // ms
   static int32_t _iprofilerBufferSize; //iprofilerbuffer size in kb

   static int32_t _maxIprofilingCount; // when invocation count is larger than
                                       // this value Iprofiler will not collect data
   static int32_t _maxIprofilingCountInStartupMode; // same as above but value is used
                                                    // during STARTUP phase (and only during classLoadPhase)
   static int32_t _iprofilerFailRateThreshold; // will reactivate Iprofiler if failure rate exceeds this threshold
   static int32_t _iprofilerFailHistorySize;
   static int32_t _iProfilerMemoryConsumptionLimit;
   static int32_t _IprofilerOffSubtractionFactor;
   static int32_t _IprofilerOffDivisionFactor;

   static int32_t _LoopyMethodSubtractionFactor;
   static int32_t _LoopyMethodDivisionFactor;

   static int32_t _localCSEFrequencyThreshold;
   static int32_t _profileAllTheTime;
   static int32_t _seriousCompFailureThreshold; // above this threshold we generate a trace point in the Snap file
   static bool _useCPUsToDetermineMaxNumberOfCompThreadsToActivate;

   static int32_t _numCodeCachesToCreateAtStartup;
   static int32_t getNumCodeCachesToCreateAtStartup() { return _numCodeCachesToCreateAtStartup; }

   static int32_t _dataCacheQuantumSize;
   static int32_t _dataCacheMinQuanta;
   static int32_t getDataCacheQuantumSize() { return _dataCacheQuantumSize; }
   static int32_t getDataCacheMinQuanta() { return _dataCacheMinQuanta; }

   static size_t _scratchSpaceLimitKBWhenLowVirtualMemory;
   static size_t getScratchSpaceLimitKBWhenLowVirtualMemory() { return _scratchSpaceLimitKBWhenLowVirtualMemory; }

   static int32_t _scratchSpaceFactorWhenJSR292Workload;
   static int32_t getScratchSpaceFactorWhenJSR292Workload() { return _scratchSpaceFactorWhenJSR292Workload; }

   static int32_t _lowVirtualMemoryMBThreshold;
   static int32_t getLowVirtualMemoryMBThreshold() { return _lowVirtualMemoryMBThreshold; }

   static int32_t _safeReservePhysicalMemoryValue;
   inline static int32_t getSafeReservePhysicalMemoryValue() { return _safeReservePhysicalMemoryValue; }
   inline static void setSafeReservePhysicalMemoryValue(int32_t size) { _safeReservePhysicalMemoryValue = size; }

   static int32_t _updateFreeMemoryMinPeriod;
   inline static int32_t getUpdateFreeMemoryMinPeriod() { return _updateFreeMemoryMinPeriod; }
   inline static void setUpdateFreeMemoryMinPeriod(int32_t value) { _updateFreeMemoryMinPeriod = value; }

   static int32_t _numDLTBufferMatchesToEagerlyIssueCompReq;
   static int32_t _dltPostponeThreshold;



   static int32_t _minSamplingPeriod;
   int32_t getMinSamplingPeriod() {return _minSamplingPeriod;}
   static int32_t _compilationBudget;

   static int32_t _compYieldStatsThreshold;
   static int32_t _compYieldStatsHeartbeatPeriod;
   static int32_t _numberOfUserClassesLoaded;
   static int32_t _numQueuedInvReqToDowngradeOptLevel;
   static int32_t _qszThresholdToDowngradeOptLevel;
   static int32_t _qsziThresholdToDowngradeDuringCLP;
   static int32_t _cpuUtilThresholdForStarvation;
   static int32_t _compPriorityQSZThreshold;
   static int32_t _GCRQueuedThresholdForCounting; // if too many GCR are queued we stop counting
   static int32_t _minimumSuperclassArraySize; //size of the minimum superclass array

   static int32_t _TLHPrefetchSize;
   static int32_t _TLHPrefetchLineSize;
   static int32_t _TLHPrefetchLineCount;
   static int32_t _TLHPrefetchStaggeredLineCount;
   static int32_t _TLHPrefetchBoundaryLineCount;
   static int32_t _TLHPrefetchTLHEndLineCount;
   static int32_t _numFirstTimeCompilationsToExitIdleMode; // use large number to disable the feature

   static int32_t _waitTimeToEnterIdleMode;
   static int32_t _waitTimeToEnterDeepIdleMode;
   static int32_t _waitTimeToExitStartupMode;
   static int32_t _waitTimeToGCR;
   static int32_t _waitTimeToStartIProfiler;
   static int32_t _compilationDelayTime;

   static int32_t _invocationThresholdToTriggerLowPriComp; // we trigger an LPQ comp req only if the method
                                                           // was invoked at least this many times
   static int32_t _aotMethodThreshold;         // when number of methods found in shared cache exceeds this threshold
                                               // we stop AOTing new methods to be put in shared cache UNLESS
   static int32_t _aotMethodCompilesThreshold; // we have already AOT compiled at least this many methods
                                               //   complication due to zOS trade scenario: two JVMs share a cache
   static int32_t _aotWarmSCCThreshold; // if there are at least that many AOT bodies in SCC at startup
                                        // then we declare the SCC to be warm
   static int32_t _largeTranslationTime; // usec
   static int32_t _weightOfAOTLoad;
   static int32_t _weightOfJSR292;

   static int32_t _hwprofilerNumOutstandingBuffers;

   static TR_YesNoMaybe _hwProfilerEnabled;
   static uint32_t _hwprofilerHotOptLevelThreshold;
   static uint32_t _hwprofilerScorchingOptLevelThreshold;
   static uint32_t _hwprofilerWarmOptLevelThreshold;
   static uint32_t _hwprofilerReducedWarmOptLevelThreshold;
   static uint32_t _hwprofilerAOTWarmOptLevelThreshold;
   static uint32_t _hwprofilerLastOptLevel;
   static uint32_t _hwprofilerRecompilationInterval;
   static uint32_t _hwprofilerRIBufferPoolSize;
   static int32_t  _hwProfilerRIBufferProcessingFrequency;
   static int32_t  _hwProfilerRecompFrequencyThreshold;
   static int32_t  _hwProfilerRecompDecisionWindow;
   static int32_t  _numDowngradesToTurnRION;
   static int32_t  _qszThresholdToTurnRION;
   static int32_t  _qszMaxThresholdToRIDowngrade;
   static int32_t  _qszMinThresholdToRIDowngrade;
   static int32_t  _hwProfilerBufferMaxPercentageToDiscard;

   static uint32_t _hwprofilerPRISamplingRate;

   static uint32_t _hwProfilerExpirationTime;

   static uint32_t _hwprofilerRIBufferThreshold;
   static uint32_t _hwprofilerZRIBufferSize;
   static uint32_t _hwprofilerZRIMode;
   static uint32_t _hwprofilerZRIRGS;
   static uint32_t _hwprofilerZRISF;

   static int32_t _expensiveCompWeight; // weight of a comp request to be considered expensive
   static int32_t _jProfilingEnablementSampleThreshold;

   static void  printPID();






   static char *kcaOffsets(char *option, void *, TR::OptionTable *entry);

   static char *gcOnResolveOption(char *option, void *, TR::OptionTable *entry);

   static char *tprofOption(char *option, void *, TR::OptionTable *entry);

   static char *loadLimitOption(char *option, void *, TR::OptionTable *entry);

   static char *loadLimitfileOption(char *option, void *, TR::OptionTable *entry);

   static char *vmStateOption(char *option, void *, TR::OptionTable *entry);

   static char *setJitConfigRuntimeFlag(char *option, void *base, TR::OptionTable *entry);
   static char *resetJitConfigRuntimeFlag(char *option, void *base, TR::OptionTable *entry);
   static char *setJitConfigNumericValue(char *option, void *base, TR::OptionTable *entry);

   static bool useCompressedPointers();
   static char *limitOption(char *option, void *, TR::OptionTable *entry);
   static char *inlinefileOption(char *option, void *, TR::OptionTable *entry);
   static char *limitfileOption(char *option, void *, TR::OptionTable *entry);
   static char *versionOption(char *option, void *, TR::OptionTable *entry);
   bool  fePreProcess(void *base);
   bool  fePostProcessAOT(void *base);
   bool  fePostProcessJIT(void *base);
   bool  feLatePostProcess(void *base, TR::OptionSet *optionSet);
   bool  showOptionsInEffect();
   bool  showPID();
   void openLogFiles(J9JITConfig *jitConfig);
   };

}

#endif
