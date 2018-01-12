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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef TR_J9_COMPILATIONBASE_INCL
#define TR_J9_COMPILATIONBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TR_J9_COMPILATIONBASE_CONNECTOR
#define TR_J9_COMPILATIONBASE_CONNECTOR
namespace J9 { class Compilation; }
namespace J9 { typedef J9::Compilation CompilationConnector; }
#endif

#include "compile/OMRCompilation.hpp"

#include "compile/CompilationTypes.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/Statistics.hpp"
#include "env/CompilerEnv.hpp"
#include "env/OMRMemory.hpp"
#include "compile/AOTClassInfo.hpp"

class TR_AOTGuardSite;
class TR_FrontEnd;
class TR_ResolvedMethod;
class TR_OptimizationPlan;
class TR_DebugExt;
class TR_J9VMBase;
class TR_ValueProfileInfoManager;
class TR_BranchProfileInfoManager;
class TR_MethodBranchProfileInfo;
class TR_ExternalValueProfileInfo;
class TR_J9VM;
class TR_AccessedProfileInfo;
namespace TR { class IlGenRequest; }

#define COMPILATION_AOT_HAS_INVOKEHANDLE -9
#define COMPILATION_RESERVE_RESOLVED_TRAMPOLINE_FATAL_ERROR -10
#define COMPILATION_RESERVE_RESOLVED_TRAMPOLINE_INSUFFICIENT_SPACE -11
#define COMPILATION_RESERVE_RESOLVED_TRAMPOLINE_ERROR_INBINARYENCODING -12
#define COMPILATION_RESERVE_RESOLVED_TRAMPOLINE_ERROR -13
#define COMPILATION_RESERVE_UNRESOLVED_TRAMPOLINE_FATAL_ERROR -14
#define COMPILATION_RESERVE_UNRESOLVED_TRAMPOLINE_INSUFFICIENT_SPACE -15
#define COMPILATION_RESERVE_UNRESOLVED_TRAMPOLINE_ERROR_INBINARYENCODING -16
#define COMPILATION_RESERVE_UNRESOLVED_TRAMPOLINE_ERROR -17
#define COMPILATION_RESERVE_NTRAMPOLINES_INSUFFICIENT_SPACE -18
#define COMPILATION_RESERVE_NTRAMPOLINES_ERROR_INBINARYENCODING -19
#define COMPILATION_AOT_RELOCATION_FAILED -20



namespace J9
{

class OMR_EXTENSIBLE Compilation : public OMR::CompilationConnector
   {
   friend class ::TR_DebugExt;

   public:

   TR_ALLOC(TR_Memory::Compilation)

   Compilation(
         int32_t compThreadId,
         J9VMThread *j9VMThread,
         TR_FrontEnd *,
         TR_ResolvedMethod *,
         TR::IlGenRequest &,
         TR::Options &,
         TR::Region &heapMemoryRegion,
         TR_Memory *,
         TR_OptimizationPlan *optimizationPlan);

   ~Compilation();

   TR_J9VMBase *fej9();
   TR_J9VM *fej9vm();

   static void allocateCompYieldStatsMatrix();

   static TR_Stats **_compYieldStatsMatrix;

   void updateCompYieldStatistics(TR_CallingContext callingContext);

   bool getUpdateCompYieldStats() { return _updateCompYieldStats; }

   void printCompYieldStats();

   uint64_t getMaxYieldInterval() { return _maxYieldInterval; }

   static const char * getContextName(TR_CallingContext context);

   static void printCompYieldStatsMatrix();

   static void printEntryName(int32_t, int32_t);

   bool getNeedsClassLookahead() {return _needsClassLookahead;}

   void setNeedsClassLookahead(bool b) {_needsClassLookahead = b;}

   bool hasBlockFrequencyInfo();

   bool isShortRunningMethod(int32_t callerIndex);

   int32_t getDltBcIndex() { return (uint32_t)_dltBcIndex;}
   void setDltBcIndex(int32_t ix) { _dltBcIndex=ix;}

   int32_t *getDltSlotDescription() { return _dltSlotDescription;}
   void setDltSlotDescription(int32_t *ds) { _dltSlotDescription = ds;}

   void *getReservedDataCache() { return _reservedDataCache; }
   void setReservedDataCache(void *dataCache) { _reservedDataCache = dataCache; }

   uint32_t getTotalNeededDataCacheSpace() { return _totalNeededDataCacheSpace; }
   void incrementTotalNeededDataCacheSpace(uint32_t size) { _totalNeededDataCacheSpace += size; }

   void * getAotMethodDataStart() const { return _aotMethodDataStart; }
   void setAotMethodDataStart(void *p) { _aotMethodDataStart = p; }

   TR::Node *findNullChkInfo(TR::Node *node);

   bool isAlignStackMaps() { return TR::Compiler->target.cpu.isARM(); }

   void changeOptLevel(TR_Hotness);

   // For replay
   void *getCurMethodMetadata() {return _curMethodMetadata;}
   void setCurMethodMetadata(void *m) {_curMethodMetadata = m;}

   void setGetImplInlineable(bool b) { _getImplInlineable = b; }
   bool getGetImplInlineable() { return _getImplInlineable; }

   //for converters
   bool canTransformConverterMethod(TR::RecognizedMethod method);
   bool isConverterMethod(TR::RecognizedMethod method);

   bool useCompressedPointers();
   bool useAnchors();

   bool isRecompilationEnabled();

   bool isJProfilingCompilation();

   bool pendingPushLivenessDuringIlgen();

   TR::list<TR_ExternalValueProfileInfo*> &getExternalVPInfos() { return _externalVPInfoList; }

   TR_ValueProfileInfoManager *getValueProfileInfoManager()             { return _vpInfoManager;}
   void setValueProfileInfoManager(TR_ValueProfileInfoManager * mgr)    { _vpInfoManager = mgr; }

   TR_BranchProfileInfoManager *getBranchProfileInfoManager()           { return _bpInfoManager;}
   void setBranchProfileInfoManager(TR_BranchProfileInfoManager * mgr)  { _bpInfoManager = mgr; }

   TR::list<TR_MethodBranchProfileInfo*> &getMethodBranchInfos() { return _methodBranchInfoList; }

   // See if the allocation of an object of the class can be inlined.
   bool canAllocateInlineClass(TR_OpaqueClassBlock *clazz);

   // See if it is OK to remove an allocation node to e.g. merge it with others
   // or allocate it locally on a stack frame.
   // If so, return the allocation size. Otherwise return 0.
   // The second argument is the returned class information.
   //
   int32_t canAllocateInlineOnStack(TR::Node* node, TR_OpaqueClassBlock* &classInfo);
   int32_t canAllocateInline(TR::Node* node, TR_OpaqueClassBlock* &classInfo);

   TR::KnownObjectTable *getOrCreateKnownObjectTable();
   void freeKnownObjectTable();

   bool compileRelocatableCode();

   int32_t maxInternalPointers();

   bool compilationShouldBeInterrupted(TR_CallingContext);

   void reportILGeneratorPhase();
   void reportAnalysisPhase(uint8_t id);
   void reportOptimizationPhase(OMR::Optimizations);
   void reportOptimizationPhaseForSnap(OMR::Optimizations);

   CompilationPhase saveCompilationPhase();
   void restoreCompilationPhase(CompilationPhase phase);

   /**
    * \brief
    *    Answers whether the fact that a method has not been executed yet implies
    *    that the method is cold.
    *
    * \return
    *    true if the fact that a method has not been executed implies it is cold;
    *    false otherwise
    */
   bool notYetRunMeansCold();

   // --------------------------------------------------------------------------
   // Hardware profiling
   //
   bool HWProfileDone() { return _doneHWProfile;}
   void setHWProfileDone(bool val) {_doneHWProfile = val;}

   void addHWPInstruction(TR::Instruction *instruction,
                          TR_HWPInstructionInfo::type instructionType,
                          void *data = NULL);
   void addHWPCallInstruction(TR::Instruction *instruction, bool indirectCall = false, TR::Instruction *prev = NULL);
   void addHWPReturnInstruction(TR::Instruction *instruction);
   void addHWPValueProfileInstruction(TR::Instruction *instruction);
   void addHWPBCMap(TR_HWPBytecodePCToIAMap map) { _hwpBCMap.add(map); }
   TR_Array<TR_HWPInstructionInfo> *getHWPInstructions() { return &_hwpInstructions; }
   TR_Array<TR_HWPBytecodePCToIAMap> *getHWPBCMap() { return &_hwpBCMap; }

   bool verifyCompressedRefsAnchors(bool anchorize);
   void verifyCompressedRefsAnchors();

   void verifyCompressedRefsAnchors(TR::Node *parent, TR::Node *node,
                                    TR::TreeTop *tt, vcount_t visitCount);
   void verifyCompressedRefsAnchors(TR::Node *parent, TR::Node *node,
                                    TR::TreeTop *tt, vcount_t visitCount,
                                    TR::list<TR_Pair<TR::Node, TR::TreeTop> *> &nodesList);

   // CodeGenerator?
   TR::list<TR_AOTGuardSite*> *getAOTGuardPatchSites() { return _aotGuardPatchSites; }
   TR_AOTGuardSite *addAOTNOPSite();

   TR::list<TR_VirtualGuardSite*> *getSideEffectGuardPatchSites() { return &_sideEffectGuardPatchSites; }
   TR_VirtualGuardSite *addSideEffectNOPSite();

   TR_CHTable *getCHTable() const { return _transientCHTable; }

   // Inliner
   bool isGeneratedReflectionMethod(TR_ResolvedMethod *method);

   // cache J9 VM pointers
   TR_OpaqueClassBlock *getObjectClassPointer() { return _ObjectClassPointer; }
   TR_OpaqueClassBlock *getRunnableClassPointer() { return _RunnableClassPointer; }
   TR_OpaqueClassBlock *getStringClassPointer() { return _StringClassPointer; }
   TR_OpaqueClassBlock *getSystemClassPointer() { return _SystemClassPointer; }
   TR_OpaqueClassBlock *getReferenceClassPointer() { return _ReferenceClassPointer; }
   TR_OpaqueClassBlock *getJITHelpersClassPointer() { return _JITHelpersClassPointer; }
   TR_OpaqueClassBlock *getClassClassPointer();

   // Monitors
   TR_Array<List<TR::RegisterMappedSymbol> * > & getMonitorAutos() { return _monitorAutos; }
   void addMonitorAuto(TR::RegisterMappedSymbol *, int32_t callerIndex);
   void addAsMonitorAuto(TR::SymbolReference* symRef, bool dontAddIfDLT);
   TR::list<TR::SymbolReference*> * getMonitorAutoSymRefsInCompiledMethod() { return &_monitorAutoSymRefsInCompiledMethod; }

   // OSR Guard Redefinition Classes
   void addClassForOSRRedefinition(TR_OpaqueClassBlock *clazz);
   TR_Array<TR_OpaqueClassBlock*> *getClassesForOSRRedefinition() { return &_classForOSRRedefinition; }

   TR::list<TR::AOTClassInfo*>* _aotClassInfo;

   J9VMThread *j9VMThread() { return _j9VMThread; }

   // cache profile information
   TR_AccessedProfileInfo *getProfileInfo() { return _profileInfo; }

   // Flag to test whether early stages of JProfiling ran
   void setSkippedJProfilingBlock(bool b = true) { _skippedJProfilingBlock = b; }
   bool getSkippedJProfilingBlock() { return _skippedJProfilingBlock; }

   //
   bool supportsQuadOptimization();

private:

   J9VMThread *_j9VMThread;

   bool _doneHWProfile;
   TR_Array<TR_HWPInstructionInfo>    _hwpInstructions;
   TR_Array<TR_HWPBytecodePCToIAMap>  _hwpBCMap;

   bool _updateCompYieldStats;

   uint64_t _hiresTimeForPreviousCallingContext;

   TR_CallingContext _previousCallingContext;

   uint64_t _maxYieldInterval;

   TR_CallingContext _sourceContextForMaxYieldInterval;

   TR_CallingContext _destinationContextForMaxYieldInterval;

   static uint64_t _maxYieldIntervalS;

   static TR_CallingContext _sourceContextForMaxYieldIntervalS;

   static TR_CallingContext _destinationContextForMaxYieldIntervalS;

   bool _needsClassLookahead;

   uint16_t _dltBcIndex;

   int32_t * _dltSlotDescription;

   void * _reservedDataCache;

   uint32_t _totalNeededDataCacheSpace;

   void * _aotMethodDataStart; // used at relocation time

   void * _curMethodMetadata;

   bool _getImplInlineable;

   TR_ValueProfileInfoManager *_vpInfoManager;

   TR_BranchProfileInfoManager *_bpInfoManager;

   TR::list<TR_MethodBranchProfileInfo*> _methodBranchInfoList;
   TR::list<TR_ExternalValueProfileInfo*> _externalVPInfoList;
   TR::list<TR_AOTGuardSite*>*         _aotGuardPatchSites;
   TR::list<TR_VirtualGuardSite*>     _sideEffectGuardPatchSites;

   // cache VM pointers
   TR_OpaqueClassBlock               *_ObjectClassPointer;
   TR_OpaqueClassBlock               *_RunnableClassPointer;
   TR_OpaqueClassBlock               *_StringClassPointer;
   TR_OpaqueClassBlock               *_SystemClassPointer;
   TR_OpaqueClassBlock               *_ReferenceClassPointer;
   TR_OpaqueClassBlock               *_JITHelpersClassPointer;

   TR_Array<List<TR::RegisterMappedSymbol> *> _monitorAutos;
   TR::list<TR::SymbolReference*>             _monitorAutoSymRefsInCompiledMethod;

   TR_Array<TR_OpaqueClassBlock*>       _classForOSRRedefinition;

   // cache profile information
   TR_AccessedProfileInfo *_profileInfo;

   bool _skippedJProfilingBlock;
   };

}

#endif
