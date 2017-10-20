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

#ifndef VMJ9_h
#define VMJ9_h

#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9cp.h"
#include "j9cfg.h"
#include "rommeth.h"
#include "codegen/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/OptionsUtil.hpp"
#include "env/ExceptionTable.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Annotations.hpp"
#include "optimizer/OptimizationStrategies.hpp"
#include "trj9/env/J9SharedCache.hpp"
#include "runtime/J9ValueProfiler.hpp"
#include "infra/Array.hpp"
#include "env/CompilerEnv.hpp"

class TR_CallStack;
namespace TR { class CompilationInfo; }
namespace TR { class CompilationInfoPerThread; }
class TR_IProfiler;
class TR_HWProfiler;
class TR_LMGuardedStorage;
class TR_Debug;
class TR_OptimizationPlan;
class TR_ValueProfileInfo;
class TR_AbstractInfo;
class TR_ExternalProfiler;
class TR_JitPrivateConfig;
class TR_DataCacheManager;
class TR_EstimateCodeSize;
struct TR_CallSite;
struct TR_CallTarget;
namespace J9 { class ObjectModel; }
namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }
namespace TR { class Compilation; }
namespace TR { class StaticSymbol; }
namespace TR { class ParameterSymbol; }

char * feGetEnv2(const char *, const void *);   /* second param should be a J9JavaVM* */

void acquireVPMutex();
void releaseVPMutex();

bool acquireVMaccessIfNeeded(J9VMThread *vmThread, TR_YesNoMaybe isCompThread);
void releaseVMaccessIfNeeded(J9VMThread *vmThread, bool haveAcquiredVMAccess);

// To be given official J9VMThread fields in the future.  For now, re-using the
// debug fields should suffice.
//
// All fields below are used to EXPERIMENT with the phase profiling infrastructure
// at this point.
//
// <PHASE PROFILER>
#define J9VMTHREAD_HOOK_FIELD                      debugEventData7
#define J9VMTHREAD_UNHOOK_FIELD                    debugEventData8
#define J9VMTHREAD_PHASE_PROFILING_ACTION_FIELD    debugEventData2

#define J9VMTHREAD_TRACINGBUFFER_SIZE_FIELD        debugEventData6
#define J9VMTHREAD_TRACINGBUFFER_CURSOR_FIELD      debugEventData5
#define J9VMTHREAD_TRACINGBUFFER_TOP_FIELD         debugEventData4
#define J9VMTHREAD_TRACINGBUFFER_FH_FIELD          debugEventData3

#define VMTHREAD_SOM_HOOK(x)                       (x->J9VMTHREAD_HOOK_FIELD)
#define VMTHREAD_SOM_UNHOOK(x)                     (x->J9VMTHREAD_UNHOOK_FIELD)
#define VMTHREAD_PHASE_PROFILING_ACTION(x)         (x->J9VMTHREAD_PHASE_PROFILING_ACTION_FIELD)

#define VMTHREAD_TRACINGBUFFER_FH(x)               (x->J9VMTHREAD_TRACINGBUFFER_FH_FIELD)
#define VMTHREAD_TRACINGBUFFER_SIZE(x)             (x->J9VMTHREAD_TRACINGBUFFER_SIZE_FIELD)
#define VMTHREAD_TRACINGBUFFER_TOP(x)              (x->J9VMTHREAD_TRACINGBUFFER_TOP_FIELD)
#define VMTHREAD_TRACINGBUFFER_CURSOR(x)           (x->J9VMTHREAD_TRACINGBUFFER_CURSOR_FIELD)

#define VMTHREAD_TRACINGBUFFER_START_MARKER        -1

#define J9_PUBLIC_FLAGS_PATCH_HOOKS_IN_JITTED_METHODS 0x8000000
// </PHASE PROFILER>

#define J9VMTHREAD_TLH_PREFETCH_COUNT              tlhPrefetchFTA

inline void getClassNameSignatureFromMethod(J9Method *method, J9UTF8 *& methodClazz, J9UTF8 *& methodName, J9UTF8 *& methodSignature)
   {
   methodClazz = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
   methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
   methodSignature = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
   }


typedef struct TR_JitPrivateConfig
   {
   TR::FILE      *vLogFile;
   char          *vLogFileName;
   uint64_t       verboseFlags;
   TR::FILE      *rtLogFile;
   char          *rtLogFileName;
   char          *itraceFileNamePrefix;
   TR_IProfiler  *iProfiler;
   TR_HWProfiler *hwProfiler;
   TR_LMGuardedStorage *lmGuardedStorage;
   TR::CodeCacheManager *codeCacheManager; // reachable from JitPrivateConfig for kca's benefit
   TR_DataCacheManager *dcManager;  // reachable from JitPrivateConfig for kca's benefit
   bool          annotationClassesAlreadyLoaded;
   TR_YesNoMaybe aotValidHeader;
   void          (*j9jitrt_lock_log)(void *voidConfig);
   void          (*j9jitrt_unlock_log)(void *voidConfig);
   int           (*j9jitrt_printf)(void *voidConfig, char *format, ...) ;

   // Runtime phase profiling buffer
   //
   int32_t        maxRuntimeTraceBufferSizeInBytes;
   int32_t        maxTraceBufferEntrySizeInBytes;
   TR_AOTStats *aotStats;
   } TR_JitPrivateConfig;


#ifdef __cplusplus
extern "C" {
#endif

   J9VMThread * getJ9VMThreadFromTR_VM(void * vm);
   J9JITConfig * getJ9JitConfigFromFE(void *vm);
   TR::FILE *j9jit_fopen(char *fileName, const char *mode, bool useJ9IO, bool encrypt);
   void j9jit_fclose(TR::FILE *pFile);
   void j9jit_seek(void *voidConfig, TR::FILE *pFile, IDATA offset, I_32 whence);
   IDATA j9jit_read(void *voidConfig, TR::FILE *pFile, void *buf, IDATA nbytes);
   void j9jit_fflush(TR::FILE *pFile);
   void j9jit_lock_vlog(void *voidConfig);
   void j9jit_unlock_vlog(void *voidConfig);
   void j9jit_printf(void *voidConfig, char *format, ...);
   I_32 j9jit_vprintf(void *voidConfig, char *format, va_list args);
   I_32 j9jit_fprintf(TR::FILE *pFile, char *format, ...);
   I_32 j9jit_vfprintf(TR::FILE *pFile, char *format, va_list args);

   void j9jitrt_lock_log(void *voidConfig);
   void j9jitrt_unlock_log(void *voidConfig);
   I_32 j9jitrt_printf(void *voidConfig, char *format, ...);

   I_32 j9jit_fopenName(char *fileName);
   I_32 j9jit_fopen_existing(char *fileName);
   I_32 j9jit_fmove(char * pathExist, char * pathNew);
   void j9jit_fcloseId(I_32 fileId);
   I_32 j9jit_fread(I_32 fd, void * buf, IDATA nbytes);
   I_32 j9jit_fseek(I_32 fd, I_32 whence);
   I_64 j9jit_time_current_time_millis();
   I_32 j9jit_vfprintfId(I_32 fileId, char *format, ...);
   I_32 j9jit_fprintfId(I_32 fileId, char *format, ...);

#ifdef __cplusplus
}
#endif


TR_Processor      portLibCall_getProcessorType();
TR_Processor      portLibCall_getPhysicalProcessorType();
TR_Processor      mapJ9Processor(J9ProcessorArchitecture j9processor);
static TR_Processor portLibCall_getARMProcessorType();
static TR_Processor portLibCall_getX86ProcessorType();
static bool portLibCall_sysinfo_has_resumable_trap_handler();
static bool portLibCall_sysinfo_has_fixed_frame_C_calling_convention();
static bool portLibCall_sysinfo_has_floating_point_unit();
static uint32_t portLibCall_sysinfo_number_bytes_read_inaccessible();
static uint32_t portLibCall_sysinfo_number_bytes_write_inaccessible();
static bool portLibCall_sysinfo_supports_scaled_index_addressing();

TR::CompilationInfo *getCompilationInfo(J9JITConfig *jitConfig);

class TR_J9VMBase : public TR_FrontEnd
   {
public:
   void * operator new (size_t, void * placement) { return placement; }
   TR_PERSISTENT_ALLOC(TR_Memory::FrontEnd)

   enum VM_TYPE
      {
      DEFAULT_VM = 0
      , J9_VM
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
      , AOT_VM
#endif
      };

   TR_J9VMBase(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, J9VMThread * vmContext);
   virtual ~TR_J9VMBase() {}

   virtual bool isAOT_DEPRECATED_DO_NOT_USE() { return false; }
   virtual bool supportsMethodEntryPadding() { return true; }

#if defined(TR_TARGET_S390)
   virtual void initializeS390zLinuxProcessorFeatures();
   virtual void initializeS390zOSProcessorFeatures();
#endif

/////
   // Inlining optimization
   //
   virtual void setInlineThresholds ( TR::Compilation *comp, int32_t &callerWeightLimit, int32_t &maxRecursiveCallByteCodeSizeEstimate, int32_t &methodByteCodeSizeThreshold, int32_t &methodInWarmBlockByteCodeSizeThreshold, int32_t &methodInColdBlockByteCodeSizeThreshold, int32_t &nodeCountThreshold, int32_t size);
   virtual int32_t adjustedInliningWeightBasedOnArgument(int32_t origWeight, TR::Node *argNode, TR::ParameterSymbol *parmSymbol,  TR::Compilation *comp);
   virtual bool canAllowDifferingNumberOrTypesOfArgsAndParmsInInliner();
/////
   virtual bool isGetImplInliningSupported();


   virtual bool isAbstractClass(TR_OpaqueClassBlock * clazzPointer);
   virtual bool isCloneable(TR_OpaqueClassBlock *);
   virtual bool isInterfaceClass(TR_OpaqueClassBlock * clazzPointer);
   virtual bool isPrimitiveClass(TR_OpaqueClassBlock *clazz);
   virtual bool isPrimitiveArray(TR_OpaqueClassBlock *);
   virtual bool isReferenceArray(TR_OpaqueClassBlock *);
   virtual bool hasFinalizer(TR_OpaqueClassBlock * classPointer);
   virtual bool isClassInitialized(TR_OpaqueClassBlock *);
   virtual bool isClassVisible(TR_OpaqueClassBlock * sourceClass, TR_OpaqueClassBlock * destClass);
   virtual bool sameClassLoaders(TR_OpaqueClassBlock *, TR_OpaqueClassBlock *);
   virtual bool jitStaticsAreSame(TR_ResolvedMethod *, int32_t, TR_ResolvedMethod *, int32_t);
   virtual bool jitFieldsAreSame(TR_ResolvedMethod *, int32_t, TR_ResolvedMethod *, int32_t, int32_t);

   virtual uintptrj_t getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock * clazz);//d169771 [2177]

   virtual bool helpersNeedRelocation() { return false; }
   virtual bool needClassAndMethodPointerRelocations() { return false; }
   virtual bool needRelocationsForStatics() { return false; }
   virtual bool nopsAlsoProcessedByRelocations() { return false; }

   bool supportsContextSensitiveInlining () { return true; }

   virtual bool callTargetsNeedRelocations() { return false; }

   virtual TR::DataType dataTypeForLoadOrStore(TR::DataType dt) { return (dt == TR::Int8 || dt == TR::Int16) ? TR::Int32 : dt; }

   virtual bool isDecimalFormatPattern(TR::Compilation *comp, TR_ResolvedMethod *method) { return false; }

   static bool createGlobalFrontEnd(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo);
   static TR_J9VMBase * get(J9JITConfig *, J9VMThread *, VM_TYPE vmType=DEFAULT_VM);
   static char *getJ9FormattedName(J9JITConfig *, J9PortLibrary *, char *, int32_t, char *, char *, bool suffix=false);
   static TR_Processor getPPCProcessorType();
   static void initializeX86ProcessorVendorId(J9JITConfig *);

   static bool isBigDecimalClass(J9UTF8 * className);
   bool isCachedBigDecimalClassFieldAddr(){ return cachedStaticDFPAvailField; }
   void setCachedBigDecimalClassFieldAddr(){ cachedStaticDFPAvailField = true; }
   void setBigDecimalClassFieldAddr(int32_t *addr) { staticDFPHWAvailField = addr; }
   int32_t *getBigDecimalClassFieldAddr() { return staticDFPHWAvailField; }
   static bool isBigDecimalConvertersClass(J9UTF8 * className);

   int32_t *getStringClassEnableCompressionFieldAddr(TR::Compilation *comp, bool isVettedForAOT);
   bool stringEquals(TR::Compilation * comp, uintptrj_t* stringLocation1, uintptrj_t* stringLocation2, int32_t& result);
   bool getStringHashCode(TR::Compilation * comp, uintptrj_t* stringLocation, int32_t& result);

   bool isThunkArchetype(J9Method * method);

   J9VMThread * vmThread();

   bool traceIsEnabled() { return _flags.testAny(TraceIsEnabled); }
   void setTraceIsEnabled(bool b) { _flags.set(TraceIsEnabled, b); }

   uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName, char * sig)
      {
      return getInstanceFieldOffset(classPointer, fieldName, (uint32_t)strlen(fieldName), sig, (uint32_t)strlen(sig));
      }
   uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName, char * sig, uintptrj_t options)
      {
      return getInstanceFieldOffset(classPointer, fieldName, (uint32_t)strlen(fieldName), sig, (uint32_t)strlen(sig), options);
      }

   virtual uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName,
                                                     uint32_t fieldLen, char * sig, uint32_t sigLen, UDATA options);
   virtual uint32_t getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName,
                                                     uint32_t fieldLen, char * sig, uint32_t sigLen);

   // Not implemented
   virtual TR_ResolvedMethod * getObjectNewInstanceImplMethod(TR_Memory *) { return 0; }

   bool stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass);

   virtual bool supportsEmbeddedHeapBounds()  { return true; }
   virtual bool supportsFastNanoTime();
   virtual bool supportsGuardMerging()        { return true; }
   virtual bool canDevirtualizeDispatch()     { return true; }
   virtual bool doStringPeepholing()          { return true; }

   virtual bool forceUnresolvedDispatch() { return false; }

   virtual TR_OpaqueMethodBlock * getMethodFromClass(TR_OpaqueClassBlock *, char *, char *, TR_OpaqueClassBlock * = NULL);

   virtual void getResolvedMethods(TR_Memory *, TR_OpaqueClassBlock *, List<TR_ResolvedMethod> *);
   virtual TR_OpaqueMethodBlock *getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, int32_t cpIndex, bool ignoreReResolve = true);
   virtual TR_OpaqueMethodBlock *getResolvedInterfaceMethod(TR_OpaqueMethodBlock *ownerMethod, TR_OpaqueClassBlock * classObject, int32_t cpIndex);

   uintptrj_t getReferenceField(uintptrj_t objectPointer, char *fieldName, char *fieldSignature)
      {
      return getReferenceFieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, fieldSignature));
      }
   uintptrj_t getVolatileReferenceField(uintptrj_t objectPointer, char *fieldName, char *fieldSignature)
      {
      return getVolatileReferenceFieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, fieldSignature));
      }

   virtual TR_Method * createMethod(TR_Memory *, TR_OpaqueClassBlock *, int32_t);
   virtual TR_ResolvedMethod * createResolvedMethod(TR_Memory *, TR_OpaqueMethodBlock *, TR_ResolvedMethod * = 0, TR_OpaqueClassBlock * = 0);
   virtual TR_ResolvedMethod * createResolvedMethodWithSignature(TR_Memory *, TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *, char *signature, int32_t signatureLength, TR_ResolvedMethod *);
   void * getStaticFieldAddress(TR_OpaqueClassBlock *, unsigned char *, uint32_t, unsigned char *, uint32_t);
   virtual int32_t getInterpreterVTableSlot(TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *);
   virtual int32_t getVTableSlot(TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *);
   virtual uint32_t           offsetOfIsOverriddenBit();
   virtual uint32_t           offsetOfMethodIsBreakpointedBit();

   virtual TR_Debug *     createDebug( TR::Compilation * comp = NULL);

   virtual void               acquireCompilationLock();
   virtual void               releaseCompilationLock();

   virtual void               acquireLogMonitor();
   virtual void               releaseLogMonitor();

   virtual bool               isAsyncCompilation();
   virtual uintptrj_t         getProcessID();
   virtual char *             getFormattedName(char *, int32_t, char *, char *, bool);

   virtual void               invalidateCompilationRequestsForUnloadedMethods(TR_OpaqueClassBlock *, bool);

   virtual void               initializeHasFixedFrameC_CallingConvention() {}
   virtual void               initializeHasResumableTrapHandler() {}
   virtual void               initializeProcessorType() {}

   void                       initializeSystemProperties();
   virtual bool               hasFPU();
   virtual bool               hasResumableTrapHandler();
   virtual bool               hasFixedFrameC_CallingConvention();
   virtual bool               pushesOutgoingArgsAtCallSite( TR::Compilation *comp);


   bool                       getX86SupportsHLE();
   virtual bool               getX86SupportsPOPCNT();

   virtual TR::PersistentInfo * getPersistentInfo()  { return ((TR_PersistentMemory *)_jitConfig->scratchSegment)->getPersistentInfo(); }
   void                       unknownByteCode( TR::Compilation *, uint8_t opcode);

   virtual bool isBenefitInliningCheckIfFinalizeObject() { return false; }


   virtual char*              printAdditionalInfoOnAssertionFailure( TR::Compilation *comp);

   virtual bool               vmRequiresSelector(uint32_t mask);

   virtual int32_t            getLineNumberForMethodAndByteCodeIndex(TR_OpaqueMethodBlock *method, int32_t bcIndex);
   virtual bool               isJavaOffloadEnabled();
   uint32_t                   getMethodSize(TR_OpaqueMethodBlock *method);
   void *                     getMethods(TR_OpaqueClassBlock *classPointer);
   uint32_t                   getNumInnerClasses(TR_OpaqueClassBlock *classPointer);
   uint32_t                   getNumMethods(TR_OpaqueClassBlock *classPointer);

   virtual uint8_t *          allocateCodeMemory( TR::Compilation *, uint32_t warmCodeSize, uint32_t coldCodeSize, uint8_t **coldCode, bool isMethodHeaderNeeded=true);
   virtual void               resizeCodeMemory( TR::Compilation *, uint8_t *, uint32_t numBytes);

   virtual uint8_t *          getCodeCacheBase();
   virtual uint8_t *          getCodeCacheBase(TR::CodeCache *);
   virtual uint8_t *          getCodeCacheTop();
   virtual uint8_t *          getCodeCacheTop(TR::CodeCache *);
   virtual void               releaseCodeMemory(void *startPC, uint8_t bytesToSaveAtStart);
   virtual uint8_t *          allocateRelocationData( TR::Compilation *, uint32_t numBytes);

   virtual bool               supportsCodeCacheSnippets();
#if defined(TR_TARGET_X86)
   void *                     getAllocationPrefetchCodeSnippetAddress( TR::Compilation * comp);
   void *                     getAllocationNoZeroPrefetchCodeSnippetAddress( TR::Compilation * comp);
#endif

#if defined(TR_TARGET_S390)
   virtual void generateBinaryEncodingPrologue(TR_BinaryEncodingData *beData, TR::CodeGenerator *cg);
#endif

   virtual TR::TreeTop *lowerTree(TR::Compilation *, TR::Node *, TR::TreeTop *);

   virtual bool canRelocateDirectNativeCalls() {return true; }

   virtual bool storeOffsetToArgumentsInVirtualIndirectThunks() { return false; }

   virtual bool               tlhHasBeenCleared();
   virtual bool               isStaticObjectFlags();
   virtual uint32_t           getStaticObjectFlags();
   virtual uintptrj_t         getOverflowSafeAllocSize();
   virtual bool               callTheJitsArrayCopyHelper();
   virtual void *             getReferenceArrayCopyHelperAddress();

   virtual uintptrj_t         thisThreadGetStackLimitOffset();
   virtual uintptrj_t         thisThreadGetCurrentExceptionOffset();
   virtual uintptrj_t         thisThreadGetPublicFlagsOffset();
   virtual uintptrj_t         thisThreadGetJavaPCOffset();
   virtual uintptrj_t         thisThreadGetJavaSPOffset();
   virtual uintptrj_t         thisThreadGetJavaLiteralsOffset();

   // Move to CompilerEnv VM?
   virtual uintptrj_t         thisThreadGetSystemSPOffset();

   virtual uintptrj_t         thisThreadGetMachineSPOffset();
   virtual uintptrj_t         thisThreadGetMachineBPOffset( TR::Compilation *);
   virtual uintptrj_t         thisThreadGetJavaFrameFlagsOffset();
   virtual uintptrj_t         thisThreadGetCurrentThreadOffset();
   virtual uintptrj_t         thisThreadGetFloatTemp1Offset();
   virtual uintptrj_t         thisThreadGetFloatTemp2Offset();
   virtual uintptrj_t         thisThreadGetTempSlotOffset();
   virtual uintptrj_t         thisThreadGetReturnValueOffset();
   virtual uintptrj_t         getThreadDebugEventDataOffset(int32_t index);
   virtual uintptrj_t         thisThreadGetDLTBlockOffset();
   uintptrj_t                 getDLTBufferOffsetInBlock();
   virtual int32_t *          getCurrentLocalsMapForDLT( TR::Compilation *);
   virtual bool               compiledAsDLTBefore(TR_ResolvedMethod *);
   virtual uintptrj_t         getObjectHeaderSizeInBytes();
   uintptrj_t                 getOffsetOfSuperclassesInClassObject();
   uintptrj_t                 getOffsetOfBackfillOffsetField();

   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims) { return 0; }

   bool vftFieldRequiresMask(){ return ~TR::Compiler->om.maskOfObjectVftField() != 0; }

   virtual uintptrj_t         getOffsetOfDiscontiguousArraySizeField();
   virtual uintptrj_t         getOffsetOfContiguousArraySizeField();
   virtual uintptrj_t         getJ9ObjectContiguousLength();
   virtual uintptrj_t         getJ9ObjectDiscontiguousLength();
   virtual uintptrj_t         getOffsetOfArrayClassRomPtrField();
   virtual uintptrj_t         getOffsetOfClassRomPtrField();
   virtual uintptrj_t         getOffsetOfClassInitializeStatus();

   virtual uintptrj_t         getOffsetOfJ9ObjectJ9Class();
   virtual uintptrj_t         getObjectHeaderHasBeenMovedInClass();
   virtual uintptrj_t         getObjectHeaderHasBeenHashedInClass();
   virtual uintptrj_t         getJ9ObjectFlagsMask32();
   virtual uintptrj_t         getJ9ObjectFlagsMask64();
   uintptrj_t                 getOffsetOfJ9ThreadJ9VM();
   uintptrj_t                 getOffsetOfJ9ROMArrayClassArrayShape();
   uintptrj_t                 getOffsetOfJLThreadJ9Thread();
   uintptrj_t                 getOffsetOfJavaVMIdentityHashData();
   uintptrj_t                 getOffsetOfJ9IdentityHashData1();
   uintptrj_t                 getOffsetOfJ9IdentityHashData2();
   uintptrj_t                 getOffsetOfJ9IdentityHashData3();
   uintptrj_t                 getOffsetOfJ9IdentityHashDataHashSaltTable();
   uintptrj_t                 getJ9IdentityHashSaltPolicyStandard();
   uintptrj_t                 getJ9IdentityHashSaltPolicyRegion();
   uintptrj_t                 getJ9IdentityHashSaltPolicyNone();
   uintptrj_t                 getIdentityHashSaltPolicy();
   virtual uintptrj_t         getJ9JavaClassRamShapeShift();
   virtual uintptrj_t         getObjectHeaderShapeMask();

   virtual bool               assumeLeftMostNibbleIsZero();

   virtual uintptrj_t         getOSRFrameHeaderSizeInBytes();
   virtual uintptrj_t         getOSRFrameSizeInBytes(TR_OpaqueMethodBlock* method);
   virtual bool               ensureOSRBufferSize(uintptrj_t osrFrameSizeInBytes, uintptrj_t osrScratchBufferSizeInBytes, uintptrj_t osrStackFrameSizeInBytes);

   virtual bool               generateCompressedPointers();
   virtual bool               generateCompressedLockWord();

   virtual void                 printVerboseLogHeader(TR::Options *);
   virtual void                 printPID();

   virtual void                 emitNewPseudoRandomNumberVerbosePrefix();
   virtual void                 emitNewPseudoRandomNumberVerbose(int32_t i);
   virtual void                 emitNewPseudoRandomVerboseSuffix();
   virtual void                 emitNewPseudoRandomNumberVerboseLine(int32_t i);

   virtual void                 emitNewPseudoRandomStringVerbosePrefix();
   virtual void                 emitNewPseudoRandomStringVerbose(char *);
   virtual void                 emitNewPseudoRandomStringVerboseLine(char *);

   uint32_t                   getAllocationSize(TR::StaticSymbol *classSym, TR_OpaqueClassBlock * clazz);

   virtual TR_OpaqueClassBlock *getObjectClass(uintptrj_t objectPointer);
   virtual uintptrj_t           getReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t offsetFromHeader);
   virtual uintptrj_t           getVolatileReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t offsetFromHeader);
   virtual uintptrj_t           getReferenceFieldAtAddress(uintptrj_t fieldAddress);
   virtual uintptrj_t           getReferenceFieldAtAddress(void *fieldAddress){ return getReferenceFieldAtAddress((uintptrj_t)fieldAddress); }
   int32_t                      getInt32FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset);

   int32_t getInt32Field(uintptrj_t objectPointer, char *fieldName)
      {
      return getInt32FieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, "I"));
      }

   int64_t getInt64Field(uintptrj_t objectPointer, char *fieldName)
      {
      return getInt64FieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, "J"));
      }
   int64_t                      getInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset);
   void                         setInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t newValue);
   void setInt64Field(uintptrj_t objectPointer, char *fieldName, int64_t newValue)
      {
      setInt64FieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, "J"), newValue);
      }

   bool                         compareAndSwapInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t oldValue, int64_t newValue);
   bool compareAndSwapInt64Field(uintptrj_t objectPointer, char *fieldName, int64_t oldValue, int64_t newValue)
      {
      return compareAndSwapInt64FieldAt(objectPointer, getInstanceFieldOffset(getObjectClass(objectPointer), fieldName, "J"), oldValue, newValue);
      }

   intptrj_t                    getArrayLengthInElements(uintptrj_t objectPointer);
   int32_t                      getInt32Element(uintptrj_t objectPointer, int32_t elementIndex);
   virtual uintptrj_t           getReferenceElement(uintptrj_t objectPointer, intptrj_t elementIndex);

   virtual TR_OpaqueClassBlock *getClassFromJavaLangClass(uintptrj_t objectPointer);
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool callSiteVettedForAOT=false) { return 0; }

   virtual uintptrj_t         getOffsetOfLastITableFromClassField();
   virtual uintptrj_t         getOffsetOfInterfaceClassFromITableField();
   virtual int32_t            getITableEntryJitVTableOffset();  // Must add this to the negation of an itable entry to get a jit vtable offset
   virtual int32_t            convertITableIndexToOffset(uintptrj_t itableIndex);
   virtual uintptrj_t         getOffsetOfJavaLangClassFromClassField();
   virtual uintptrj_t         getOffsetOfInitializeStatusFromClassField();
   virtual uintptrj_t         getOffsetOfClassFromJavaLangClassField();
   virtual uintptrj_t         getOffsetOfRamStaticsFromClassField();
   virtual uintptrj_t         getOffsetOfInstanceShapeFromClassField();
   virtual uintptrj_t         getOffsetOfInstanceDescriptionFromClassField();
   virtual uintptrj_t         getOffsetOfDescriptionWordFromPtrField();

   virtual uintptrj_t         getConstantPoolFromMethod(TR_OpaqueMethodBlock *);

   virtual uintptrj_t         getOffsetOfIsArrayFieldFromRomClass();
   virtual uintptrj_t         getOffsetOfClassAndDepthFlags();
   virtual uintptrj_t         getOffsetOfClassFlags();
   virtual uintptrj_t         getOffsetOfArrayComponentTypeField();
   virtual uintptrj_t         getOffsetOfIndexableSizeField();
   virtual uintptrj_t         constReleaseVMAccessOutOfLineMask();
   virtual uintptrj_t         constReleaseVMAccessMask();
   virtual uintptrj_t         constAcquireVMAccessOutOfLineMask();
   virtual uintptrj_t         constJNICallOutFrameFlagsOffset();
   virtual uintptrj_t         constJNICallOutFrameType();
   virtual uintptrj_t         constJNICallOutFrameSpecialTag();
   virtual uintptrj_t         constJNICallOutFrameInvisibleTag();
   virtual uintptrj_t         constJNICallOutFrameFlags();
   virtual uintptrj_t         constJNIReferenceFrameAllocatedFlags();
   virtual uintptrj_t         constClassFlagsPrimitive();
   virtual uintptrj_t         constClassFlagsAbstract();
   virtual uintptrj_t         constClassFlagsFinal();
   virtual uintptrj_t         constClassFlagsPublic();

   virtual uintptrj_t         getGCForwardingPointerOffset();

   virtual bool               jniRetainVMAccess(TR_ResolvedMethod *method);
   virtual bool               jniNoGCPoint(TR_ResolvedMethod *method);
   virtual bool               jniNoNativeMethodFrame(TR_ResolvedMethod *method);
   virtual bool               jniNoExceptionsThrown(TR_ResolvedMethod *method);
   virtual bool               jniNoSpecialTeardown(TR_ResolvedMethod *method);
   virtual bool               jniDoNotWrapObjects(TR_ResolvedMethod *method);
   virtual bool               jniDoNotPassReceiver(TR_ResolvedMethod *method);
   virtual bool               jniDoNotPassThread(TR_ResolvedMethod *method);

   virtual uintptrj_t         thisThreadJavaVMOffset();
   uintptrj_t                 getMaxObjectSizeForSizeClass();
   uintptrj_t                 thisThreadAllocationCacheCurrentOffset(uintptrj_t);
   uintptrj_t                 thisThreadAllocationCacheTopOffset(uintptrj_t);
   uintptrj_t                 getCellSizeForSizeClass(uintptrj_t);
   virtual uintptrj_t         getObjectSizeClass(uintptrj_t);

   uintptrj_t                 thisThreadMonitorCacheOffset();
   uintptrj_t                 thisThreadOSThreadOffset();

   uintptrj_t                 getMonitorNextOffset();
   uintptrj_t                 getMonitorOwnerOffset();
   uintptrj_t                 getMonitorEntryCountOffset();

   uintptrj_t                 getRealtimeSizeClassesOffset();
   uintptrj_t                 getSmallCellSizesOffset();
   uintptrj_t                 getSizeClassesIndexOffset();

   virtual int32_t            getFirstArrayletPointerOffset( TR::Compilation *comp);
   int32_t                    getArrayletFirstElementOffset(int8_t elementSize,  TR::Compilation *comp);

   virtual uintptrj_t         thisThreadGetProfilingBufferCursorOffset();
   virtual uintptrj_t         thisThreadGetProfilingBufferEndOffset();
   virtual uintptrj_t         thisThreadGetOSRBufferOffset();
   virtual uintptrj_t         thisThreadGetOSRScratchBufferOffset();
   virtual uintptrj_t         thisThreadGetOSRFrameIndexOffset();
   virtual uintptrj_t         thisThreadGetOSRReturnAddressOffset();

   virtual int32_t            getArraySpineShift(int32_t);
   virtual int32_t            getArrayletMask(int32_t);
   virtual int32_t            getArrayletLeafIndex(int32_t, int32_t);
   virtual int32_t            getLeafElementIndex(int32_t , int32_t);
   virtual bool               CEEHDLREnabled();

   virtual int32_t            getCAASaveOffset();
   virtual int32_t            getFlagValueForPrimitiveTypeCheck();
   virtual int32_t            getFlagValueForArrayCheck();
   virtual int32_t            getFlagValueForFinalizerCheck();
   uint32_t                   getWordOffsetToGCFlags();
   uint32_t                   getWriteBarrierGCFlagMaskAsByte();
   virtual int32_t            getByteOffsetToLockword(TR_OpaqueClassBlock *clazzPointer);

   virtual bool               javaLangClassGetModifiersImpl(TR_OpaqueClassBlock * clazzPointer, int32_t &result);
   virtual int32_t            getJavaLangClassHashCode(TR::Compilation * comp, TR_OpaqueClassBlock * clazzPointer, bool &hashCodeComputed);

   virtual int32_t getCompInfo(char *processorName, int32_t stringLength) { return -1; }

   virtual bool traceableMethodsCanBeInlined() { return false; }

   bool isAnyMethodTracingEnabled(TR_OpaqueMethodBlock *method);


   bool                       isLogSamplingSet();

   virtual bool               hasFinalFieldsInClass(TR_OpaqueClassBlock * classPointer);

   virtual TR_OpaqueClassBlock *getProfiledClassFromProfiledInfo(TR_ExtraAddressInfo *profiledInfo);

   virtual bool               scanReferenceSlotsInClassForOffset( TR::Compilation * comp, TR_OpaqueClassBlock * classPointer, int32_t offset);
   virtual int32_t            findFirstHotFieldTenuredClassOffset( TR::Compilation *comp, TR_OpaqueClassBlock *opclazz);

   virtual bool               isInlineableNativeMethod( TR::Compilation *, TR::ResolvedMethodSymbol * methodSymbol);
   //receiverClass is for specifying a more specific receiver type. otherwise it is determined from the call.
   virtual bool               maybeHighlyPolymorphic(TR::Compilation *, TR_ResolvedMethod *caller, int32_t cpIndex, TR_Method *callee, TR_OpaqueClassBlock *receiverClass = NULL);
   virtual bool isQueuedForVeryHotOrScorching(TR_ResolvedMethod *calleeMethod, TR::Compilation *comp);

   //getSymbolAndFindInlineTarget queries
   virtual bool supressInliningRecognizedInitialCallee(TR_CallSite* callsite, TR::Compilation* comp);
   virtual int checkInlineableWithoutInitialCalleeSymbol (TR_CallSite* callsite, TR::Compilation* comp);
   virtual int checkInlineableTarget (TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp, bool inJSR292InliningPasses);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION 
   virtual bool inlineRecognizedCryptoMethod(TR_CallTarget* target, TR::Compilation* comp);
   virtual bool inlineNativeCryptoMethod(TR::Node *callNode, TR::Compilation *comp);
#endif
   virtual void refineColdness (TR::Node* node, bool& isCold);

   virtual TR::Node * inlineNativeCall( TR::Compilation *,  TR::TreeTop *, TR::Node *) { return 0; }

   virtual bool               inlinedAllocationsMustBeVerified() { return false; }

   virtual int32_t            getInvocationCount(TR_OpaqueMethodBlock *methodInfo);
   virtual bool               setInvocationCount(TR_OpaqueMethodBlock *methodInfo, int32_t oldCount, int32_t newCount);
   virtual bool               startAsyncCompile(TR_OpaqueMethodBlock *methodInfo, void *oldStartPC, bool *queued, TR_OptimizationPlan *optimizationPlan  = NULL);
   virtual bool               isBeingCompiled(TR_OpaqueMethodBlock *methodInfo, void *startPC);
   virtual uint32_t           virtualCallOffsetToVTableSlot(uint32_t offset);
   virtual void *             addressOfFirstClassStatic(TR_OpaqueClassBlock *);

   virtual TR_ResolvedMethod * getDefaultConstructor(TR_Memory *, TR_OpaqueClassBlock *);

   virtual uint32_t           getNewInstanceImplVirtualCallOffset();

   char * classNameChars(TR::Compilation *comp, TR::SymbolReference * symRef, int32_t & length);

   virtual char *             getClassNameChars(TR_OpaqueClassBlock * clazz, int32_t & length);
   virtual char *             getClassSignature_DEPRECATED(TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *);
   virtual char *             getClassSignature(TR_OpaqueClassBlock * clazz, TR_Memory *);
   virtual int32_t            printTruncatedSignature(char *sigBuf, int32_t bufLen, TR_OpaqueMethodBlock *method);

   virtual bool               isClassFinal(TR_OpaqueClassBlock *);
   virtual bool               isClassArray(TR_OpaqueClassBlock *);
   virtual bool               isFinalFieldPointingAtJ9Class(TR::SymbolReference *symRef, TR::Compilation *comp);

   virtual const char * getX86ProcessorVendorId();
   virtual uint32_t     getX86ProcessorSignature();
   virtual uint32_t     getX86ProcessorFeatureFlags();
   virtual uint32_t     getX86ProcessorFeatureFlags2();
   virtual uint32_t     getX86ProcessorFeatureFlags8();
   virtual bool         getX86SupportsSSE4_1();

   virtual void *getJ2IThunk(char *signatureChars, uint32_t signatureLength,  TR::Compilation *comp);
   virtual void *setJ2IThunk(char *signatureChars, uint32_t signatureLength, void *thunkptr,  TR::Compilation *comp);
   virtual void *setJ2IThunk(TR_Method *method, void *thunkptr, TR::Compilation *comp);  // DMDM: J9 now

   // JSR292 {{{

   // J2I thunk support
   //
   virtual void * getJ2IThunk(TR_Method *method, TR::Compilation *comp);  // DMDM: J9 now

   virtual char    *getJ2IThunkSignatureForDispatchVirtual(char *invokeHandleSignature, uint32_t signatureLength,  TR::Compilation *comp);
   virtual TR::Node *getEquivalentVirtualCallNodeForDispatchVirtual(TR::Node *node,  TR::Compilation *comp);
   virtual bool     needsInvokeExactJ2IThunk(TR::Node *node,  TR::Compilation *comp);

   virtual void *findPersistentJ2IThunk(char *signatureChars);
   virtual void *findPersistentThunk(char *signatureChars, uint32_t signatureLength);
   virtual void * persistJ2IThunk(void *thunk);


   // Object manipulation
   //
   virtual uintptrj_t  methodHandle_thunkableSignature(uintptrj_t methodHandle);
   virtual void *      methodHandle_jitInvokeExactThunk(uintptrj_t methodHandle);
   virtual uintptrj_t  methodHandle_type(uintptrj_t methodHandle);
   virtual uintptrj_t  methodType_descriptor(uintptrj_t methodType);
   virtual uintptrj_t *mutableCallSite_bypassLocation(uintptrj_t mutableCallSite);
   virtual uintptrj_t *mutableCallSite_findOrCreateBypassLocation(uintptrj_t mutableCallSite);

   virtual TR_OpaqueMethodBlock *lookupArchetype(TR_OpaqueClassBlock *clazz, char *name, char *signature);
   virtual TR_OpaqueMethodBlock *lookupMethodHandleThunkArchetype(uintptrj_t methodHandle);
   virtual TR_ResolvedMethod    *createMethodHandleArchetypeSpecimen(TR_Memory *, uintptrj_t *methodHandleLocation, TR_ResolvedMethod *owningMethod = 0);
   virtual TR_ResolvedMethod    *createMethodHandleArchetypeSpecimen(TR_Memory *, TR_OpaqueMethodBlock *archetype, uintptrj_t *methodHandleLocation, TR_ResolvedMethod *owningMethod = 0); // more efficient if you already know the archetype

   virtual uintptrj_t mutableCallSiteCookie(uintptrj_t mutableCallSite, uintptrj_t potentialCookie=0);

   bool hasMethodTypesSideTable();

   // JSR292 }}}

   virtual uintptrj_t getFieldOffset( TR::Compilation * comp, TR::SymbolReference* classRef, TR::SymbolReference* fieldRef);
   virtual bool canDereferenceAtCompileTime(TR::SymbolReference *fieldRef,  TR::Compilation *comp);

   virtual bool getStringFieldByName( TR::Compilation *, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult);
   virtual bool      isString(TR_OpaqueClassBlock *clazz);
   virtual TR_YesNoMaybe typeReferenceStringObject(TR_OpaqueClassBlock *clazz);
   virtual bool      isJavaLangObject(TR_OpaqueClassBlock *clazz);
   virtual bool      isString(uintptrj_t objectPointer);
   virtual int32_t   getStringLength(uintptrj_t objectPointer);
   virtual uint16_t  getStringCharacter(uintptrj_t objectPointer, int32_t index);
   virtual intptrj_t getStringUTF8Length(uintptrj_t objectPointer);
   virtual char     *getStringUTF8      (uintptrj_t objectPointer, char *buffer, intptrj_t bufferSize);

   virtual uint32_t getVarHandleHandleTableOffset(TR::Compilation *);

   virtual void reportILGeneratorPhase();
   virtual void reportOptimizationPhase(OMR::Optimizations);
   virtual void reportAnalysisPhase(uint8_t id);
   virtual void reportOptimizationPhaseForSnap(OMR::Optimizations, TR::Compilation *comp = NULL);
   virtual void reportCodeGeneratorPhase(TR::CodeGenPhase::PhaseValue phase);
   virtual int32_t saveCompilationPhase();
   virtual void restoreCompilationPhase(int32_t phase);

   virtual void reportPrexInvalidation(void * startPC);
   virtual void traceAssumeFailure(int32_t line, const char * file);

   virtual bool compilationShouldBeInterrupted( TR::Compilation *, TR_CallingContext);
   bool checkForExclusiveAcquireAccessRequest( TR::Compilation *);
   virtual bool haveAccess();
   virtual bool haveAccess(TR::Compilation *);
   virtual void releaseAccess(TR::Compilation *);
   virtual bool tryToAcquireAccess( TR::Compilation *, bool *);

   virtual bool compileMethods(TR::OptionSet*, void *);
   virtual void waitOnCompiler(void *);

   bool tossingCode();

   virtual TR::KnownObjectTable::Index getCompiledMethodReceiverKnownObjectIndex( TR::Compilation *comp);
   virtual bool methodMayHaveBeenInterpreted( TR::Compilation *comp);
   virtual bool canRecompileMethodWithMatchingPersistentMethodInfo( TR::Compilation *comp);

   virtual void abortCompilationIfLowFreePhysicalMemory(TR::Compilation *comp, size_t sizeToAllocate = 0);

   virtual void               revertToInterpreted(TR_OpaqueMethodBlock *method);

   virtual bool               argumentCanEscapeMethodCall(TR::MethodSymbol *method, int32_t argIndex);

   virtual bool               hasTwoWordObjectHeader();
   virtual int32_t *          getReferenceSlotsInClass( TR::Compilation *, TR_OpaqueClassBlock *classPointer);
   virtual void               initializeLocalObjectHeader( TR::Compilation *, TR::Node * allocationNode,  TR::TreeTop * allocationTreeTop);
   virtual void               initializeLocalArrayHeader ( TR::Compilation *, TR::Node * allocationNode,  TR::TreeTop * allocaitonTreeTop);
   virtual TR::TreeTop*        initializeClazzFlagsMonitorFields(TR::Compilation*, TR::TreeTop* prevTree, TR::Node* allocationNode, TR::Node* classNode, J9Class* ramClass);

   bool shouldPerformEDO(TR::Block *catchBlock,  TR::Compilation * comp);

   // Multiple code cache support
   virtual TR::CodeCache *getDesignatedCodeCache( TR::Compilation *comp); // MCT
   virtual void setHasFailedCodeCacheAllocation(); // MCT
   void *getCCPreLoadedCodeAddress(TR::CodeCache *codeCache, TR_CCPreLoadedCode h, TR::CodeGenerator *cg);

   virtual void reserveTrampolineIfNecessary( TR::Compilation *, TR::SymbolReference *symRef, bool inBinaryEncoding);
   virtual intptrj_t indexedTrampolineLookup(int32_t helperIndex, void *callSite);
   virtual void reserveNTrampolines( TR::Compilation *, int32_t n, bool inBinaryEncoding);
   virtual TR::CodeCache* getResolvedTrampoline( TR::Compilation *, TR::CodeCache* curCache, J9Method * method, bool inBinaryEncoding) = 0;
   virtual bool needsMethodTrampolines();

   // Interpreter profiling support
   virtual TR_IProfiler  *getIProfiler();
   virtual TR_AbstractInfo *createIProfilingValueInfo( TR_ByteCodeInfo &bcInfo,  TR::Compilation *comp);
   virtual TR_ValueProfileInfo   *getValueProfileInfoFromIProfiler(TR_ByteCodeInfo &bcInfo,  TR::Compilation *comp);
   uint32_t *getAllocationProfilingDataPointer(TR_ByteCodeInfo &bcInfo, TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method,  TR::Compilation *comp);
   uint32_t *getGlobalAllocationDataPointer();
   virtual TR_ExternalProfiler   *hasIProfilerBlockFrequencyInfo(TR::Compilation& comp);
   virtual int32_t getCGEdgeWeight(TR::Node *callerNode, TR_OpaqueMethodBlock *callee,  TR::Compilation *comp);
   virtual int32_t getIProfilerCallCount(TR_OpaqueMethodBlock *caller, int32_t bcIndex,  TR::Compilation *);
   virtual int32_t getIProfilerCallCount(TR_OpaqueMethodBlock *callee, TR_OpaqueMethodBlock *caller, int32_t bcIndex,  TR::Compilation *);
   virtual void setIProfilerCallCount(TR_OpaqueMethodBlock *caller, int32_t bcIndex, int32_t count,  TR::Compilation *);
   virtual int32_t getIProfilerCallCount(TR_ByteCodeInfo &bcInfo,  TR::Compilation *comp);
   virtual void    setIProfilerCallCount(TR_ByteCodeInfo &bcInfo, int32_t count,  TR::Compilation *comp);
   virtual bool    isCallGraphProfilingEnabled();
   virtual bool    isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT = false);
   virtual bool    isClassLibraryClass(TR_OpaqueClassBlock *clazz);
   virtual int32_t getMaxCallGraphCallCount();

   virtual void getNumberofCallersAndTotalWeight(TR_OpaqueMethodBlock *method, uint32_t *count, uint32_t *weight);
   virtual uint32_t getOtherBucketWeight(TR_OpaqueMethodBlock *method);
   virtual bool getCallerWeight(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *callerMethod , uint32_t *weight);
   virtual bool getCallerWeight(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *callerMethod , uint32_t *weight, uint32_t pcIndex);

   virtual bool    getSupportsRecognizedMethods();

   // Hardware profiling support
   virtual void    createHWProfilerRecords(TR::Compilation *comp);

   virtual uint64_t getUSecClock();
   virtual uint64_t getHighResClock();
   virtual uint64_t getHighResClockResolution();

   TR_JitPrivateConfig *getPrivateConfig();
   static TR_JitPrivateConfig *getPrivateConfig(void *jitConfig);

   TR_YesNoMaybe vmThreadIsCompilationThread() { return _vmThreadIsCompilationThread; }

   virtual bool releaseClassUnloadMonitorAndAcquireVMaccessIfNeeded( TR::Compilation *comp, bool *hadClassUnloadMonitor);
   virtual void acquireClassUnloadMonitorAndReleaseVMAccessIfNeeded( TR::Compilation *comp, bool hadVMAccess, bool hadClassUnloadMonitor);
   // The following two methods are defined here so that they can be easily inlined
   virtual bool acquireVMAccessIfNeeded() { return acquireVMaccessIfNeeded(vmThread(), _vmThreadIsCompilationThread); }
   virtual void releaseVMAccessIfNeeded(bool haveAcquiredVMAccess) { releaseVMaccessIfNeeded(vmThread(), haveAcquiredVMAccess); }
   virtual uintptrj_t         getBytecodePC(TR_OpaqueMethodBlock *method, TR_ByteCodeInfo &bcInfo);

   virtual bool hardwareProfilingInstructionsNeedRelocation() { return false; }

   // support for using compressed class pointers
   // in the future we may add a base value to the classOffset to transform it
   // into a real J9Class pointer
   virtual TR_OpaqueClassBlock *convertClassPtrToClassOffset(J9Class *clazzPtr);
   virtual J9Method *convertMethodOffsetToMethodPtr(TR_OpaqueMethodBlock *methodOffset);
   virtual TR_OpaqueMethodBlock *convertMethodPtrToMethodOffset(J9Method *methodPtr);


    TR::TreeTop * lowerAsyncCheck( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
    TR::TreeTop * lowerAtcCheck( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
   virtual bool isMethodEnterTracingEnabled(TR_OpaqueMethodBlock *method);
   virtual bool isMethodEnterTracingEnabled(J9Method *j9method)
      {
      return isMethodEnterTracingEnabled((TR_OpaqueMethodBlock *)j9method);
      }
   virtual bool isMethodExitTracingEnabled(TR_OpaqueMethodBlock *method);
   virtual bool isMethodExitTracingEnabled(J9Method *j9method)
      {
      return isMethodExitTracingEnabled((TR_OpaqueMethodBlock *)j9method);
      }

   virtual bool isSelectiveMethodEnterExitEnabled();

   virtual bool canMethodEnterEventBeHooked();
   virtual bool canMethodExitEventBeHooked();
   virtual bool methodsCanBeInlinedEvenIfEventHooksEnabled();

   TR::TreeTop * lowerMethodHook( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
   TR::TreeTop * lowerArrayLength( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
   TR::TreeTop * lowerContigArrayLength( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
   TR::TreeTop * lowerMultiANewArray( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);
   TR::TreeTop * lowerToVcall( TR::Compilation *, TR::Node * root,  TR::TreeTop * treeTop);

   TR::Node * initializeLocalObjectFlags( TR::Compilation *, TR::Node * allocationNode, J9Class * ramClass);

   virtual bool             testOSForSSESupport();
   virtual J9JITConfig *getJ9JITConfig() { return _jitConfig; }

   virtual int32_t getCompThreadIDForVMThread(void *vmThread);
   virtual uint8_t getCompilationShouldBeInterruptedFlag();

   bool shouldSleep() { _shouldSleep = !_shouldSleep; return _shouldSleep; } // no need for virtual; we need this per thread

   virtual bool isAnonymousClass(TR_OpaqueClassBlock *j9clazz) { return (J9_ARE_ALL_BITS_SET(((J9Class*)j9clazz)->romClass->extraModifiers, J9AccClassAnonClass)); }
   virtual int64_t getCpuTimeSpentInCompThread(TR::Compilation * comp); // resolution is 0.5 sec or worse. Returns -1 if unavailable

   virtual void *             getClassLoader(TR_OpaqueClassBlock * classPointer);

   J9VMThread *            _vmThread;
   J9PortLibrary *         _portLibrary;
   J9JITConfig *           _jitConfig;
   J9InternalVMFunctions * _vmFunctionTable;
   TR::CompilationInfo *    _compInfo; // storing _compInfo in multiple places could spell trouble when we free the structure
   TR_IProfiler  *         _iProfiler;
   int32_t                 _hwProfilerShouldNotProcessBuffers;
   uint8_t*                _bufferStart;

   // To minimize the overhead of testing if vmThread is the compilation thread (which may be
   // performed very often when compiling without VMaccess) we store this information here
   // The TR_J9VMBase is attached to the thread in vmThread->jitVMwithThreadInfo and
   // vmThread->aotVMwithThreadInfo
   TR_YesNoMaybe          _vmThreadIsCompilationThread;
   TR::CompilationInfoPerThread *_compInfoPT;
   bool                   _shouldSleep; // used to throttle application threads

   TR_J9SharedCache *     _sharedCache;

   static int32_t *       staticDFPHWAvailField;
   static bool            cachedStaticDFPAvailField;

   static int32_t *       staticStringEnableCompressionFieldAddr;

   static bool            cachedStaticAMRDCASAvailField;
   static int32_t *       staticAMRDCASAvailField;

   static bool            cachedStaticAMRDSetAvailField;
   static int32_t *       staticAMRDSetAvailField;

   static bool            cachedStaticASRDCASAvailField;
   static int32_t *       staticASRDCASAvailField;

   static bool            cachedStaticASRDSetAvailField;
   static int32_t *       staticASRDSetAvailField;

   static char            x86VendorID[13];
   static bool            x86VendorIDInitialized;

#if !defined(HINTS_IN_SHAREDCACHE_OBJECT)
   bool isSharedCacheHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   void addSharedCacheHint(TR_ResolvedMethod *, TR_SharedCacheHint);
   virtual uint16_t getAllSharedCacheHints(J9Method *method);
   bool isSharedCacheHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   void addSharedCacheHint(J9Method *, TR_SharedCacheHint);
#endif

   virtual J9Class * matchRAMclassFromROMclass(J9ROMClass * clazz,  TR::Compilation * comp);
   virtual J9VMThread * getCurrentVMThread();

   uint8_t *allocateDataCacheRecord(uint32_t numBytes,  TR::Compilation *comp, bool contiguous,
                                bool *shouldRetryAllocation, uint32_t allocationType, uint32_t *size);

   virtual int findOrCreateMethodSymRef(TR::Compilation* comp, TR::ResolvedMethodSymbol* owningMethodSym, char* classSig, char** methodSig, TR::SymbolReference** symRefs, int methodCount);
   virtual int findOrCreateMethodSymRef(TR::Compilation* comp, TR::ResolvedMethodSymbol* owningMethodSym, char** methodSig, TR::SymbolReference** symRefs, int methodCount);
   virtual TR::SymbolReference* findOrCreateMethodSymRef(TR::Compilation* comp, TR::ResolvedMethodSymbol* owningMethodSym, char* classSig, char* methodSig);
   virtual TR::SymbolReference* findOrCreateMethodSymRef(TR::Compilation* comp, TR::ResolvedMethodSymbol* owningMethodSym, char* methodSig);

   virtual bool isOwnableSyncClass(TR_OpaqueClassBlock *clazz);
   const char *getJ9MonitorName(J9ThreadMonitor* monitor);

   virtual TR_J9SharedCache *sharedCache() { return _sharedCache; }
   virtual void              freeSharedCache();

   const char *getByteCodeName(uint8_t opcode);

   virtual int32_t maxInternalPlusPinningArrayPointers(TR::Compilation* comp);

   virtual void scanClassForReservation(TR_OpaqueClassBlock *,  TR::Compilation *comp);

   virtual void *getSystemClassLoader();

   virtual TR_EstimateCodeSize *getCodeEstimator( TR::Compilation *comp);
   virtual void releaseCodeEstimator( TR::Compilation *comp, TR_EstimateCodeSize *estimator);

   virtual bool acquireClassTableMutex();
   virtual void releaseClassTableMutex(bool);

   virtual TR_OpaqueClassBlock *getClassFromStatic(void *p);

   // --------------------------------------------------------------------------
   // Object model
   // --------------------------------------------------------------------------

   virtual bool getNurserySpaceBounds(uintptrj_t *base, uintptrj_t *top);
   virtual uintptrj_t getLowTenureAddress();
   virtual uintptrj_t getHighTenureAddress();
   virtual uintptrj_t getThreadLowTenureAddressPointerOffset();
   virtual uintptrj_t getThreadHighTenureAddressPointerOffset();
   virtual uintptrj_t thisThreadRememberedSetFragmentOffset();
   virtual uintptrj_t getFragmentParentOffset();
   virtual uintptrj_t getRememberedSetGlobalFragmentOffset();
   virtual uintptrj_t getLocalFragmentOffset();
   virtual int32_t getLocalObjectAlignmentInBytes();

   uint32_t getInstanceFieldOffsetIncludingHeader(char* classSignature, char * fieldName, char * fieldSig, TR_ResolvedMethod* method);

   virtual void markHotField( TR::Compilation *, TR::SymbolReference *, TR_OpaqueClassBlock *, bool);
   virtual void markClassForTenuredAlignment( TR::Compilation *comp, TR_OpaqueClassBlock *opclazz, uint32_t alignFromStart);

   virtual bool shouldDelayAotLoad() { return false; }

   virtual void *getLocationOfClassLoaderObjectPointer(TR_OpaqueClassBlock *classPointer);
   virtual bool isMethodBreakpointed(TR_OpaqueMethodBlock *method);

   protected:
#if defined(TR_TARGET_S390)
   int32_t getS390MachineName(TR_S390MachineType machine, char* processorName, int32_t stringLength);
#endif

   private:
#if !defined(HINTS_IN_SHAREDCACHE_OBJECT)
   uint32_t     getSharedCacheHint(J9VMThread * vmThread, J9Method *romMethod, J9SharedClassConfig * scConfig);
#endif

protected:

   enum // _flags
      {
      TraceIsEnabled = 0x0000001,
      DummyLastEnum
      };

   flags32_t _flags;

   };

class TR_J9VM : public TR_J9VMBase
   {
public:
   TR_J9VM(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, J9VMThread * vmContext);

   virtual bool               classHasBeenExtended(TR_OpaqueClassBlock *);
   virtual bool               classHasBeenReplaced(TR_OpaqueClassBlock *);
   virtual TR::Node *          inlineNativeCall( TR::Compilation *,  TR::TreeTop *, TR::Node *);
   virtual TR_OpaqueClassBlock *getClassOfMethod(TR_OpaqueMethodBlock *method);
   virtual int32_t            getObjectAlignmentInBytes();

   virtual void               initializeProcessorType();
   virtual void               initializeHasResumableTrapHandler();
   virtual void               initializeHasFixedFrameC_CallingConvention();

   virtual bool               isPublicClass(TR_OpaqueClassBlock *clazz);
   virtual TR_OpaqueMethodBlock *             getMethodFromName(char * className, char *methodName, char *signature, TR_OpaqueMethodBlock *callingMethod=0);
   virtual uintptrj_t         getClassDepthAndFlagsValue(TR_OpaqueClassBlock * classPointer);

   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass);
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass);
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass);
   virtual int32_t               getNewArrayTypeFromClass(TR_OpaqueClassBlock *clazz);
   virtual TR_OpaqueClassBlock * getClassFromNewArrayType(int32_t arrayType);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims);
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT=false);
   virtual TR_YesNoMaybe      isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock *castClass, bool instanceIsFixed, bool castIsFixed = true, bool optimizeForAOT=false);


   virtual TR_OpaqueClassBlock *             getSuperClass(TR_OpaqueClassBlock *classPointer);
   virtual bool               isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *);

   virtual TR_OpaqueClassBlock * getClassClassPointer(TR_OpaqueClassBlock *);

   // for replay
   virtual TR_OpaqueClassBlock * getClassFromMethodBlock(TR_OpaqueMethodBlock *);

   virtual int32_t getCompInfo(char *processorName, int32_t stringLength);

   virtual const char *       sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *memory);

   virtual TR_ResolvedMethod * getObjectNewInstanceImplMethod(TR_Memory *);

   virtual intptrj_t          methodTrampolineLookup( TR::Compilation *, TR::SymbolReference *symRef, void *callSite);
   virtual TR::CodeCache*    getResolvedTrampoline( TR::Compilation *, TR::CodeCache *curCache, J9Method * method, bool inBinaryEncoding);

   virtual TR_IProfiler *         getIProfiler();

   virtual void                   createHWProfilerRecords(TR::Compilation *comp);

   virtual TR_OpaqueClassBlock *  getClassOffsetForAllocationInlining(J9Class *clazzPtr);
   virtual J9Class *              getClassForAllocationInlining( TR::Compilation *comp, TR::SymbolReference *classSymRef);
   virtual bool                   supportAllocationInlining( TR::Compilation *comp, TR::Node *node);
   virtual TR_OpaqueClassBlock *  getPrimitiveArrayAllocationClass(J9Class *clazz);
   virtual uint32_t               getPrimitiveArrayOffsetInJavaVM(uint32_t arrayType);

   virtual bool isDecimalFormatPattern( TR::Compilation *comp, TR_ResolvedMethod *method);

private:
   void transformJavaLangClassIsArrayOrIsPrimitive( TR::Compilation *, TR::Node * callNode,  TR::TreeTop * treeTop, int32_t andMask);
   void transformJavaLangClassIsArray( TR::Compilation *, TR::Node * callNode,  TR::TreeTop * treeTop);
   void setProcessorByDebugOption();
   };

#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
class TR_J9SharedCacheVM : public TR_J9VM
   {
public:
   TR_J9SharedCacheVM(J9JITConfig * aJitConfig, TR::CompilationInfo * compInfo, J9VMThread * vmContext);

   // in process of removing this query in favour of more meaningful queries below
   virtual bool               isAOT_DEPRECATED_DO_NOT_USE()                                         { return true; }

   // replacing calls to isAOT
   virtual bool               supportsCodeCacheSnippets()                     { return false; }
   virtual bool               canRelocateDirectNativeCalls()                  { return false; }
   virtual bool               needClassAndMethodPointerRelocations()          { return true; }
   virtual bool               inlinedAllocationsMustBeVerified()              { return true; }
   virtual bool               helpersNeedRelocation()                         { return true; }
   virtual bool               supportsEmbeddedHeapBounds()                    { return false; }
   virtual bool               supportsFastNanoTime()                          { return false; }
   virtual bool               needRelocationsForStatics()                     { return true; }
   virtual bool               forceUnresolvedDispatch()                       { return true; }
   virtual bool               nopsAlsoProcessedByRelocations()                { return true; }
   virtual bool               supportsGuardMerging()                          { return false; }
   virtual bool               canDevirtualizeDispatch()                       { return false; }
   virtual bool               storeOffsetToArgumentsInVirtualIndirectThunks() { return true; }
   virtual bool               callTargetsNeedRelocations()                    { return true; }
   virtual bool               doStringPeepholing()                            { return false; }
   virtual bool               hardwareProfilingInstructionsNeedRelocation()   { return true; }
   virtual bool               supportsMethodEntryPadding()                    { return false; }
   virtual bool               isBenefitInliningCheckIfFinalizeObject()          { return true; }
   virtual bool               shouldDelayAotLoad();

   virtual bool               isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT = false);

   virtual bool               isMethodEnterTracingEnabled(TR_OpaqueMethodBlock *method);
   virtual bool               isMethodExitTracingEnabled(TR_OpaqueMethodBlock *method);
   virtual bool               traceableMethodsCanBeInlined();

   virtual bool               canMethodEnterEventBeHooked();
   virtual bool               canMethodExitEventBeHooked();
   virtual bool               methodsCanBeInlinedEvenIfEventHooksEnabled();
   virtual TR_OpaqueClassBlock *getClassOfMethod(TR_OpaqueMethodBlock *method);
   virtual void               getResolvedMethods(TR_Memory *, TR_OpaqueClassBlock *, List<TR_ResolvedMethod> *);
   virtual void               scanClassForReservation(TR_OpaqueClassBlock *,  TR::Compilation *comp);
   virtual uint32_t           getInstanceFieldOffset(TR_OpaqueClassBlock * classPointer, char * fieldName,
                                                     uint32_t fieldLen, char * sig, uint32_t sigLen, UDATA options);
#if defined(TR_TARGET_S390)
   virtual void               initializeS390zLinuxProcessorFeatures();
   virtual void               initializeS390zOSProcessorFeatures();
#endif

   virtual int32_t            getJavaLangClassHashCode(TR::Compilation * comp, TR_OpaqueClassBlock * clazzPointer, bool &hashCodeComputed);
   virtual bool               javaLangClassGetModifiersImpl(TR_OpaqueClassBlock * clazzPointer, int32_t &result);
   virtual TR_OpaqueClassBlock *             getSuperClass(TR_OpaqueClassBlock *classPointer);

   virtual bool               sameClassLoaders(TR_OpaqueClassBlock *, TR_OpaqueClassBlock *);
   virtual bool               isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *);
   virtual bool               classHasBeenExtended(TR_OpaqueClassBlock *);
   virtual bool               isGetImplInliningSupported();
   virtual bool               isPublicClass(TR_OpaqueClassBlock *clazz);
   virtual bool               hasFinalizer(TR_OpaqueClassBlock * classPointer);
   virtual uintptrj_t         getClassDepthAndFlagsValue(TR_OpaqueClassBlock * classPointer);
   virtual TR_OpaqueMethodBlock *             getMethodFromName(char * className, char *methodName, char *signature, TR_OpaqueMethodBlock *callingMethod=0);
   virtual TR_OpaqueMethodBlock * getMethodFromClass(TR_OpaqueClassBlock *, char *, char *, TR_OpaqueClassBlock * = NULL);
   virtual bool               isPrimitiveClass(TR_OpaqueClassBlock *clazz);
   virtual TR_OpaqueClassBlock * getComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass);
   virtual TR_OpaqueClassBlock * getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass);
   virtual TR_OpaqueClassBlock * getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass);
   virtual TR_OpaqueClassBlock * getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims);
   virtual TR_OpaqueClassBlock * getClassFromNewArrayType(int32_t arrayType);
   virtual TR_YesNoMaybe      isInstanceOf(TR_OpaqueClassBlock *instanceClass, TR_OpaqueClassBlock *castClass, bool instanceIsFixed, bool castIsFixed = true, bool optimizeForAOT=false);
   virtual bool               isClassVisible(TR_OpaqueClassBlock * sourceClass, TR_OpaqueClassBlock * destClass);
   virtual bool               isPrimitiveArray(TR_OpaqueClassBlock *);
   virtual bool               isReferenceArray(TR_OpaqueClassBlock *);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT=false);
   virtual TR_OpaqueClassBlock * getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT=false);

   virtual intptrj_t          methodTrampolineLookup( TR::Compilation *, TR::SymbolReference *symRef, void *callSite);
   virtual TR::CodeCache *    getResolvedTrampoline( TR::Compilation *, TR::CodeCache* curCache, J9Method * method, bool inBinaryEncoding);

   virtual TR_OpaqueMethodBlock *getInlinedCallSiteMethod(TR_InlinedCallSite *ics);
   virtual TR_OpaqueClassBlock *getProfiledClassFromProfiledInfo(TR_ExtraAddressInfo *profiledInfo);

   // Multiple code cache support
   virtual TR::CodeCache *getDesignatedCodeCache( TR::Compilation *comp); // MCT

   virtual void *getJ2IThunk(char *signatureChars, uint32_t signatureLength,  TR::Compilation *comp);
   virtual void *setJ2IThunk(char *signatureChars, uint32_t signatureLength, void *thunkptr,  TR::Compilation *comp);

   virtual void * persistJ2IThunk(void *thunk);
   virtual void *persistThunk(char *signatureChars, uint32_t signatureLength, uint8_t *thunkStart, uint32_t totalSize);

   virtual TR_ResolvedMethod * getObjectNewInstanceImplMethod(TR_Memory *);
   virtual bool                   supportAllocationInlining( TR::Compilation *comp, TR::Node *node);

   virtual J9Class *              getClassForAllocationInlining( TR::Compilation *comp, TR::SymbolReference *classSymRef);
   };

#endif

#define J9RAM_CP_BASE(x) ((x*) (vm->literals))

struct TR_PCMap
   {
   uint32_t      _numberOfEntries;
   TR_PCMapEntry _mapEntries[1]; // size is allocated dynamically
   };

typedef J9JITExceptionTable TR_MethodMetaData;

TR_MethodMetaData * createMethodMetaData(TR_J9VMBase &, TR_ResolvedMethod *, TR::Compilation *);

extern J9JITConfig * jitConfig;

struct TR_MetaDataStats
   {
#if defined(DEBUG)
   ~TR_MetaDataStats();
#endif

   uint32_t _counter;
   uint32_t _codeSize;
   uint32_t _maxCodeSize;
   uint32_t _exceptionDataSize;
   uint32_t _tableSize;
   uint32_t _inlinedCallDataSize;
   uint32_t _gcDataSize;
   uint32_t _relocationSize;

   };

extern struct TR_MetaDataStats metaDataStats;

static inline char * utf8Data(J9UTF8 * name)
   {
   return (char *)J9UTF8_DATA(name);
   }

static inline char * utf8Data(J9UTF8 * name, uint32_t & len)
   {
   len = J9UTF8_LENGTH(name);
   return (char *)J9UTF8_DATA(name);
   }

static inline char * utf8Data(J9UTF8 * name, int32_t & len)
   {
   len = J9UTF8_LENGTH(name);
   return (char *)J9UTF8_DATA(name);
   }

inline bool isValidVmStateIndex(uint32_t &index)
   {
   // basic bounds checking
   //
   if (index <= 0 ||
       (index >= 10 && index != 0x11))
      return false;
   //HACK: is there a better way to set the correct index?
   if (index == 0x11) index = 0x9;
   return true;
   }

inline TR_PersistentMemory * persistentMemory(J9JITConfig * jitConfig) { return (TR_PersistentMemory *)jitConfig->scratchSegment; }
bool signalOutOfMemory(J9JITConfig *);

#endif // VMJ9

