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

#ifndef TR_RECOMPILATION_INFO_INCL
#define TR_RECOMPILATION_INFO_INCL


#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CHTable.hpp"
#include "env/TRMemory.hpp"
#include "env/defines.h"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/Timer.hpp"
#include "runtime/J9Profiler.hpp"

class TR_FrontEnd;
class TR_OpaqueMethodBlock;
class TR_OptimizationPlan;
class TR_ResolvedMethod;
namespace TR { class Instruction; }
namespace TR { class SymbolReference; }

// Bits to represent sampling mechanism in method return info field.
// Bits 3, 4, 5, 6, 7 are reserved for this purpose (could use fewer)
// This has to be in sync with LinkageInfo (in Runtime.hpp).
//
#define METHOD_SAMPLING_RECOMPILATION 0x00000010
#define METHOD_COUNTING_RECOMPILATION 0x00000020
#define METHOD_HAS_BEEN_RECOMPILED    0x00000040

// Adaptive Profiling Parameters: chose smaller counts for methods
// with fewer back-edges.
//                                         0    1     2     3    4    >= 5        <-- number of async checks
static int32_t profilingCountsTable[] = { 100, 625, 1250, 2500, 5000, 10000 }; // <-- profiling count
static int32_t profilingFreqTable  [] = {  19,  29,   47,   47,   47,    53 }; // <-- profiling frequency
                                                                               //         (should be prime)
#define MAX_BACKEDGES                                                    (5)   //     max index in the above array

#define DEFAULT_PROFILING_FREQUENCY (profilingFreqTable  [MAX_BACKEDGES])
#define DEFAULT_PROFILING_COUNT     (profilingCountsTable[MAX_BACKEDGES])

namespace TR { class DefaultCompilationStrategy; }
namespace TR { class ThresholdCompilationStrategy; }
namespace OMR { class Recompilation; }
namespace J9 { class Recompilation; }

// Persistent information associated with a method for recompilation
//
class TR_PersistentMethodInfo
   {
   friend class OMR::Recompilation;
   friend class J9::Recompilation;
   friend class TR::CompilationInfo;
   friend class TR_S390Recompilation;  // FIXME: ugly
   friend class ::OMR::Options;
   friend class TR_DebugExt;
   friend class TR::DefaultCompilationStrategy;
   friend class TR::ThresholdCompilationStrategy;

   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentMethodInfo);

   TR_PersistentMethodInfo(TR::Compilation *);
   TR_PersistentMethodInfo(TR_OpaqueMethodBlock *);

   static TR_PersistentMethodInfo * get(TR::Compilation *);
   static TR_PersistentMethodInfo * get(TR_ResolvedMethod * method);
   TR_OpaqueMethodBlock   *         getMethodInfo() {return _methodInfo;}
   void *                           getAddressOfMethodInfo() { return &_methodInfo; }

   void                             setMethodInfo(void *mi) { _methodInfo = (TR_OpaqueMethodBlock *)mi; }


   void setDisableProfiling() { _flags.set(ProfilingDisabled); }
   bool profilingDisabled()    { return _flags.testAny(ProfilingDisabled); }

   void setDisableMiscSamplingCounterDecrementation() { _flags.set(DisableMiscSamplingCounterDecrementation); }
   bool disableMiscSamplingCounterDecrementation() { return _flags.testAny(DisableMiscSamplingCounterDecrementation); }

   void setOptLevelDowngraded(bool b) { _flags.set(OptLevelDowngraded, b); }
   bool isOptLevelDowngraded() { return _flags.testAny(OptLevelDowngraded); }

   void setReasonForRecompilation(int32_t reason) { _flags.setValue(CompilationReasonMask, reason); }
   int32_t getReasonForRecompilation() { return _flags.getValue(CompilationReasonMask); }

   bool hasBeenReplaced() { return _flags.testAny(HasBeenReplaced); }
   void setHasBeenReplaced(bool b=true) { _flags.set(HasBeenReplaced, b); }

   bool wasNeverInterpreted() { return _flags.testAny(WasNeverInterpreted); }
   void setWasNeverInterpreted(bool b) { _flags.set(WasNeverInterpreted, b); }

   bool wasScannedForInlining() { return _flags.testAny(WasScannedForInlining); }
   void setWasScannedForInlining(bool b) { _flags.set(WasScannedForInlining, b); }

   bool isInDataCache() { return _flags.testAny(IsInDataCache); }
   void setIsInDataCache(bool b) { _flags.set(IsInDataCache, b); }

   bool hasFailedDLTCompRetrials() { return _flags.testAny(HasFailedDLTCompRetrials); }
   void setHasFailedDLTCompRetrials(bool b) { _flags.set(HasFailedDLTCompRetrials, b); }

   bool hasRefinedAliasSets() { return _flags.testAny(RefinedAliasesMask); }

   bool doesntKillAddressStatics() { return _flags.testAny(DoesntKillAddressStatics); }
   void setDoesntKillAddressStatics(bool b) { _flags.set(DoesntKillAddressStatics, b); }

   bool doesntKillIntStatics() { return _flags.testAny(DoesntKillIntStatics); }
   void setDoesntKillIntStatics(bool b) { _flags.set(DoesntKillIntStatics, b); }

   bool doesntKillNonIntPrimitiveStatics() { return _flags.testAny(DoesntKillNonIntPrimitiveStatics); }
   void setDoesntKillNonIntPrimitiveStatics(bool b) { _flags.set(DoesntKillNonIntPrimitiveStatics, b); }

   bool doesntKillAddressFields() { return _flags.testAny(DoesntKillAddressFields); }
   void setDoesntKillAddressFields(bool b) { _flags.set(DoesntKillAddressFields, b); }

   bool doesntKillIntFields() { return _flags.testAny(DoesntKillIntFields); }
   void setDoesntKillIntFields(bool b) { _flags.set(DoesntKillIntFields, b); }

   bool doesntKillNonIntPrimitiveFields() { return _flags.testAny(DoesntKillNonIntPrimitiveFields); }
   void setDoesntKillNonIntPrimitiveFields(bool b) { _flags.set(DoesntKillNonIntPrimitiveFields, b); }

   bool doesntKillAddressArrayShadows() { return _flags.testAny(DoesntKillAddressArrayShadows); }
   void setDoesntKillAddressArrayShadows(bool b) { _flags.set(DoesntKillAddressArrayShadows, b); }

   bool doesntKillIntArrayShadows() { return _flags.testAny(DoesntKillIntArrayShadows); }
   void setDoesntKillIntArrayShadows(bool b) { _flags.set(DoesntKillIntArrayShadows, b); }

   bool doesntKillNonIntPrimitiveArrayShadows() { return _flags.testAny(DoesntKillNonIntPrimitiveArrayShadows); }
   void setDoesntKillNonIntPrimitiveArrayShadows(bool b) { _flags.set(DoesntKillNonIntPrimitiveArrayShadows, b); }

   bool doesntKillEverything() { return _flags.testAny(DoesntKillEverything); }
   void setDoesntKillEverything(bool b) { _flags.set(DoesntKillEverything, b); }

   bool doesntKillAnything() { return _flags.testAll(RefinedAliasesMask); }

   // Accessor methods for the "cpoCounter".  This does not really
   // need to be its own counter, as it is conceptually the same as
   // "_counter".  However, the original _counter is still during instrumentation, so
   // it was simplest to keep them separate
   //
   int32_t cpoGetCounter()                {return _cpoSampleCounter;}
   int32_t cpoIncCounter()                {return ++_cpoSampleCounter;}
   int32_t cpoSetCounter(int newCount)    {return _cpoSampleCounter = newCount;}

   uint16_t getTimeStamp() { return _timeStamp; }

   TR_OptimizationPlan * getOptimizationPlan() {return _optimizationPlan;}
   uint8_t getNumberOfInvalidations() {return _numberOfInvalidations;}
   void incrementNumberOfInvalidations() {_numberOfInvalidations++;}
   uint8_t getNumberOfInlinedMethodRedefinition() {return _numberOfInlinedMethodRedefinition;}
   void incrementNumberOfInlinedMethodRedefinition() {_numberOfInlinedMethodRedefinition++;}
   int16_t getNumPrexAssumptions() {return _numPrexAssumptions;}
   void incNumPrexAssumptions() {_numPrexAssumptions++;}

   enum InfoBits
      {
      // Normally set by the previous compilation to indicate that the next
      // compilation should use profiling.  Sometimes we can start out without
      // profiling and then set it during if switched our minds.
      // At the end of compilation we set it to the value we want for the
      // next compilation.
      // If the flag ProfilingDisabled (below) is set we should never set this flag.
      UseProfiling                         = 0x00000001,

      CanBeCalledInSinglePrecisionMode     = 0x00000002,

      // This flag disables any future profiling of this method.
      // Normally we set this when we know that profiling is going to have
      // a large overhead.
      ProfilingDisabled                    = 0x00000004,

      // This flag is used to disable the decrementation of the sampling
      // counter for reasons other than sampling or EDO. Normally it is set in
      // sampleMethod when a recompilation is triggered by PIC misses decrementing
      // the sampling counter or by profiling of PIC addresses at warm.
      DisableMiscSamplingCounterDecrementation=0x00000008,

      // This flag is set when a method is silently downgraded from warm to cold
      OptLevelDowngraded                      = 0x00000010,

      HasFailedDLTCompRetrials             = 0x00000020,

      RefinedAliasesMask                   = 0x0000FFC0,

      DoesntKillAddressStatics             = 0x00000040,
      DoesntKillIntStatics                 = 0x00000080,
      DoesntKillNonIntPrimitiveStatics     = 0x00000100,
      DoesntKillAddressFields              = 0x00000200,
      DoesntKillIntFields                  = 0x00000400,
      DoesntKillNonIntPrimitiveFields      = 0x00000800,
      DoesntKillAddressArrayShadows        = 0x00001000,
      DoesntKillIntArrayShadows            = 0x00002000,
      DoesntKillNonIntPrimitiveArrayShadows= 0x00004000,
      DoesntKillEverything                 = 0x00008000,

      // Define 4 bits to record the reason for recompilation (RAS feature; will be printed in VLOG)
      CompilationReasonMask                = 0x000F0000,
      RecompDueToThreshold                 = 0x00010000,
      RecompDueToCounterZero               = 0x00020000,
      RecompDueToMegamorphicCallProfile    = 0x00030000, // also PIC miss (because we cannot distinguish between the two)
      RecompDueToEdo                       = 0x00040000,
      RecompDueToOptLevelUpgrade           = 0x00050000,
      RecompDueToSecondaryQueue            = 0x00060000,
      RecompDueToRecompilationPushing      = 0x00070000,
      RecompDueToGCR                       = 0x00080000,
      RecompDueToForcedAOTUpgrade          = 0x00090000,
      RecompDueToRI                        = 0x000A0000,
      RecompDueToJProfiling                = 0x000B0000,
      RecompDueToInlinedMethodRedefinition = 0x000C0000,
      // NOTE: recompilations due to EDO decrementation cannot be tracked
      // because they are triggered from a snippet (must change the code for snippet)
      // Also, the recompilations after a profiling step cannot be marked as such.
      // NOTE: recompilations can be triggered by invalidations too, but this
      // information is already available in the linkage info for the body

      HasBeenReplaced                      = 0x00100000, // HCR: this struct is for the old version of a replaced method
                                                       // Note: _methodInfo points to the methodInfo for the new version
                                                       // Note: this flag is accessed from recomp asm code, so be careful about changing it
      WasNeverInterpreted                  = 0x00200000, // for methods that were compiled at count=0
                                                       // Attention: this is not always accurate
      WasScannedForInlining                = 0x00400000, // New scanning for warm method inlining
      IsInDataCache                        = 0x00800000, // This TR_PersistentMethodInfo is stored in the datacache for AOT
      lastFlag                             = 0x80000000
      };

   void       setNextCompileLevel(TR_Hotness level, bool profile)
      {
      _nextHotness = level; if (profile) TR_ASSERT(!profilingDisabled(), "assertion failure");
      _flags.set(UseProfiling, profile);
      }

   TR_Hotness getNextCompileLevel() { return _nextHotness; }
   bool       getNextCompileProfiling() { return _flags.testAny(UseProfiling); }

   /**
    * Methods to update and access profile information. These will modify reference counts.
    * Most accesses to profiling data should go TR_AccessesProfileInfo on TR::Compilation,
    * as it will manage reference counts for a compilation.
    *
    * Several threads may attempt to manipulate reference counts on these at once, potentially
    * resulting in a deallocation before it was intended. The low bit of the relevant pointer
    * is reused to avoid these situations. All accesses to _bestProfileInfo and _recentProfileInfo
    * should consider this.
    */
   TR_PersistentProfileInfo *getBestProfileInfo() { return getForSharedInfo(&_bestProfileInfo); }
   TR_PersistentProfileInfo *getRecentProfileInfo() { return getForSharedInfo(&_recentProfileInfo); }
   void setBestProfileInfo(TR_PersistentProfileInfo * ppi) { setForSharedInfo(&_bestProfileInfo, ppi); }
   void setRecentProfileInfo(TR_PersistentProfileInfo * ppi) { setForSharedInfo(&_recentProfileInfo, ppi); }

   // ### IMPORTANT ###
   // Method info must always be the first field in this structure
   // Flags must always be second
   private:
   TR_OpaqueMethodBlock                  *_methodInfo;
   flags32_t                              _flags;
   // ### IMPORTANT ###

   // During compilation _nextHotness is really the present hotness
   // at which compilation is taking place.  This is setup at the end
   // of compilation to correct hotness level the next compilation should
   // be at.  This may get tweaked by the sampling thread at runtime.
   //
   TR_Hotness                      _nextHotness;


   TR_OptimizationPlan            *_optimizationPlan;

   int32_t                         _cpoSampleCounter; // TODO remove this field
   uint16_t                        _timeStamp;
   uint8_t                         _numberOfInvalidations; // how many times this method has been invalidated
   uint8_t                         _numberOfInlinedMethodRedefinition; // how many times this method triggers recompilation because of its inlined callees being redefined
   int16_t                         _numPrexAssumptions;

   TR_PersistentProfileInfo       *_bestProfileInfo;
   TR_PersistentProfileInfo       *_recentProfileInfo;

   TR_PersistentProfileInfo * getForSharedInfo(TR_PersistentProfileInfo** ptr);
   void setForSharedInfo(TR_PersistentProfileInfo** ptr, TR_PersistentProfileInfo *newInfo);
   };


// This information is kept for every jitted method that can be recompiled
// It may be garbage collected along with the jitted method
// The only way to get the following information is via a pointer that is kept
// in the prologue of the jitted method.
//
class TR_PersistentJittedBodyInfo
   {
   friend class OMR::Recompilation;
   friend class J9::Recompilation;
   friend class TR::CompilationInfo;
   friend class TR_S390Recompilation; // FIXME: ugly
   friend class TR::DefaultCompilationStrategy;
   friend class TR_EmilyPersistentJittedBodyInfo;
   friend class ::OMR::Options;
   friend class J9::Options;
   friend class TR_DebugExt;

#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64)
   friend void fixPersistentMethodInfo(void *table);
#endif

   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentJittedBodyInfo);

   static TR_PersistentJittedBodyInfo *get(void *startPC);

   bool getHasLoops()               { return _flags.testAny(HasLoops); }
   bool getUsesPreexistence()       { return _flags.testAny(UsesPreexistence); }
   bool getDisableSampling()        { return _flags.testAny(DisableSampling);  }
   void setDisableSampling(bool b)  { _flags.set(DisableSampling, b); }
   bool getIsProfilingBody()        { return _flags.testAny(IsProfilingBody); }
   bool getIsAotedBody()            { return _flags.testAny(IsAotedBody); }
   void setIsAotedBody(bool b)      { _flags.set(IsAotedBody, b); }
   bool getSamplingRecomp()         { return _flags.testAny(SamplingRecomp); }
   void setSamplingRecomp()         { _flags.set(SamplingRecomp, true); }
   bool getIsPushedForRecompilation(){ return _flags.testAny(IsPushedForRecompilation); }
   void setIsPushedForRecompilation(){ _flags.set(IsPushedForRecompilation, true); }
   bool getIsInvalidated()          { return _isInvalidated; }
   void setIsInvalidated()          { _isInvalidated = true; }

   bool getFastHotRecompilation()   { return _flags.testAny(FastHotRecompilation); }
   void setFastHotRecompilation(bool b){ _flags.set(FastHotRecompilation, b); }
   bool getFastScorchingRecompilation(){ return _flags.testAny(FastScorchingRecompilation); }
   void setFastScorchingRecompilation(bool b){ _flags.set(FastScorchingRecompilation, b); }
   bool getFastRecompilation()      { return _flags.testAny(FastRecompilationMask); }

   bool getUsesGCR() { return _flags.testAny(UsesGCR); }
   void setUsesGCR() { _flags.set(UsesGCR, true); }

   bool getReducedWarm() { return _flags.testAny(ReducedWarm); }
   void setReducedWarm() { _flags.set(ReducedWarm, true); }

   bool getUsesSamplingJProfiling() { return _flags.testAny(UsesSamplingJProfiling); }
   void setUsesSamplingJProfiling() { _flags.set(UsesSamplingJProfiling, true); }

   bool getUsesJProfiling() { return _flags.testAny(UsesJProfiling); }
   void setUsesJProfiling() { _flags.set(UsesJProfiling, true); }
 
   // used in dump recompilations
   void *getStartPCAfterPreviousCompile() { return _startPCAfterPreviousCompile; }
   void setStartPCAfterPreviousCompile(void *oldStartPC) { _startPCAfterPreviousCompile = oldStartPC; }

   TR_PersistentMethodInfo *getMethodInfo() { return _methodInfo; }
   int32_t    getCounter() const { return _counter; }
   int32_t    getStartCount() const { return _startCount; }
   TR_Hotness getHotness() const { return _hotness; }
   void       setHotness(TR_Hotness h) { _hotness = h; }
   int32_t    getOldStartCount() const { return _startCount - _oldStartCountDelta; } // FIXME: what if this is negative?
   uint16_t   getOldStartCountDelta() const { return _oldStartCountDelta; }
   uint16_t   getHotStartCountDelta() const { return _hotStartCountDelta; }
   void       setHotStartCountDelta(uint16_t v) { _hotStartCountDelta = v; }

   // TODO: can we eliminate this mechanism?
   // FIXME: this should be unsigned
   uint8_t    getAggressiveRecompilationChances() const { return _aggressiveRecompilationChances; }
   uint8_t    decAggressiveRecompilationChances() { return _aggressiveRecompilationChances > 0 ? --_aggressiveRecompilationChances : 0; }

   uint8_t    getNumScorchingIntervals() const { return _numScorchingIntervals; }
   void       incNumScorchingIntervals() { if (_numScorchingIntervals < 255) ++_numScorchingIntervals; }
   void       setMethodInfo(TR_PersistentMethodInfo *mi) { _methodInfo = mi; }
   void       setStartCount(int32_t count) { _startCount = count; }
   void       setCounter(int32_t counter)  { _counter = counter; }
   void       setOldStartCountDelta(uint16_t count) { _oldStartCountDelta = count; }

   void *getMapTable() const { return _mapTable; }
   void setMapTable(void* p) { _mapTable = p; }

   bool       isLongRunningInterpreted() const { return _longRunningInterpreted; }

   /**
    * Access and modify the persistent profile info for this body.
    *
    * Uses of these methods should only occur while the body info is guaranteed
    * to not be cleaned up, such as during its compilation. This is because
    * these calls do not manage reference counts or synchronization, in
    * an attempt to reduce the overhead on accesses that are known to be safe.
    */
   void setProfileInfo(TR_PersistentProfileInfo * ppi) { _profileInfo = ppi; }
   TR_PersistentProfileInfo *getProfileInfo() { return _profileInfo; }

   enum
      {
      HasLoops                = 0x0001,
      //HasManyIterationsLoops  = 0x0002, // Available
      UsesPreexistence        = 0x0004,
      DisableSampling         = 0x0008, // This flag disables sampling of this method even though its recompilable
      IsProfilingBody         = 0x0010,
      IsAotedBody             = 0x0020,
      //IsForcedCompilation     = 0x0040, To be reused
      SamplingRecomp          = 0x0080, // Set when recomp decision is taken due to sampling; used to
                                        // prevent further sampling once a decision is taken
      IsPushedForRecompilation= 0x0100,  // Set when the counter of this method is abruptly decremented to 1
                                        // by the recompilation pushing mechanism
      FastRecompilationMask   = 0x0600, // RAS
      FastHotRecompilation    = 0x0200, // RAS flag
      FastScorchingRecompilation=0x0400,// RAS flag
      UsesGCR                 = 0x0800,
      ReducedWarm             = 0x1000,  // Warm body was optimized to a lesser extent (NoServer) to reduce compilation time
      UsesSamplingJProfiling  = 0x2000,  // Body has samplingJProfiling code
      UsesJProfiling          = 0x4000   // Body has jProfiling code
      };

   // ### IMPORTANT ###
   // These following four fields must always be the first four elements of this structure
   private:
   int32_t                  _counter;             // must be at offset 0
   TR_PersistentMethodInfo *_methodInfo;          // must be at offset 4 (8 for 64bit)
   void                    *_startPCAfterPreviousCompile;
   void                    *_mapTable;            // must be at offset 12 (24 for 64bit)

   // ### IMPORTANT ###

   static TR_PersistentJittedBodyInfo *allocate(TR_PersistentMethodInfo *methodInfo, TR_Hotness hotness, bool profiling, TR::Compilation * comp = 0);
   TR_PersistentJittedBodyInfo(TR_PersistentMethodInfo *methodInfo, TR_Hotness hotness, bool profile, TR::Compilation * comp = 0);

   int32_t *getCounterAddress()      {return &_counter; }
   int32_t decCounter()              { return --_counter; } //FIXME verify implementation

   uint8_t getSampleIntervalCount() { return _sampleIntervalCount; }
   void    setSampleIntervalCount(uint8_t val) { _sampleIntervalCount = val; }
   uint8_t incSampleIntervalCount(uint8_t maxValue)
      {
      if (++_sampleIntervalCount >= maxValue)
         {
         _sampleIntervalCount = 0; // wrap around
         incNumScorchingIntervals();
         }
      return _sampleIntervalCount;
      }


   void setHasLoops(bool b)               { _flags.set(HasLoops, b); }
   void setUsesPreexistence(bool b)       { _flags.set(UsesPreexistence, b); }
   void setIsProfilingBody(bool b)        { _flags.set(IsProfilingBody, b); }

   int32_t                  _startCount; // number of global samples at the beginning of the sampling window
   uint16_t                 _hotStartCountDelta; // delta from the startCount (for the begin of a hot sampling window)
   uint16_t                 _oldStartCountDelta; // delta from the current start count (in the past);
   flags16_t                _flags;
   uint8_t                  _sampleIntervalCount; // increases from 0 to 29. Defines a hot sampling window (30 samples)
   uint8_t                  _aggressiveRecompilationChances;
   TR_Hotness               _hotness;
   uint8_t                  _numScorchingIntervals; // How many times we reached scorching recompilation decision points
   bool                     _isInvalidated;
   bool                     _longRunningInterpreted; // This cannot be moved into _flags due to synchronization issues
   TR_PersistentProfileInfo * _profileInfo;
   public:
   // Used for HWP-based recompilation
   bool                     _hwpInducedRecompilation;
   bool                     _hwpReducedWarmCompileRequested;
   bool                     _hwpReducedWarmCompileInQueue;
   uint64_t                 _hwpInstructionStartCount;
   uint32_t                 _hwpInstructionCount;
   };

#endif
