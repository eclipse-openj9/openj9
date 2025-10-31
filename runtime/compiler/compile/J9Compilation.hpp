/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "infra/set.hpp"
#include "infra/Statistics.hpp"
#include "infra/vector.hpp"
#include "env/CompilerEnv.hpp"
#include "env/OMRMemory.hpp"
#include "compile/AOTClassInfo.hpp"
#include "runtime/SymbolValidationManager.hpp"
#include "env/PersistentCollections.hpp"

namespace J9 { class RetainedMethodSet; }
namespace J9 { class ConstProvenanceGraph; }
class TR_AOTGuardSite;
class TR_FrontEnd;
class TR_ResolvedMethod;
class TR_OptimizationPlan;
class TR_J9VMBase;
class TR_ValueProfileInfoManager;
class TR_BranchProfileInfoManager;
class TR_MethodBranchProfileInfo;
class TR_ExternalValueProfileInfo;
class TR_J9VM;
class TR_AccessedProfileInfo;
class TR_RelocationRuntime;
namespace TR { class IlGenRequest; }
#ifdef J9VM_OPT_JITSERVER
struct SerializedRuntimeAssumption;
class ClientSessionData;
class AOTCacheRecord;
class AOTCacheClassRecord;
class AOTCacheThunkRecord;
#endif /* defined(J9VM_OPT_JITSERVER) */

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
         TR_OptimizationPlan *optimizationPlan,
         TR_RelocationRuntime *reloRuntime,
         TR::Environment *target
#if defined(J9VM_OPT_JITSERVER)
         , size_t numPermanentLoaders
#endif
         );

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

   int32_t getDltBcIndex() { return _dltBcIndex;}
   void setDltBcIndex(int32_t ix) { _dltBcIndex=ix;}

   int32_t *getDltSlotDescription() { return _dltSlotDescription;}
   void setDltSlotDescription(int32_t *ds) { _dltSlotDescription = ds;}

   void *getReservedDataCache() { return _reservedDataCache; }
   void setReservedDataCache(void *dataCache) { _reservedDataCache = dataCache; }

   uint32_t getTotalNeededDataCacheSpace() { return _totalNeededDataCacheSpace; }
   void incrementTotalNeededDataCacheSpace(uint32_t size) { _totalNeededDataCacheSpace += size; }

   void * getAotMethodDataStart() const { return _aotMethodDataStart; }
   void setAotMethodDataStart(void *p) { _aotMethodDataStart = p; }

   TR_AOTMethodHeader * getAotMethodHeaderEntry();

   TR::Node *findNullChkInfo(TR::Node *node);

   bool isAlignStackMaps() { return J9::Compilation::target().cpu.isARM(); }

   void changeOptLevel(TR_Hotness);

   // For replay
   void *getCurMethodMetadata() {return _curMethodMetadata;}
   void setCurMethodMetadata(void *m) {_curMethodMetadata = m;}

   void setGetImplInlineable(bool b) { setGetImplAndRefersToInlineable(b); }
   /**
    * \brief Sets whether the native \c java.lang.ref.Reference
    * methods, \c getImpl and \c refersTo, can be inlined.
    * \param[in] b A \c bool argument indicating whether \c getImpl
    *              and \c refersTo can be inlined.
    */
   void setGetImplAndRefersToInlineable(bool b) { _getImplAndRefersToInlineable = b; }

   /**
    * \brief Indicates whether the native \c java.lang.ref.Reference
    * methods, \c getImpl and \c refersTo, can be inlined.
    *
    * \return \c true if they can be inlined; \c false otherwise
    */
   bool getGetImplAndRefersToInlineable() { return _getImplAndRefersToInlineable; }

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

   bool compilePortableCode();

   int32_t maxInternalPointers();

   bool compilationShouldBeInterrupted(TR_CallingContext);

   /* Heuristic Region APIs
    *
    * Heuristic Regions denotes regions where decisions
    * within the region do not need to be remembered. In relocatable compiles,
    * when the compiler requests some information via front end query,
    * it's possible that the front end might walk a data structure,
    * looking at several different possible answers before finally deciding
    * on one. For a relocatable compile, only the final answer is important.
    * Thus, a heuristic region is used to ignore all of the intermediate
    * steps in determining the final answer.
    */
   void enterHeuristicRegion();
   void exitHeuristicRegion();

   /* Used to ensure that a implementer chosen for inlining is valid under
    * AOT.
    */
   bool validateTargetToBeInlined(TR_ResolvedMethod *implementer);

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
   using OMR::CompilationConnector::incInlineDepth;
   bool incInlineDepth(TR::ResolvedMethodSymbol *, TR_ByteCodeInfo &, int32_t cpIndex, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo = 0);

   bool isGeneratedReflectionMethod(TR_ResolvedMethod *method);

   TR_ExternalRelocationTargetKind getReloTypeForMethodToBeInlined(TR_VirtualGuardSelection *guard, TR::Node *callNode, TR_OpaqueClassBlock *receiverClass);

   // cache J9 VM pointers
   TR_OpaqueClassBlock *getObjectClassPointer();
   TR_OpaqueClassBlock *getRunnableClassPointer();
   TR_OpaqueClassBlock *getStringClassPointer();
   TR_OpaqueClassBlock *getSystemClassPointer();
   TR_OpaqueClassBlock *getReferenceClassPointer();
   TR_OpaqueClassBlock *getJITHelpersClassPointer();
   TR_OpaqueClassBlock *getClassClassPointer(bool isVettedForAOT = false);

   // Monitors
   TR_Array<List<TR::RegisterMappedSymbol> * > & getMonitorAutos() { return _monitorAutos; }
   void addMonitorAuto(TR::RegisterMappedSymbol *, int32_t callerIndex);
   void addAsMonitorAuto(TR::SymbolReference* symRef, bool dontAddIfDLT);
   TR::list<TR::SymbolReference*> * getMonitorAutoSymRefsInCompiledMethod() { return &_monitorAutoSymRefsInCompiledMethod; }

   // OSR Guard Redefinition Classes
   void addClassForOSRRedefinition(TR_OpaqueClassBlock *clazz);
   TR_Array<TR_OpaqueClassBlock*> *getClassesForOSRRedefinition() { return &_classForOSRRedefinition; }

   void addClassForStaticFinalFieldModification(TR_OpaqueClassBlock *clazz);
   TR_Array<TR_OpaqueClassBlock*> *getClassesForStaticFinalFieldModification() { return &_classForStaticFinalFieldModification; }

   TR::list<TR::AOTClassInfo*>* _aotClassInfo;

   J9VMThread *j9VMThread() { return _j9VMThread; }

   // cache profile information
   TR_AccessedProfileInfo *getProfileInfo() { return _profileInfo; }

   // Flag to test whether early stages of JProfiling ran
   void setSkippedJProfilingBlock(bool b = true) { _skippedJProfilingBlock = b; }
   bool getSkippedJProfilingBlock() { return _skippedJProfilingBlock; }

   //
   bool supportsQuadOptimization();

   TR_RelocationRuntime *reloRuntime() { return _reloRuntime; }

   bool incompleteOptimizerSupportForReadWriteBarriers();

#if defined(J9VM_OPT_JITSERVER)
   static bool isOutOfProcessCompilation() { return _outOfProcessCompilation; } // server side
   static void setOutOfProcessCompilation() { _outOfProcessCompilation = true; }

   bool isRemoteCompilation() const { return _remoteCompilation; } // client side
   void setRemoteCompilation() { _remoteCompilation = true; }

   TR::list<SerializedRuntimeAssumption *> &getSerializedRuntimeAssumptions() { return _serializedRuntimeAssumptions; }

   ClientSessionData *getClientData() const { return _clientData; }
   void setClientData(ClientSessionData *clientData) { _clientData = clientData; }

   JITServer::ServerStream *getStream() const { return _stream; }
   void setStream(JITServer::ServerStream *stream) { _stream = stream; }

   void switchToPerClientMemory() { _trMemory = _perClientMemory; }
   void switchToGlobalMemory() { _trMemory = &_globalMemory; }

   TR::list<TR_OpaqueMethodBlock *> &getMethodsRequiringTrampolines() { return _methodsRequiringTrampolines; }

   bool isDeserializedAOTMethod() const { return _deserializedAOTMethod; }
   void setDeserializedAOTMethod(bool deserialized) { _deserializedAOTMethod = deserialized; }

   bool isDeserializedAOTMethodStore() const { return _deserializedAOTMethodStore; }
   void setDeserializedAOTMethodStore(bool deserializedStore) { _deserializedAOTMethodStore = deserializedStore; }

   bool isDeserializedAOTMethodUsingSVM() const { return _deserializedAOTMethodUsingSVM; }
   void setDeserializedAOTMethodUsingSVM(bool usingSVM) { _deserializedAOTMethodUsingSVM = usingSVM; }

   bool isAOTCacheStore() const { return _aotCacheStore; }
   void setAOTCacheStore(bool store) { _aotCacheStore = store; }

   bool ignoringLocalSCC() const { return _ignoringLocalSCC; }
   void setIgnoringLocalSCC(bool ignoringLocalSCC) { _ignoringLocalSCC = ignoringLocalSCC; }

   Vector<std::pair<const AOTCacheRecord *, uintptr_t>> &getSerializationRecords() { return _serializationRecords; }
   // Adds an AOT cache record and the corresponding offset into AOT relocation data to the list that
   // will be used when the result of this out-of-process compilation is serialized and stored in
   // JITServer AOT cache. If record is NULL, fails serialization by setting _aotCacheStore to false if we are not
   // ignoring the client's SCC, and otherwise fails the compilation entirely.
   void addSerializationRecord(const AOTCacheRecord *record, uintptr_t reloDataOffset);

   UnorderedSet<const AOTCacheThunkRecord *> &getThunkRecords() { return _thunkRecords; }
   // Adds an AOT cache thunk record to the set of thunks that this compilation depends on, and also adds it
   // to the list of records that this compilation depends on if the thunk record is new. If the record is NULL,
   // fails serialization by setting _aotCacheStore to false if we are not ignoring the client's SCC, and otherwise
   // fails the compilation entirely.
   void addThunkRecord(const AOTCacheThunkRecord *record);
#else
   bool isDeserializedAOTMethod() const { return false; }
   bool ignoringLocalSCC() const { return false; }
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR::SymbolValidationManager *getSymbolValidationManager() { return _symbolValidationManager; }

   // overrides OMR::Compilation::createRetainedMethods(TR_ResolvedMethod*)
   OMR::RetainedMethodSet *createRetainedMethods(TR_ResolvedMethod *method);

   /**
    * \brief Determine whether retained methods need to be tracked.
    *
    * If they do, then J9::RetainedMethodSet will be used. Otherwise, the base
    * OMR::RetainedMethodSet will be used instead, which does no tracking.
    *
    * \return true if tracking is needed, false otherwise
    */
   bool mustTrackRetainedMethods();

   // overrides OMR::Compilation::bondMethodsTraceNote().
   const char *bondMethodsTraceNote();

   /**
    * \brief Get the set of classes to keep alive.
    *
    * During optimization, these are classes that should be kept alive due to
    * IL transformations, as opposed to the keepalives in retainedMethods(),
    * which are due to inlining. They are kept separately because the API of
    * OMR::RetainedMethodSet is designed to avoid assuming that unloading
    * proceeds at any granularity coarser than per-method.
    *
    * \return the set of classes to keep alive
    */
   const TR::set<TR_OpaqueClassBlock*> &keepaliveClasses() { return _keepaliveClasses; }

   /**
    * \brief Add a keepalive class.
    *
    * This is only for cases where an IL transformation based on known objects
    * causes the IL to directly use a class (i.e. with loadaddr) or one of
    * its members, when it didn't previously. After such a transformation, the
    * known objects could end up being unused, in which case they wouldn't
    * guarantee on their own that the class remains loaded at the point of use.
    *
    * If the IL is modified to use a member that will necessarily remain loaded
    * at the point of use anyway, e.g. an instance method, then no keepalive is
    * needed.
    *
    * \param c the class to keep alive
    */
   void addKeepaliveClass(TR_OpaqueClassBlock *c);

   J9::ConstProvenanceGraph *constProvenanceGraph()
      {
      return _constProvenanceGraph;
      }

   /**
    * \brief Determine whether it's currently expected to be possible to add
    * OSR assumptions and corresponding fear points somewhere in the method.
    *
    * The result is independent of any particular program point. Even if the
    * result is true, there may still be restrictions on the placement of fear
    * points. However, if the result is false, then no fear points can be
    * placed and no new assumptions can be made.
    *
    * \param comp the compilation object
    * \return true if it's possible in general to add assumptions, false otherwise
    */
   bool canAddOSRAssumptions();

   /**
    * \brief Determine whether fear points may be placed (almost) anywhere.
    *
    * If the result is true, then prior to fear point analysis, the compiler
    * must ensure that OSR induction remains possible at every OSR yield point.
    * As such, fear points may be placed almost anywhere in the method.
    *
    * \warning This does not allow fear points to be placed on the taken side
    * of a guard (except after an OSR yield point, e.g. a cold call). That
    * restriction is due to a limitation of the fear point analysis.
    *
    * \return true if fear points may be placed (almost) anywhere
    */
   bool isFearPointPlacementUnrestricted() { return false; }

   // Flag to record whether fear-point analysis has already been done.
   void setFearPointAnalysisDone() { _wasFearPointAnalysisDone = true; }
   bool wasFearPointAnalysisDone() { return _wasFearPointAnalysisDone; }

   // Flag to record if any optimization has prohibited OSR over a range of trees
   void setOSRProhibitedOverRangeOfTrees() { _osrProhibitedOverRangeOfTrees = true; }
   bool isOSRProhibitedOverRangeOfTrees() { return _osrProhibitedOverRangeOfTrees; }

#if defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)
   void addAOTMethodDependency(TR_OpaqueClassBlock *ramClass, uintptr_t chainOffse = TR_SharedCache::INVALID_CLASS_CHAIN_OFFSETt) {}
#else
   /**
    * \brief Add the provided class as an AOT Method Dependency
    *
    * In the case of JITServer with AOTCache, this method calls the private
    * helper method addAOTMethodDependency with the associated
    * AOTCacheClassRecord; otherwise, it passes the class chain offset. Failure
    * to acquire the appropriate data in both cases results in a compilation
    * failure.
    *
    * \param ramClass the J9Class which is a dependncy of the the currently
    *                 method being compiled.
    * \param chainOffset an optional parameter, in case the the caller already
    *                    has the chain offset on hand.
    *
    */
   void addAOTMethodDependency(TR_OpaqueClassBlock *ramClass, uintptr_t chainOffset = TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET);

   /**
    * \brief Populated the provided buffer with position independent AOT Method
    *        Dependencies.
    *
    * \param definingClass the defining class of the method being compiled.
    * \param chainBuffer[out] the buffer to which the position independent
    *                         dependencies will be stored.
    *
    * \return The number of AOT Method Dependencies
    */
   uintptr_t populateAOTMethodDependencies(TR_OpaqueClassBlock *definingClass, Vector<uintptr_t> &chainBuffer);

   /**
    * \brief Return the number of dependencies for the method being compiled.
    *
    * \return The number of dependencies for the method being compiled.
    */
   uintptr_t numAOTMethodDependencies() { return _aotMethodDependencies.size(); }

   /**
    * \brief Return a reference to the _aotMethodDependencies map
    *
    * \return reference to the _aotMethodDependencies map
    */
   UnorderedMap<uintptr_t, bool> & getAOTMethodDependencies() { return _aotMethodDependencies; }
#endif

   /**
    * \brief Get the class loaders that are known to be permanent.
    * \return a vector of pointers to all known-permanent class loaders
    */
   const TR::vector<J9ClassLoader*, TR::Region&> &permanentLoaders();

private:
   enum CachedClassPointerId
      {
      OBJECT_CLASS_POINTER,
      RUNNABLE_CLASS_POINTER,
      STRING_CLASS_POINTER,
      SYSTEM_CLASS_POINTER,
      REFERENCE_CLASS_POINTER,
      JITHELPERS_CLASS_POINTER,
      CACHED_CLASS_POINTER_COUNT,
      };

   TR_OpaqueClassBlock *getCachedClassPointer(CachedClassPointerId which);

#if !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)
   /**
    * \brief Helper method inserts the dependency into _aotMethodDependencies
    *
    * \param dependency the AOT method dependency. This could either be an
    *                   offset into the SCC or a AOTCacheClassRecord pointer.
    * \param classIsInitialized boolean to indicate whether the class that the
    *                           dependency is associated with has been
    *                           initialized.
    */
   void insertAOTMethodDependency(uintptr_t dependency, bool classIsInitialized);

   /**
    * \brief Heleper method to store a dependency represented by an offset into
    *        the SCC.
    *
    * \param offset the offset into the SCC of the class associated with the
    *               dependency.
    * \param classIsInitialized boolean to indicate wiether the class that the
    *                           dependency is associated with has been
    *                           initialized.
    */
   void addAOTMethodDependency(uintptr_t offset, bool classIsInitialized);
#if defined(J9VM_OPT_JITSERVER)
   /**
    * \brief Helper method to store a dependency represented by a
    *        AOTCacheClassRecord.
    *
    * \param record The AOTCacheClassRecord pointer associated with the class
    *               associated with the dependency.
    * \param classIsInitialized boolean to indicate wiether the class that the
    *                           dependency is associated with has been
    *                           initialized.
    */
   void addAOTMethodDependency(const AOTCacheClassRecord *record, bool classIsInitialized);
#endif
#endif  /*  !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */

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

   int32_t _dltBcIndex;

   int32_t * _dltSlotDescription;

   void * _reservedDataCache;

   uint32_t _totalNeededDataCacheSpace;

   void * _aotMethodDataStart; // used at relocation time

   void * _curMethodMetadata;

   bool _getImplAndRefersToInlineable;

   TR_ValueProfileInfoManager *_vpInfoManager;

   TR_BranchProfileInfoManager *_bpInfoManager;

   TR::list<TR_MethodBranchProfileInfo*> _methodBranchInfoList;
   TR::list<TR_ExternalValueProfileInfo*> _externalVPInfoList;
   TR::list<TR_AOTGuardSite*>*         _aotGuardPatchSites;
   TR::list<TR_VirtualGuardSite*>     _sideEffectGuardPatchSites;

   // cache VM pointers
   TR_OpaqueClassBlock               *_cachedClassPointers[CACHED_CLASS_POINTER_COUNT];

   TR_OpaqueClassBlock               *_aotClassClassPointer;
   bool                               _aotClassClassPointerInitialized;

   TR_Array<List<TR::RegisterMappedSymbol> *> _monitorAutos;
   TR::list<TR::SymbolReference*>             _monitorAutoSymRefsInCompiledMethod;

   TR::set<TR_OpaqueClassBlock*>        _keepaliveClasses;

   TR_Array<TR_OpaqueClassBlock*>       _classForOSRRedefinition;
   // Classes that have their static final fields folded and need assumptions
   TR_Array<TR_OpaqueClassBlock*>       _classForStaticFinalFieldModification;

   // cache profile information
   TR_AccessedProfileInfo *_profileInfo;

   bool _skippedJProfilingBlock;

   TR_RelocationRuntime *_reloRuntime;

#if defined(J9VM_OPT_JITSERVER)
   // This list contains assumptions created during the compilation at the JITServer
   // It needs to be sent to the client at the end of compilation
   TR::list<SerializedRuntimeAssumption *> _serializedRuntimeAssumptions;
   // The following flag is set when this compilation is performed in a
   // VM that does not have the runtime part (server side in JITServer)
   static bool _outOfProcessCompilation;
   // The following flag is set when a request to complete this compilation
   // has been sent to a remote VM (client side in JITServer)
   bool _remoteCompilation;
   // Client session data for the client that requested this out-of-process
   // compilation (at the JITServer); unused (always NULL) at the client side
   ClientSessionData *_clientData;
   // Server stream used by this out-of-process compilation; always NULL at the client
   JITServer::ServerStream *_stream;

   TR_Memory *_perClientMemory;
   TR_Memory _globalMemory;
   // This list contains RAM method pointers of resolved methods
   // that require method trampolines.
   // It needs to be sent to the client at the end of compilation
   // so that trampolines can be reserved there.
   TR::list<TR_OpaqueMethodBlock *> _methodsRequiringTrampolines;

   // True if this remote compilation resulted in deserializing an AOT method
   // received from the JITServer AOT cache; always false at the server
   bool _deserializedAOTMethod;
   // True if this remote compilation resulted in deserializing an AOT method
   // that was compiled as an AOT cache store; always false at the server
   bool _deserializedAOTMethodStore;
   // True if this deserialized AOT method received from the
   // JITServer AOT cache uses SVM; always false at the server
   bool _deserializedAOTMethodUsingSVM;
   // True if the result of this out-of-process compilation will be
   // stored in JITServer AOT cache; always false at the client
   bool _aotCacheStore;
   // True at the client if the compilation is to be stored in the AOT cache at the server and the
   // client is ignoring the local SCC; always false at the server
   bool _ignoringLocalSCC;
   // List of AOT cache records and corresponding offsets into AOT relocation data that will
   // be used to store the result of this compilation in AOT cache; always empty at the client
   Vector<std::pair<const AOTCacheRecord *, uintptr_t>> _serializationRecords;
   // Set of AOT cache thunk records that this compilation depends on; always empty at the client
   UnorderedSet<const AOTCacheThunkRecord *> _thunkRecords;
   // For the server, the number of permanent loaders the client has specified
   // we must use for this compilation.
   size_t _numPermanentLoaders;
#endif /* defined(J9VM_OPT_JITSERVER) */

#if !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)
   // A map recording the dependencies of an AOT method. The keys are the class
   // chain offsets of classes this method depends on, and the values record
   // whether the class needs to be initialized before method loading, or only
   // loaded.
   UnorderedMap<uintptr_t, bool> _aotMethodDependencies;
#endif /* defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */

   TR::SymbolValidationManager *_symbolValidationManager;
   TR::vector<J9ClassLoader*, TR::Region&> _permanentLoaders;
   ConstProvenanceGraph *_constProvenanceGraph;
   bool _osrProhibitedOverRangeOfTrees;
   bool _wasFearPointAnalysisDone;
   bool _permanentLoadersInitialized;
   };

}

#endif
