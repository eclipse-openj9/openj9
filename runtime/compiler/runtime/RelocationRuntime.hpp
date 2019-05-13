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

#ifndef RELOCATION_RUNTIME_INCL
#define RELOCATION_RUNTIME_INCL

#include "j9cfg.h"

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))
   #define TR_SHARED_CACHE_AOT_SE_PLATFORM
#endif

#include <assert.h>
#include "codegen/Relocation.hpp"
#include "env/j9method.h"
#include "runtime/HWProfiler.hpp"
#include "env/VMJ9.h"
#include "env/J9CPU.hpp"

namespace TR { class CompilationInfo; }
class TR_RelocationRecord;
class TR_RelocationRuntimeLogger;
class TR_RelocationTarget;
class TR_ResolvedMethod;
namespace TR { class CodeCache; }
namespace TR { class PersistentInfo; }

#ifdef __cplusplus
extern "C" {
#endif

/* TR_AOTHeader Versions:
 *    1.0    Java6 GA
 *    1.1    Java6 SR1
 *    2.1    Java7 Beta
 *    2.2    Java7
 *    2.3    Java7
 *    3.0    Java7.1
 *    3.1    Java7.1
 *    4.1    Java8 (828)
 *    5.0    Java9 (929 AND 829)
 */

#define TR_AOTHeaderMajorVersion 5
#define TR_AOTHeaderMinorVersion 1
#define TR_AOTHeaderEyeCatcher   0xA0757A27

/* AOT Header Flags */
typedef enum TR_AOTFeatureFlags
   {
   TR_FeatureFlag_sanityCheckBegin              = 0x00000001,
   TR_FeatureFlag_IsSMP                         = 0x00000002,
   TR_FeatureFlag_UsesCompressedPointers        = 0x00000004,
   TR_FeatureFlag_UseDFPHardware                = 0x00000008,
   TR_FeatureFlag_DisableTraps                  = 0x00000010,
   TR_FeatureFlag_TLHPrefetch                   = 0x00000020,
   TR_FeatureFlag_MethodTrampolines             = 0x00000040,
   TR_FeatureFlag_MultiTenancy                  = 0x00000080,
   TR_FeatureFlag_HCREnabled                    = 0x00000100,
   TR_FeatureFlag_SIMDEnabled                   = 0x00000200,     //set and tested for s390
   TR_FeatureFlag_AsyncCompilation              = 0x00000400,     //async compilation - switch to interpreter code NOT generated
   TR_FeatureFlag_ConcurrentScavenge            = 0x00000800,
   TR_FeatureFlag_SoftwareReadBarrier           = 0x00001000,
   TR_FeatureFlag_UsesTM                        = 0x00002000,
   TR_FeatureFlag_SanityCheckEnd                = 0x80000000
   } TR_AOTFeatureFlags;


typedef struct TR_Version {
   uintptr_t structSize;
   uintptr_t majorVersion;
   uintptr_t minorVersion;
   char vmBuildVersion[64];
   char jitBuildVersion[64];
} TR_Version;

typedef struct TR_AOTHeader {
    uintptr_t eyeCatcher;
    TR_Version version;
    uintptr_t *relativeMethodMetaDataTable;
    uintptr_t architectureAndOs;
    uintptr_t endiannessAndWordSize;
    uintptr_t processorSignature;
    uintptr_t featureFlags;
    uintptr_t vendorId;
    uintptr_t gcPolicyFlag;
    uintptr_t compressedPointerShift;
    uint32_t lockwordOptionHashValue;
    int32_t   arrayLetLeafSize;
    TR_ProcessorFeatureFlags processorFeatureFlags;
} TR_AOTHeader;

typedef struct TR_AOTRuntimeInfo {
    struct TR_AOTHeader* aotHeader;
    struct J9MemorySegment* codeCache;
    struct J9MemorySegment* dataCache;
    void* baseJxeAddress;
    uintptr_t compileFirstClassLocation;
    uintptr_t *fe;
} TR_AOTRuntimeInfo;

#ifdef __cplusplus
}
#endif

class TR_RelocationRuntime {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_RelocationRuntime(J9JITConfig *jitCfg);

      TR_RelocationTarget *reloTarget()                           { return _reloTarget; }
      TR_RelocationRuntimeLogger *reloLogger()                    { return _reloLogger; }
      TR_AOTStats *aotStats()                                     { return _aotStats; }

      J9JITConfig *jitConfig()                                    { return _jitConfig; }
      TR_FrontEnd *fe()                                           { return _fe; }
      TR_J9VMBase *fej9()                                         { return (TR_J9VMBase *)_fe; }
      J9JavaVM *javaVM()                                          { return _javaVM; }
      TR_Memory *trMemory()                                       { return _trMemory; }
      TR::CompilationInfo *compInfo()                              { return _compInfo; }
      J9VMThread *currentThread()                                 { return _currentThread; }
      J9Method *method()                                          { return _method; }
      TR::CodeCache *codeCache()                                  { return _codeCache; }
      J9MemorySegment *dataCache()                                { return _dataCache; }
      bool useCompiledCopy()                                      { return _useCompiledCopy; }
      bool haveReservedCodeCache()                                { return _haveReservedCodeCache; }

      J9JITExceptionTable * exceptionTable()                      { return _exceptionTable; }
      uint8_t * methodCodeStart()                                 { return _newMethodCodeStart; }
      J9ConstantPool *ramCP()                                     { return _ramCP; }
      UDATA metaDataAllocSize()                                   { return _metaDataAllocSize; }
      TR_AOTMethodHeader *aotMethodHeaderEntry()                  { return _aotMethodHeaderEntry; }
      J9JITDataCacheHeader *exceptionTableCacheEntry()            { return _exceptionTableCacheEntry; }
      uint8_t *newMethodCodeStart()                               { return _newMethodCodeStart; }
      UDATA codeCacheDelta()                                      { return _codeCacheDelta; }
      UDATA dataCacheDelta()                                      { return _dataCacheDelta; }
      UDATA classReloAmount()                                     { return _classReloAmount; }
      U_8 *baseAddress()                                          { return _baseAddress; }
      uintptr_t compileFirstClassLocation()                       { return _compileFirstClassLocation; }

      UDATA reloStartTime()                                       { return _reloStartTime; }
      void setReloStartTime(UDATA time)                           { _reloStartTime = time; }

      UDATA reloEndTime()                                         { return _reloEndTime; }
      void setReloEndTime(UDATA time)                             { _reloEndTime = time; }

      int32_t returnCode()                                        { return _returnCode; }
      void setReturnCode(int32_t rc)                              { _returnCode = rc; }

      TR::Options *options()                                       { return _options; }
      TR::Compilation *comp()                                    { return _comp; }
      TR_ResolvedMethod *currentResolvedMethod()                  { return _currentResolvedMethod; }

      TR::PersistentInfo *getPersistentInfo()                      { return comp()->getPersistentInfo(); }

      // current main entry point
      J9JITExceptionTable *prepareRelocateAOTCodeAndData(J9VMThread* vmThread,
                                                         TR_FrontEnd *fe,
                                                         TR::CodeCache *aotMCCRuntimeCodeCache,
                                                         const J9JITDataCacheHeader *cacheEntry,
                                                         J9Method *theMethod,
                                                         bool shouldUseCompiledCopy,
                                                         TR::Options *options,
                                                         TR::Compilation *compilation,
                                                         TR_ResolvedMethod *resolvedMethod);

      virtual bool storeAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe, J9VMThread *curThread);
      virtual TR_AOTHeader *createAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe);
      virtual bool validateAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe, J9VMThread *curThread);

      virtual void *isROMClassInSharedCaches(UDATA romClassValue, J9JavaVM *javaVM);
      virtual bool isRomClassForMethodInSharedCache(J9Method *method, J9JavaVM *javaVM);
      virtual TR_YesNoMaybe isMethodInSharedCache(J9Method *method, J9JavaVM *javaVM);
      virtual TR_OpaqueClassBlock *getClassFromCP(J9VMThread *vmThread, J9JavaVM *javaVM, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic);

      static uintptr_t    getGlobalValue(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueList[g];
         }
      static void         setGlobalValue(uint32_t g, uintptr_t v)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         _globalValueList[g] = v;
         }
      static char *       nameOfGlobal(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueNames[g];
         }

      bool isLoading() { return _isLoading; }
      void setIsLoading() { _isLoading = true; }
      void resetIsLoading() { _isLoading = false; }

      void initializeHWProfilerRecords(TR::Compilation *comp);
      void addClazzRecord(uint8_t *ia, uint32_t bcIndex, TR_OpaqueMethodBlock *method);

#if 1 // defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      // Detect unexpected scenarios when build has assumes
      void incNumValidations() { _numValidations++; }
      void incNumFailedValidations() { _numFailedValidations++; }
      void incNumInlinedMethodRelos() { _numInlinedMethodRelos++; }
      void incNumFailedInlinedMethodRelos() { _numFailedInlinedMethodRelos++; }
      void incNumInlinedAllocRelos() { _numInlinedAllocRelos++; }
      void incNumFailedAllocInlinedRelos() { _numFailedInlinedAllocRelos++; }

      uint32_t getNumValidations() { return _numValidations; }
      uint32_t getNumFailedValidations() { return _numFailedValidations; }
      uint32_t getNumInlinedMethodRelos() { return _numInlinedMethodRelos; }
      uint32_t getNumFailedInlinedMethodRelos() { return _numFailedInlinedMethodRelos; }
      uint32_t getNumInlinedAllocRelos() { return _numInlinedAllocRelos; }
      uint32_t getNumFailedAllocInlinedRelos() { return _numFailedInlinedAllocRelos; }

#else
      void incNumValidations() { }
      void incNumFailedValidations() { }
      void incNumInlinedMethodRelos() { }
      void incNumFailedInlinedMethodRelos() { }
      void incNumInlinedAllocRelos() { }
      void incNumFailedAllocInlinedRelos() { }

      uint32_t getNumValidations() { return 0; }
      uint32_t getNumFailedValidations() { return 0; }
      uint32_t getNumInlinedMethodRelos() { return 0; }
      uint32_t getNumFailedInlinedMethodRelos() { return 0; }
      uint32_t getNumInlinedAllocRelos() { return 0; }
      uint32_t getNumFailedAllocInlinedRelos() { return 0; }
#endif

   private:
      virtual uint8_t * allocateSpaceInCodeCache(UDATA codeSize)                           { return NULL; }

      virtual uint8_t * allocateSpaceInDataCache(UDATA metaDataSize,
                                                 UDATA type)                               { return NULL; }

      virtual void initializeAotRuntimeInfo()                                              {}

      virtual void initializeCacheDeltas()                                                 {}

      void relocateAOTCodeAndData(U_8 *tempDataStart,
                                  U_8 *oldDataStart,
                                  U_8 *codeStart,
                                  U_8 *oldCodeStart);

      void relocateMethodMetaData(UDATA codeRelocationAmount, UDATA dataRelocationAmount);

      bool aotMethodHeaderVersionsMatch();

      typedef enum {
         RelocationNoError = 1,
         RelocationNoClean = -1, //just a general failure, nothing to really clean
         RelocationTableCreateError = -2, //we failed to allocate to data cache (_newExceptionTableEntry)
         RelocationAssumptionCreateError = -3,
         RelocationPersistentCreateError = -4, //we failed to allocate data cache (_newPersistentInfo)
         RelocationCodeCreateError = -5, //could not allocate code cache entry
         RelocationFailure = -6 //error sometime during actual relocation, full cleanup needed (code & data)
      } TR_AotRelocationCleanUp;
      TR_AotRelocationCleanUp _relocationStatus;
      void relocationFailureCleanup();

      static bool       _globalValuesInitialized;
      static uintptr_t  _globalValueList[TR_NumGlobalValueItems];
      static uint8_t    _globalValueSizeList[TR_NumGlobalValueItems];
      static char      *_globalValueNames[TR_NumGlobalValueItems];


   protected:

      J9JITConfig *_jitConfig;
      J9JavaVM *_javaVM;
      TR_FrontEnd *_fe;
      TR_Memory *_trMemory;
      TR::CompilationInfo * _compInfo;

      TR_RelocationTarget *_reloTarget;
      TR_RelocationRuntimeLogger *_reloLogger;
      TR_AOTStats *_aotStats;
      J9JITExceptionTable *_exceptionTable;
      uint8_t *_newExceptionTableStart;
      uint8_t *_newPersistentInfo;

      // inlined J9AOTWalkRelocationInfo
      UDATA _dataCacheDelta;
      UDATA _codeCacheDelta;
      // omitted handlers for all the relocation record types

      // inlined TR_AOTRuntimeInfo
      struct TR_AOTHeader* _aotHeader;
      uintptr_t _compileFirstClassLocation;
      U_8 * _baseAddress;
      UDATA _classReloAmount;

      TR::CodeCache *_codeCache;
      struct J9MemorySegment* _dataCache;

      bool _useCompiledCopy;
      UDATA _metaDataAllocSize;
      TR_AOTMethodHeader *_aotMethodHeaderEntry;
      J9JITDataCacheHeader *_exceptionTableCacheEntry;
      J9VMThread *_currentThread;
      J9Method *_method;
      J9ConstantPool *_ramCP;
      uint8_t * _newMethodCodeStart;

      bool _haveReservedCodeCache;

      UDATA _reloStartTime;
      UDATA _reloEndTime;

      int32_t _returnCode;

      TR::Options *_options;
      TR::Compilation *_comp;
      TR_ResolvedMethod *_currentResolvedMethod;

      bool _isLoading;

#if 1 // defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      // Detect unexpected scenarios when build has assumes
      uint32_t _numValidations;
      uint32_t _numFailedValidations;
      uint32_t _numInlinedMethodRelos;
      uint32_t _numFailedInlinedMethodRelos;
      uint32_t _numInlinedAllocRelos;
      uint32_t _numFailedInlinedAllocRelos;
#endif
};


class TR_SharedCacheRelocationRuntime : public TR_RelocationRuntime {
public:
      TR_ALLOC(TR_Memory::Relocation);
      void * operator new(size_t, J9JITConfig *);
      TR_SharedCacheRelocationRuntime(J9JITConfig *jitCfg) :
         _sharedCacheIsFull(false), TR_RelocationRuntime(jitCfg) {}

      virtual bool storeAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe, J9VMThread *curThread);
      virtual TR_AOTHeader *createAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe);
      virtual bool validateAOTHeader(J9JavaVM *javaVM, TR_FrontEnd *fe, J9VMThread *curThread);

      virtual void *isROMClassInSharedCaches(UDATA romClassValue, J9JavaVM *javaVM);
      virtual bool isRomClassForMethodInSharedCache(J9Method *method, J9JavaVM *javaVM);
      virtual TR_YesNoMaybe isMethodInSharedCache(J9Method *method, J9JavaVM *javaVM);

      virtual TR_OpaqueClassBlock *getClassFromCP(J9VMThread *vmThread, J9JavaVM *javaVM, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic);

private:
      uint32_t getCurrentLockwordOptionHashValue(J9JavaVM *vm) const;
      virtual uint8_t * allocateSpaceInCodeCache(UDATA codeSize);
      virtual uint8_t * allocateSpaceInDataCache(UDATA metaDataSize, UDATA type);
      virtual void initializeAotRuntimeInfo();
      virtual void initializeCacheDeltas();

      virtual void incompatibleCache(U_32 module, U_32 reason, char *assumeMessage);

      void checkAOTHeaderFlags(TR_FrontEnd *fe, TR_AOTHeader * hdrInCache, intptr_t featureFlags);
      bool generateError(char *assumeMessage);

      bool _sharedCacheIsFull;

      static bool useDFPHardware(TR_FrontEnd *fe);
      static uintptr_t generateFeatureFlags(TR_FrontEnd *fe);

      static const char aotHeaderKey[];
      static const UDATA aotHeaderKeyLength;
};

#endif   // RELOCATION_RUNTIME_INCL
