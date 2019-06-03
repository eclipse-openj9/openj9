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

#include <algorithm>
#include "j9cp.h"
#include "j9cfg.h"
#include "j9.h"
#include "j9consts.h"
#include "j9version.h"
#include "jilconsts.h"
#include "rommeth.h"
#include "j9protos.h"
#include "jvminit.h"
#include "jitprotos.h"
#include "env/ut_j9jit.h"

#if defined(J9VM_OPT_SHARED_CLASSES)
#include "j9jitnls.h"
#endif

#include "env/VMJ9.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/J9Runtime.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/DataCache.hpp"
#include "codegen/FrontEnd.hpp"
#include "infra/Monitor.hpp"
#include "env/PersistentInfo.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/CompilerEnv.hpp"

#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRuntimeLogger.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationTarget.hpp"
#include "arm/runtime/ARMRelocationTarget.hpp"
#include "x/runtime/X86RelocationTarget.hpp"
#include "p/runtime/PPCRelocationTarget.hpp"
#include "z/runtime/S390RelocationTarget.hpp"

#include "runtime/RuntimeAssumptions.hpp"

#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"

#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/CompilationRuntime.hpp"

TR_RelocationRuntime::TR_RelocationRuntime(J9JITConfig *jitCfg)
   {
   _method = NULL;
   _ramCP = NULL;

   _jitConfig = jitCfg;
   _javaVM = jitCfg->javaVM;
   _trMemory = NULL;
   _options = TR::Options::getAOTCmdLineOptions();
   _compInfo = TR::CompilationInfo::get(_jitConfig);

   PORT_ACCESS_FROM_JAVAVM(javaVM());
   _reloLogger = new (PERSISTENT_NEW) TR_RelocationRuntimeLogger(this);
   if (_reloLogger == NULL)
      {
      // TODO: need error condition here
      return;
      }

   _aotStats = ((TR_JitPrivateConfig *)jitConfig()->privateConfig)->aotStats;

   #if defined(TR_HOST_X86)
      #if defined(TR_HOST_64BIT)
      _reloTarget = new (PERSISTENT_NEW) TR_AMD64RelocationTarget(this);
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_X86RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_POWER)
      #if defined(TR_HOST_64BIT)
      _reloTarget = new (PERSISTENT_NEW) TR_PPC64RelocationTarget(this);
      #else
      _reloTarget = new (PERSISTENT_NEW) TR_PPC32RelocationTarget(this);
      #endif
   #elif defined(TR_HOST_S390)
      _reloTarget = new (PERSISTENT_NEW) TR_S390RelocationTarget(this);
   #elif defined(TR_HOST_ARM)
      _reloTarget = new (PERSISTENT_NEW) TR_ARMRelocationTarget(this);
   #else
      TR_ASSERT(0, "Unsupported relocation target");
   #endif

   if (_reloTarget == NULL)
      {
      // TODO: need error condition here
      return;
      }

   // initialize global values
   if (!_globalValuesInitialized)
      {
      J9VMThread *vmThread = javaVM()->internalVMFunctions->currentVMThread(javaVM());
      TR::PersistentInfo *pinfo = ((TR_PersistentMemory *)_jitConfig->scratchSegment)->getPersistentInfo();
      setGlobalValue(TR_CountForRecompile, (uintptr_t) &(pinfo->_countForRecompile));
      setGlobalValue(TR_HeapBaseForBarrierRange0, (uintptr_t) *(uintptr_t*)((uint8_t*)vmThread + offsetof(J9VMThread, heapBaseForBarrierRange0)));
      setGlobalValue(TR_ActiveCardTableBase, (uintptr_t) *(uintptr_t*)((uint8_t*)vmThread + offsetof(J9VMThread, activeCardTableBase)));
      setGlobalValue(TR_HeapSizeForBarrierRange0, (uintptr_t) *(uintptr_t*)((uint8_t*)vmThread + offsetof(J9VMThread, heapSizeForBarrierRange0)));
      setGlobalValue(TR_MethodEnterHookEnabledAddress, (uintptr_t) &(javaVM()->hookInterface.flags[J9HOOK_VM_METHOD_ENTER]));
      setGlobalValue(TR_MethodExitHookEnabledAddress, (uintptr_t) &(javaVM()->hookInterface.flags[J9HOOK_VM_METHOD_RETURN]));
      _globalValuesInitialized = true;
      }

      _isLoading = false;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      _numValidations = 0;
      _numFailedValidations = 0;
      _numInlinedMethodRelos = 0;
      _numFailedInlinedMethodRelos = 0;
      _numInlinedAllocRelos = 0;
      _numFailedInlinedAllocRelos = 0;
#endif
   }


// Prepare to relocate an AOT method from either a JXE or shared cache
// returns J9JITExceptionTable pointer if successful, NULL if not
J9JITExceptionTable *
TR_RelocationRuntime::prepareRelocateAOTCodeAndData(J9VMThread* vmThread,
                                                    TR_FrontEnd *theFE,
                                                    TR::CodeCache *aotMCCRuntimeCodeCache,
                                                    const J9JITDataCacheHeader *cacheEntry,
                                                    J9Method *theMethod,
                                                    bool shouldUseCompiledCopy,
                                                    TR::Options *options,
                                                    TR::Compilation *comp,
                                                    TR_ResolvedMethod *resolvedMethod)
   {
   _currentThread = vmThread;
   _fe = theFE;
   _codeCache = aotMCCRuntimeCodeCache;
   _method = theMethod;
   _ramCP = J9_CP_FROM_METHOD(_method);
   _useCompiledCopy = shouldUseCompiledCopy;
   _classReloAmount = 0;
   _exceptionTable = NULL;
   _newExceptionTableStart = NULL;
   _relocationStatus = RelocationNoError;
   _haveReservedCodeCache = false; // MCT
   _returnCode = 0;

   _comp = comp;
   _trMemory = comp->trMemory();
   _currentResolvedMethod = resolvedMethod;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;

   _options = options;
   TR_ASSERT(_options, "Options were not correctly initialized.");
   _reloLogger->setupOptions(_options);

   uint8_t *tempCodeStart, *tempDataStart;
   uint8_t *oldDataStart, *oldCodeStart, *newCodeStart;
   tempDataStart = (uint8_t *)cacheEntry;

   //Check method header is valid
   _aotMethodHeaderEntry = (TR_AOTMethodHeader *)(cacheEntry + 1); // skip the header J9JITDataCacheHeader
   if (!aotMethodHeaderVersionsMatch())
      return NULL;

   // If we want to trace this method but the AOT body is not prepared to handle it
   // we must fail this AOT load with an error code that will force retrial
   if ((fej9->isMethodTracingEnabled((TR_OpaqueMethodBlock*)theMethod) || fej9->canMethodExitEventBeHooked())
      &&
       (_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_IsNotCapableOfMethodExitTracing))
      {
      setReturnCode(compilationAotValidateMethodExitFailure);
      return NULL; // fail
      }
   if ((fej9->isMethodTracingEnabled((TR_OpaqueMethodBlock*)theMethod) || fej9->canMethodEnterEventBeHooked())
      &&
       (_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_IsNotCapableOfMethodEnterTracing))
      {
      setReturnCode(compilationAotValidateMethodEnterFailure);
      return NULL; // fail
      }

   // Check the flags related to string compression
   if (_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_UsesEnableStringCompressionFolding)
      {
      int32_t *enableCompressionFieldAddr = fej9->getStringClassEnableCompressionFieldAddr(comp, true);
      bool conflict = true;
      if (enableCompressionFieldAddr)
         {
         if (*enableCompressionFieldAddr)
            {
            if (_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_StringCompressionEnabled)
               conflict = false;
            }
         else
            {
            if (!(_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_StringCompressionEnabled))
               conflict = false;
            }
         }
      if (conflict)
         {
         setReturnCode(compilationAotValidateStringCompressionFailure);
         return NULL;
         }
      }

   // Check the flags related to the symbol validation manager
   if (_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_UsesSymbolValidationManager)
      {
      comp->setOption(TR_UseSymbolValidationManager);
      }

   if ((_aotMethodHeaderEntry->flags & TR_AOTMethodHeader_TMDisabled) && !comp->getOption(TR_DisableTM))
      {
      setReturnCode(compilationAOTValidateTMFailure);
      return NULL;
      }

   _exceptionTableCacheEntry = (J9JITDataCacheHeader *)((uint8_t *)cacheEntry + _aotMethodHeaderEntry->offsetToExceptionTable);

   if (_exceptionTableCacheEntry->type == J9_JIT_DCE_EXCEPTION_INFO)
      {
      oldDataStart = (U_8 *)_aotMethodHeaderEntry->compileMethodDataStartPC;
      oldCodeStart = (U_8 *)_aotMethodHeaderEntry->compileMethodCodeStartPC;

      UDATA dataSize = _aotMethodHeaderEntry->compileMethodDataSize;
      UDATA codeSize = _aotMethodHeaderEntry->compileMethodCodeSize;

      TR_ASSERT(codeSize > sizeof(OMR::CodeCacheMethodHeader), "codeSize for AOT loads should include the CodeCacheHeader");

      if (useCompiledCopy())
         {
         newCodeStart = oldCodeStart;
         _newExceptionTableStart = oldDataStart;
         TR_ASSERT(oldCodeStart != NULL, "assertion failure");
         TR_ASSERT(_codeCache, "assertion failure"); // some code cache must be reserved // MCT
         TR_ASSERT(oldDataStart != NULL, "assertion failure");
         _exceptionTable = (J9JITExceptionTable *) (_exceptionTableCacheEntry + 1); // skip the header J9JITDataCacheHeader
         }
      else
         {
         _newExceptionTableStart = allocateSpaceInDataCache(_exceptionTableCacheEntry->size, _exceptionTableCacheEntry->type);
         tempCodeStart = tempDataStart + dataSize;
         if (_newExceptionTableStart)
            {
            TR_DataCacheManager::copyDataCacheAllocation(reinterpret_cast<J9JITDataCacheHeader *>(_newExceptionTableStart), _exceptionTableCacheEntry);
            _exceptionTable = reinterpret_cast<J9JITExceptionTable *>(_newExceptionTableStart + sizeof(J9JITDataCacheHeader)); // Get new exceptionTable location

            // This must be an AOT load because for AOT compilations we relocate in place

            // We must prepare the list of assumptions linked to the metadata
            // We could set just a NULL pointer and let the code update that should an
            // assumption be created.
            // Another alternative is to create a sentinel entry right away to avoid
            // having to allocate one at runtime and possibly running out of memory
            OMR::RuntimeAssumption * raList = new (PERSISTENT_NEW) TR::SentinelRuntimeAssumption();
            comp->setMetadataAssumptionList(raList); // copy this list to the compilation object as well (same view as for a JIT compilation)
            _exceptionTable->runtimeAssumptionList = raList;
            // If we cannot allocate the memory, fail the compilation
            if (raList == NULL)
               _relocationStatus = RelocationAssumptionCreateError; // signal an error


            if (_exceptionTable->bodyInfo)
               {
               J9JITDataCacheHeader *persistentInfoCacheEntry = (J9JITDataCacheHeader *)((U_8 *)cacheEntry + _aotMethodHeaderEntry->offsetToPersistentInfo);
               TR_ASSERT(persistentInfoCacheEntry->type == J9_JIT_DCE_AOT_PERSISTENT_INFO, "Incorrect data cache type read from disk.");
               _newPersistentInfo = allocateSpaceInDataCache(persistentInfoCacheEntry->size, persistentInfoCacheEntry->type);
               if (_newPersistentInfo)
                  {
                  TR_DataCacheManager::copyDataCacheAllocation(reinterpret_cast<J9JITDataCacheHeader *>(_newPersistentInfo), persistentInfoCacheEntry);
                  }
               else
                  {
                  reloLogger()->maxCodeOrDataSizeWarning();
                  _relocationStatus = RelocationPersistentCreateError;
                  }
               }
            // newCodeStart points after a OMR::CodeCacheMethodHeader, but tempCodeStart points at a OMR::CodeCacheMethodHeader
            // to keep alignment consistent, back newCodeStart over the OMR::CodeCacheMethodHeader
            //we can still do the code start without the bodyInfo! need check in cleanup!
            newCodeStart = allocateSpaceInCodeCache(codeSize-sizeof(OMR::CodeCacheMethodHeader));
            if (newCodeStart)
               {
               TR_ASSERT(_codeCache->isReserved(), "codeCache must be reserved"); // MCT
               newCodeStart = ((U_8*)newCodeStart) - sizeof(OMR::CodeCacheMethodHeader);
               // Before copying, memorize the real size of the block returned by the code cache manager
               // and fix it later
               U_32 blockSize = ((OMR::CodeCacheMethodHeader*)newCodeStart)->_size;
               memcpy(newCodeStart, tempCodeStart, codeSize);  // the real size may have been overwritten
               ((OMR::CodeCacheMethodHeader*)newCodeStart)->_size = blockSize; // fix it
               // Must fix the pointer to the metadata which is stored in the OMR::CodeCacheMethodHeader
               ((OMR::CodeCacheMethodHeader*)newCodeStart)->_metaData = _exceptionTable;
               }
            else
               {
               reloLogger()->maxCodeOrDataSizeWarning();
               _relocationStatus = RelocationCodeCreateError;
               }
            }
         else
            {
            reloLogger()->maxCodeOrDataSizeWarning();
            _relocationStatus = RelocationTableCreateError;
            }
         }
      }
   else
      {
      PORT_ACCESS_FROM_JAVAVM(javaVM());
      j9tty_printf(PORTLIB, "Relocation Error: Failed to find the exception table");
      _relocationStatus = RelocationNoClean;
      }

   if (_relocationStatus == RelocationNoError)
      {
      initializeAotRuntimeInfo();
      relocateAOTCodeAndData(tempDataStart, oldDataStart, newCodeStart, oldCodeStart);
      }

   if (_relocationStatus != RelocationNoError)
      {
      if (_options->getOption(TR_EnableAOTCacheReclamation))
         {
         relocationFailureCleanup();
         }
      else
         {
         _exceptionTable=NULL;
         }
      }

   if (haveReservedCodeCache())
      codeCache()->unreserve();
   return _exceptionTable;
   }

//clean up the allocations we made during the relocation, depending on point of failure
void TR_RelocationRuntime::relocationFailureCleanup()
   {
   if (_relocationStatus == RelocationNoError)
      return;
   switch (_relocationStatus)
      {
      case (RelocationFailure):
         {
         //remove our code cache entry
         _codeCache->addFreeBlock(_exceptionTable);
         }
      case RelocationCodeCreateError:
         {
         //since we don't use this field for compiled copy, don't free it.
         if (!useCompiledCopy() && _exceptionTable->bodyInfo)
            {
            TR_DataCacheManager::getManager()->freeDataCacheRecord(_newPersistentInfo+sizeof(J9JITDataCacheHeader));
            }
         }
      case RelocationPersistentCreateError:
         {
         // I don't have to reclaim the assumptions here
         // When we detect failure, we will do this cleanup anyway
         // getPersistentInfo()->getRuntimeAssumptionTable()->reclaimAssumptions(_exceptionTable);
         }
      case RelocationAssumptionCreateError:
         {
         TR_DataCacheManager::getManager()->freeDataCacheRecord(_exceptionTable);
         }
      case RelocationTableCreateError: // Nothing to do. Fall through
      default:
         {
         _exceptionTable = NULL;
         }
      }
   }

// Function called for AOT method from both JXE and Shared Classes to perform relocations
//
void
TR_RelocationRuntime::relocateAOTCodeAndData(U_8 *tempDataStart,
                                             U_8 *oldDataStart,
                                             U_8 *codeStart,
                                             U_8 *oldCodeStart)
   {
   J9JITDataCacheHeader *cacheEntry = (J9JITDataCacheHeader*)(tempDataStart);
   UDATA startPC = 0;

   RELO_LOG(_reloLogger,
            7,
            "relocateAOTCodeAndData jitConfig=%p aotDataCache=%p aotMccCodeCache=%p method=%p tempDataStart=%p exceptionTable=%p oldDataStart=%p codeStart=%p oldCodeStart=%p classReloAmount=%p cacheEntry=%p\n",
            jitConfig(),
            dataCache(),
            codeCache(),
            _method,
            tempDataStart,
            _exceptionTable,
            oldDataStart,
            codeStart,
            oldCodeStart,
            classReloAmount(),
            cacheEntry
            );

   initializeCacheDeltas();

   _newMethodCodeStart = codeStart;

   reloLogger()->relocationDump();

   if (_exceptionTableCacheEntry->type == J9_JIT_DCE_EXCEPTION_INFO)
      {
      /* Adjust exception table entires */
      _exceptionTable->ramMethod = _method;
      _exceptionTable->constantPool = ramCP();
      getClassNameSignatureFromMethod(_method, _exceptionTable->className, _exceptionTable->methodName, _exceptionTable->methodSignature);
      RELO_LOG(reloLogger(), 1, "relocateAOTCodeAndData: method %.*s.%.*s%.*s\n",
                                    _exceptionTable->className->length,
                                    _exceptionTable->className->data,
                                    _exceptionTable->methodName->length,
                                    _exceptionTable->methodName->data,
                                    _exceptionTable->methodSignature->length,
                                    _exceptionTable->methodSignature->data);

      /* Now it is safe to perform the JITExceptionTable structure relocations */
      relocateMethodMetaData((UDATA)codeStart - (UDATA)oldCodeStart, (UDATA)_exceptionTable - (UDATA)((U_8 *)oldDataStart + _aotMethodHeaderEntry->offsetToExceptionTable + sizeof(J9JITDataCacheHeader)));

      reloTarget()->preRelocationsAppliedEvent();

      /* Perform code relocations */
      if (_aotMethodHeaderEntry->offsetToRelocationDataItems != 0)
         {
         TR_RelocationRecordBinaryTemplate * binaryReloRecords = (TR_RelocationRecordBinaryTemplate * )((U_8 *)_aotMethodHeaderEntry - sizeof(J9JITDataCacheHeader) + _aotMethodHeaderEntry->offsetToRelocationDataItems);
         TR_RelocationRecordGroup reloGroup(binaryReloRecords);

         RELO_LOG(reloLogger(), 6, "relocateAOTCodeAndData: jitConfig=%x aotDataCache=%x aotMccCodeCache=%x method=%x tempDataStart=%x exceptionTable=%x\n", jitConfig(), dataCache(), codeCache(), _method, tempDataStart, _exceptionTable);
         RELO_LOG(reloLogger(), 6, "                        oldDataStart=%x codeStart=%x oldCodeStart=%x classReloAmount=%x cacheEntry=%x\n", oldDataStart, codeStart, oldCodeStart, classReloAmount(), cacheEntry);
         RELO_LOG(reloLogger(), 6, "                        tempDataStart: %p, _aotMethodHeaderEntry: %p, header offset: %x, binaryReloRecords: %p\n", tempDataStart, _aotMethodHeaderEntry, (UDATA)_aotMethodHeaderEntry-(UDATA)tempDataStart, binaryReloRecords);

         try
            {
            _returnCode = reloGroup.applyRelocations(this, reloTarget(), newMethodCodeStart() + codeCacheDelta());
            }
         catch (...)
            {
            _returnCode = compilationAotClassReloFailure;
            }

         RELO_LOG(reloLogger(), 6, "relocateAOTCodeAndData: return code %d\n", _returnCode);

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
         // Detect some potential incorrectness that could otherwise be missed
         if ((getNumValidations() > 10) && (getNumFailedValidations() > getNumValidations() / 2))
            {
            if (aotStats())
               aotStats()->failedPerfAssumptionCode = tooManyFailedValidations;
            //TR_ASSERT(0, "AOT failed too many validations");
            }
         if ((getNumInlinedMethodRelos() > 10) && (getNumFailedInlinedMethodRelos() > getNumInlinedMethodRelos() / 2))
            {
            if (aotStats())
               aotStats()->failedPerfAssumptionCode = tooManyFailedInlinedMethodRelos;
            //TR_ASSERT(0, "AOT failed too many inlined method relos");
            }
         if ((getNumInlinedAllocRelos() > 10) && (getNumFailedAllocInlinedRelos() > getNumInlinedAllocRelos() / 2))
            {
            if (aotStats())
               aotStats()->failedPerfAssumptionCode = tooManyFailedInlinedAllocRelos;
            //TR_ASSERT(0, "AOT failed too many inlined alloc relos");
            }
#endif

         if (_returnCode != 0)
            {
            //clean up code cache
            _relocationStatus = RelocationFailure;
            return;
            }
         }

#if 0
      // add this in later...
      /* Perform meta-data relocations */
      if (_aotMethodHeaderEntry->offsetToMetaDataRelocations != 0)
         {
         TR_RelocationRecordBinaryTemplate * binaryReloRecords = (TR_RelocationRecordBinaryTemplate * )((U_8 *)_aotMethodHeaderEntry - sizeof(J9JITDataCacheHeader) + _aotMethodHeaderEntry->offsetToMetaDataRelocationItems);
         TR_RelocationRecordGroup reloGroup(binaryReloRecords);
         int rc = reloGroup.applyRelocations(this, reloTarget, _exceptionTable);
         if (rc != 0)
            {
            _relocationStatus = RelocationFailure;
            return;
            }
         }
#endif

      reloTarget()->flushCache(codeStart, _aotMethodHeaderEntry->compileMethodCodeSize);

#if 1
      // replace this with meta-data relocations above when we implement it

      /* Fix up inlined exception table ram method entries if wide */
      if (((UDATA)_exceptionTable->numExcptionRanges) & J9_JIT_METADATA_WIDE_EXCEPTIONS)
         {
         UDATA numExcptionRanges = ((UDATA)_exceptionTable->numExcptionRanges) & 0x7fff;

         /* 4 byte exception range entries */
         J9JIT32BitExceptionTableEntry *excptEntry32 = (J9JIT32BitExceptionTableEntry *)(_exceptionTable + 1);
         while (numExcptionRanges > 0)
            {
            J9Method *actualMethod = _method;
            UDATA inlinedSiteIndex = (UDATA)excptEntry32->ramMethod;
            if (inlinedSiteIndex != (UDATA)-1)
               {
               TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(_exceptionTable, (int)inlinedSiteIndex);
               actualMethod = (J9Method *) inlinedCallSite->_methodInfo;
               }
            excptEntry32->ramMethod = actualMethod;


            //excptEntry32->ramMethod = _method;
            excptEntry32++;
            numExcptionRanges--;
            }
         }
#endif

      // Fix RAM method and send target AFTER all relocations are complete.
      startPC = _exceptionTable->startPC;
      } //end if J9_JIT_DCE_EXCEPTION_INFO

   if (startPC)
      {
      // insert exceptionTable into JIT artifacts avl tree under mutex
         {
         TR_TranslationArtifactManager::CriticalSection updateMetaData;

         jit_artifact_insert(javaVM()->portLibrary, jitConfig()->translationArtifacts, _exceptionTable);

         // Fix up RAM method
         TR::CompilationInfo::setJ9MethodExtra(_method, startPC);

         // Return the send target
         _method->methodRunAddress = jitConfig()->i2jTransition;

         // Test for anonymous classes
         J9Class *j9clazz = ramCP()->ramClass;
         if (fej9()->isAnonymousClass((TR_OpaqueClassBlock*)j9clazz))
            {
            J9CLASS_EXTENDED_FLAGS_SET(j9clazz, J9ClassContainsJittedMethods);
            _exceptionTable->prevMethod = NULL;
            _exceptionTable->nextMethod = j9clazz->jitMetaDataList;
            if (j9clazz->jitMetaDataList)
               j9clazz->jitMetaDataList->prevMethod = _exceptionTable;
            j9clazz->jitMetaDataList = _exceptionTable;
            }
         else
            {
            J9ClassLoader * classLoader = j9clazz->classLoader;
            classLoader->flags |= J9CLASSLOADER_CONTAINS_JITTED_METHODS;
            _exceptionTable->prevMethod = NULL;
            _exceptionTable->nextMethod = classLoader->jitMetaDataList;
            if (classLoader->jitMetaDataList)
               classLoader->jitMetaDataList->prevMethod = _exceptionTable;
            classLoader->jitMetaDataList = _exceptionTable;
            }
         }

      reloLogger()->relocationTime();
      }
   }

// This whole function can be dealt with more easily by meta-data relocations rather than this specialized function
//   but leave it here for now
void
TR_RelocationRuntime::relocateMethodMetaData(UDATA codeRelocationAmount, UDATA dataRelocationAmount)
   {
   #if 0
      J9UTF8 * name;
      fprintf(stdout, "relocating ROM method %p ", _exceptionTable->ramMethod);
   #endif

   reloLogger()->metaData();

   _exceptionTable->startPC = (UDATA) ( ((U_8 *)_exceptionTable->startPC) + codeRelocationAmount);
   _exceptionTable->endPC = (UDATA) ( ((U_8 *)_exceptionTable->endPC) + codeRelocationAmount);
   _exceptionTable->endWarmPC = (UDATA) ( ((U_8 *)_exceptionTable->endWarmPC) + codeRelocationAmount);
   if (_exceptionTable->startColdPC)
      _exceptionTable->startColdPC = (UDATA) ( ((U_8 *)_exceptionTable->startColdPC) + codeRelocationAmount);

   _exceptionTable->codeCacheAlloc = (UDATA) ( ((U_8 *)_exceptionTable->codeCacheAlloc) + codeRelocationAmount);

   if (_exceptionTable->gcStackAtlas)
      {
      bool relocateStackAtlasFirst = classReloAmount() != 0;

      if (relocateStackAtlasFirst)
         _exceptionTable->gcStackAtlas = (void *)( ((U_8 *)_exceptionTable->gcStackAtlas) + dataRelocationAmount);

      J9JITStackAtlas *vmAtlas = (J9JITStackAtlas*)_exceptionTable->gcStackAtlas;
      if (vmAtlas->internalPointerMap)
         {
         vmAtlas->internalPointerMap = (U_8 *)( ((U_8 *)vmAtlas->internalPointerMap) + dataRelocationAmount);
         }

      if (vmAtlas->stackAllocMap)
         {
         vmAtlas->stackAllocMap = (U_8 *)( ((U_8 *)vmAtlas->stackAllocMap) + dataRelocationAmount);
         }

      if (!relocateStackAtlasFirst)
         _exceptionTable->gcStackAtlas = (void *)( ((U_8 *)_exceptionTable->gcStackAtlas) + dataRelocationAmount);
      }

   // Believe this will eventually be handled via relocations
   if (_exceptionTable->inlinedCalls)
      {
      U_32 numInlinedCallSites;
      _exceptionTable->inlinedCalls = (void *) (((U_8 *)_exceptionTable->inlinedCalls) + dataRelocationAmount);
      numInlinedCallSites = getNumInlinedCallSites(_exceptionTable);
      /* FIXME: methodInfo is not correctly fixed up.
      if (classReloAmount() && numInlinedCallSites)
         {
         U_8 * callSiteCursor = _exceptionTable->inlinedCalls;
         for (i = 0; i < numInlinedCallSites; ++i)
            {
            TR_InlinedCallSite * inlinedCallSite = (TR_InlinedCallSite *)callSiteCursor;
            inlinedCallSite->_methodInfo = (void *)((U_8 *)inlinedCallSite->_methodInfo + classReloAmount());
            callSiteCursor += sizeOfInlinedCallSiteArrayElement(_exceptionTable);
            }
         }
      */
      }

   if (_exceptionTable->bodyInfo && !useCompiledCopy())
      {
      TR_PersistentJittedBodyInfo *persistentBodyInfo = reinterpret_cast<TR_PersistentJittedBodyInfo *>( (_newPersistentInfo + sizeof(J9JITDataCacheHeader) ) );
      TR_PersistentMethodInfo *persistentMethodInfo = reinterpret_cast<TR_PersistentMethodInfo *>( (_newPersistentInfo + sizeof(J9JITDataCacheHeader) ) + sizeof(TR_PersistentJittedBodyInfo) );
      persistentBodyInfo->setMethodInfo(persistentMethodInfo);
      _exceptionTable->bodyInfo = (void *)(persistentBodyInfo);
      }

   if (getPersistentInfo()->isRuntimeInstrumentationEnabled() &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableHardwareProfileIndirectDispatch) &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableMetadataBytecodePCToIAMap) &&
       _exceptionTable->riData)
      {
      _exceptionTable->riData = (void *) (((U_8 *)_exceptionTable->riData) + dataRelocationAmount);
      }

   #if 0
      fprintf(stdout, "-> %p", _exceptionTable->ramMethod);
      if (classReloAmount())
         {
         name = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(((J9ROMMethod *)_exceptionTable->ramMethod))->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(((J9ROMMethod *)_exceptionTable->ramMethod)));
         fprintf(stdout, " (%.*s)", name->length, name->data);
         }
      fprintf(stdout, "\n");
      fflush(stdout);
   #endif
   }



bool
TR_RelocationRuntime::aotMethodHeaderVersionsMatch()
   {
   if ((_aotMethodHeaderEntry->majorVersion != TR_AOTMethodHeader_MajorVersion) ||
       (_aotMethodHeaderEntry->minorVersion != TR_AOTMethodHeader_MinorVersion))
      {
      reloLogger()->versionMismatchWarning();
      return false;
      }
   return true;
   }


bool
TR_RelocationRuntime::storeAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe, J9VMThread *curThread)
   {
   TR_ASSERT(0, "Error: storeAOTHeader not supported in this relocation runtime");
   return false;
   }

TR_AOTHeader *
TR_RelocationRuntime::createAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe)
   {
   TR_ASSERT(0, "Error: createAOTHeader not supported in this relocation runtime");
   return NULL;
   }

bool
TR_RelocationRuntime::validateAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe, J9VMThread *curThread)
   {
   TR_ASSERT(0, "Error: validateAOTHeader not supported in this relocation runtime");
   return false;
   }

void *
TR_RelocationRuntime::isROMClassInSharedCaches(UDATA romClassValue, J9JavaVM *pjavaVM)
   {
   TR_ASSERT(0, "Error: isROMClassInSharedCaches not supported in this relocation runtime");
   return NULL;
   }

bool
TR_RelocationRuntime::isRomClassForMethodInSharedCache(J9Method *method, J9JavaVM *pjavaVM)
   {
   TR_ASSERT(0, "Error: isRomClassForMethodInSharedCache not supported in this relocation runtime");
   return false;
   }

TR_YesNoMaybe
TR_RelocationRuntime::isMethodInSharedCache(J9Method *method, J9JavaVM *pjavaVM)
   {
   TR_ASSERT(0, "Error: isMethodInSharedCache not supported in this relocation runtime");
   return TR_no;
   }

TR_OpaqueClassBlock *
TR_RelocationRuntime::getClassFromCP(J9VMThread *vmThread, J9JavaVM *pjavaVM, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic)
   {
   TR_ASSERT(0, "Error: getClassFromCP not supported in this relocation runtime");
   return NULL;
   }

bool TR_RelocationRuntime::_globalValuesInitialized=false;

uintptr_t TR_RelocationRuntime::_globalValueList[TR_NumGlobalValueItems] =
   {
   0,          // not used
   0,          // TR_CountForRecompile
   0,          // TR_HeapBase
   0,          // TR_HeapTop
   0,          // TR_HeapBaseForBarrierRange0
   0,          // TR_ActiveCardTableBase
   0          // TR_HeapSizeForBarrierRange0
   };

char *TR_RelocationRuntime::_globalValueNames[TR_NumGlobalValueItems] =
   {
   "not used (0)",
   "TR_CountForRecompile (1)",
   "TR_HeapBase (2)",
   "TR_HeapTop (3)",
   "TR_HeapBaseForBarrierRange0 (4)",
   "TR_ActiveCardTableBase (5)",
   "TR_HeapSizeForBarrierRange0 (6)"
   };



#if defined(J9VM_OPT_SHARED_CLASSES)

//
// TR_SharedCacheRelocationRuntime
//

const char TR_SharedCacheRelocationRuntime::aotHeaderKey[] = "J9AOTHeader";
// When we write out the header, we don't seem to include the \0 character at the end of the string.
const UDATA TR_SharedCacheRelocationRuntime::aotHeaderKeyLength = sizeof(TR_SharedCacheRelocationRuntime::aotHeaderKey) - 1;

U_8 *
TR_SharedCacheRelocationRuntime::allocateSpaceInCodeCache(UDATA codeSize)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();

   int32_t compThreadID = fej9->getCompThreadIDForVMThread(_currentThread);
   if (!codeCache())
      {
      int32_t numReserved;

      _codeCache = manager->reserveCodeCache(false, codeSize, compThreadID, &numReserved);  // Acquire a cold/warm code cache.
      if (!codeCache())
         {
         // TODO: how do we pass back error codes to trigger retrial?
         //if (numReserved >= 1) // We could still get some code space in caches that are currently reserved
         //    *returnCode = compilationCodeReservationFailure; // this will promp a retrial
         return NULL;
         }
       // The GC may unload classes if code caches have been switched

      if (compThreadID >= 0 && fej9->getCompilationShouldBeInterruptedFlag())
         {
         codeCache()->unreserve(); // cancel the reservation
         //*returnCode = compilationInterrupted; // allow retrial //FIXME: how do we pass error codes?
         return NULL; // fail this AOT load
         }
      _haveReservedCodeCache = true;
      }

   uint8_t *coldCode;
   U_8 *codeStart = manager->allocateCodeMemory(codeSize, 0, &_codeCache, &coldCode, false);
   // FIXME: the GC may unload classes if code caches have been switched
   if (compThreadID >= 0 && fej9->getCompilationShouldBeInterruptedFlag())
      {
      codeCache()->unreserve(); // cancel the reservation
      _haveReservedCodeCache = false;
      //*returnCode = compilationInterrupted; // allow retrial
      return NULL; // fail this AOT load
      }
   return codeStart;
   }


// TODO: why do shared cache and JXE paths manage alignment differently?
//       main reason why there are two implementations here
uint8_t *
TR_SharedCacheRelocationRuntime::allocateSpaceInDataCache(uintptr_t metaDataSize,
                                                          uintptr_t type)
   {
   // Ensure data cache is aligned
   _metaDataAllocSize = TR_DataCacheManager::alignToMachineWord(metaDataSize);
   U_8 *newDataStart = TR_DataCacheManager::getManager()->allocateDataCacheRecord(_metaDataAllocSize, type, 0);
   if (newDataStart)
      newDataStart -= sizeof(J9JITDataCacheHeader);
   return newDataStart;
   }


void
TR_SharedCacheRelocationRuntime::initializeAotRuntimeInfo()
   {
   _baseAddress = (U_8 *) javaVM()->sharedClassConfig->cacheDescriptorList->romclassStartAddress;
   _compileFirstClassLocation = _aotMethodHeaderEntry->compileFirstClassLocation;
   if (!useCompiledCopy())
      _classReloAmount = 1;
   }


void
TR_SharedCacheRelocationRuntime::initializeCacheDeltas()
   {
   _dataCacheDelta = 0;
   _codeCacheDelta = 0;
   }


bool
TR_SharedCacheRelocationRuntime::useDFPHardware(TR_FrontEnd *fe)
   {
   TR::Options  *options = TR::Options::getCmdLineOptions();
   bool dfpbd = options->getOption(TR_DisableHysteresis);
   bool nodfpbd =  options->getOption(TR_DisableDFP);
   bool isPOWERDFP = TR::Compiler->target.cpu.isPower() && TR::Compiler->target.cpu.supportsDecimalFloatingPoint();
   bool is390DFP =
#ifdef TR_TARGET_S390
      TR::Compiler->target.cpu.isZ() && TR::Compiler->target.cpu.getSupportsDecimalFloatingPointFacility();
#else
      false;
#endif
   if ((isPOWERDFP || is390DFP) && ((!dfpbd && !nodfpbd) || dfpbd))
      {
      return true;
      }
   return false;
   }


void
TR_SharedCacheRelocationRuntime::incompatibleCache(U_32 module_name, U_32 reason, char *assumeMessage)
   {
   if (TR::Options::isAnyVerboseOptionSet())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "%s\n", assumeMessage);
      }

   if (javaVM()->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
      {
      PORT_ACCESS_FROM_JAVAVM(javaVM());
      j9nls_printf(PORTLIB, (UDATA) J9NLS_WARNING, module_name, reason);
      }
   }

bool
TR_SharedCacheRelocationRuntime::generateError(char *assumeMessage)
   {
   incompatibleCache(J9NLS_RELOCATABLE_CODE_WRONG_HARDWARE, assumeMessage);
   return false;
   }

void
TR_SharedCacheRelocationRuntime::checkAOTHeaderFlags(TR_FrontEnd *fe, TR_AOTHeader *hdrInCache, intptr_t featureFlags)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   bool defaultMessage = true;

   if (!TR::Compiler->target.cpu.isCompatible((TR_Processor)hdrInCache->processorSignature, hdrInCache->processorFeatureFlags))
      defaultMessage = generateError("AOT header validation failed: Processor incompatible.");
   if ((featureFlags & TR_FeatureFlag_sanityCheckBegin) != (hdrInCache->featureFlags & TR_FeatureFlag_sanityCheckBegin))
      defaultMessage = generateError("AOT header validation failed: Processor feature sanity bit mangled.");
   if ((featureFlags & TR_FeatureFlag_IsSMP) != (hdrInCache->featureFlags & TR_FeatureFlag_IsSMP))
      defaultMessage = generateError("AOT header validation failed: SMP feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_UsesCompressedPointers) != (hdrInCache->featureFlags & TR_FeatureFlag_UsesCompressedPointers))
      defaultMessage = generateError("AOT header validation failed: Compressed references feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_UseDFPHardware) != (hdrInCache->featureFlags & TR_FeatureFlag_UseDFPHardware))
      defaultMessage = generateError("AOT header validation failed: DFP hardware feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_DisableTraps) != (hdrInCache->featureFlags & TR_FeatureFlag_DisableTraps))
      defaultMessage = generateError("AOT header validation failed: Use of trap instruction feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_TLHPrefetch) != (hdrInCache->featureFlags & TR_FeatureFlag_TLHPrefetch))
      defaultMessage = generateError("AOT header validation failed: TLH prefetch feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_MethodTrampolines) != (hdrInCache->featureFlags & TR_FeatureFlag_MethodTrampolines))
      defaultMessage = generateError("AOT header validation failed: MethodTrampolines feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_MultiTenancy) != (hdrInCache->featureFlags & TR_FeatureFlag_MultiTenancy))
      defaultMessage = generateError("AOT header validation failed: MultiTenancy feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_HCREnabled) != (hdrInCache->featureFlags & TR_FeatureFlag_HCREnabled))
      defaultMessage = generateError("AOT header validation failed: HCR feature mismatch.");
   if (((featureFlags & TR_FeatureFlag_SIMDEnabled) == 0) && ((hdrInCache->featureFlags & TR_FeatureFlag_SIMDEnabled) != 0))
      defaultMessage = generateError("AOT header validation failed: SIMD feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_AsyncCompilation) != (hdrInCache->featureFlags & TR_FeatureFlag_AsyncCompilation))
      defaultMessage = generateError("AOT header validation failed: AsyncCompilation feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_ConcurrentScavenge) != (hdrInCache->featureFlags & TR_FeatureFlag_ConcurrentScavenge))
      defaultMessage = generateError("AOT header validation failed: Concurrent Scavenge feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_SoftwareReadBarrier) != (hdrInCache->featureFlags & TR_FeatureFlag_SoftwareReadBarrier))
      defaultMessage = generateError("AOT header validation failed: Software Read Barrier feature mismatch.");
   if ((featureFlags & TR_FeatureFlag_UsesTM) != (hdrInCache->featureFlags & TR_FeatureFlag_UsesTM))
      defaultMessage = generateError("AOT header validation failed: TM feature mismatch.");

   if ((featureFlags & TR_FeatureFlag_SanityCheckEnd) != (hdrInCache->featureFlags & TR_FeatureFlag_SanityCheckEnd))
      defaultMessage = generateError("AOT header validation failed: Trailing sanity bit mismatch.");

   if (defaultMessage)
      generateError("AOT header validation failed: Unkown problem with processor features.");
   }

// The method CS::Hash_FNV is being used to compute the hash value
// Notice that currently CS2::Hash_FNV is a hash function that never returns 0
uint32_t
TR_SharedCacheRelocationRuntime::getCurrentLockwordOptionHashValue(J9JavaVM *vm) const
   {
   IDATA currentLockwordArgIndex = FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, "-Xlockword", NULL);
   uint32_t currentLockwordOptionHashValue = 0;
   if (currentLockwordArgIndex >= 0)
      {
      char * currentLockwordOption = NULL;
      GET_OPTION_VALUE(currentLockwordArgIndex, ':', &currentLockwordOption);
      currentLockwordOptionHashValue = CS2::Hash_FNV((unsigned char*)currentLockwordOption, strlen(currentLockwordOption));
      }
   return currentLockwordOptionHashValue;
   }

// This function currently does not rely on the object beyond the v-table override (compiled as static without any problems).
// If this changes, we will need to look further into whether its users risk concurrent access.
bool
TR_SharedCacheRelocationRuntime::validateAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe, J9VMThread *curThread)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   /* Look for an AOT header in the cache and see if this JVM is compatible */

   J9SharedDataDescriptor firstDescriptor;
   firstDescriptor.address = NULL;
   javaVM()->sharedClassConfig->findSharedData(curThread,
                                             aotHeaderKey,
                                             aotHeaderKeyLength,
                                             J9SHR_DATA_TYPE_AOTHEADER,
                                             FALSE,
                                             &firstDescriptor,
                                             NULL);

   const void* result = firstDescriptor.address;
   if (result)
      {
      /* check compatibility */
      TR_AOTHeader * hdrInCache = (TR_AOTHeader * )result;

      intptr_t featureFlags = generateFeatureFlags(fe);

      TR_Version currentVersion;
      memset(&currentVersion, 0, sizeof(TR_Version));
      currentVersion.structSize = sizeof(TR_Version);
      currentVersion.majorVersion = TR_AOTHeaderMajorVersion;
      currentVersion.minorVersion = TR_AOTHeaderMinorVersion;
      strncpy(currentVersion.vmBuildVersion, EsBuildVersionString, std::min(strlen(EsBuildVersionString), sizeof(currentVersion.vmBuildVersion) - 1));
      strncpy(currentVersion.jitBuildVersion, TR_BUILD_NAME, std::min(strlen(TR_BUILD_NAME), sizeof(currentVersion.jitBuildVersion) - 1));

      if (hdrInCache->eyeCatcher != TR_AOTHeaderEyeCatcher ||
          currentVersion.structSize != hdrInCache->version.structSize ||
          memcmp(&currentVersion, &hdrInCache->version, sizeof(TR_Version)))
         {
         incompatibleCache(J9NLS_RELOCATABLE_CODE_WRONG_JVM_VERSION,
                           "AOT header validation failed: bad header version or version string");
         }
      else if
         (hdrInCache->featureFlags != featureFlags ||
          !TR::Compiler->target.cpu.isCompatible((TR_Processor)hdrInCache->processorSignature, hdrInCache->processorFeatureFlags)
         )
         {
         checkAOTHeaderFlags(fe, hdrInCache, featureFlags);
         }
      else if ( hdrInCache->gcPolicyFlag != javaVM()->memoryManagerFunctions->j9gc_modron_getWriteBarrierType(javaVM()) )
         {
         incompatibleCache(J9NLS_RELOCATABLE_CODE_WRONG_GC_POLICY,
                           "AOT header validation failed: incompatible gc write barrier type");
         }
      else if (hdrInCache->lockwordOptionHashValue != getCurrentLockwordOptionHashValue(pjavaVM))
         {
         incompatibleCache(J9NLS_RELOCATABLE_CODE_PROCESSING_COMPATIBILITY_FAILURE,
                           "AOT header validation failed: incompatible lockword options");
         }
      else if (hdrInCache->arrayLetLeafSize != TR::Compiler->om.arrayletLeafSize())
         {
         incompatibleCache(J9NLS_RELOCATABLE_CODE_PROCESSING_COMPATIBILITY_FAILURE,
                           "AOT header validation failed: incompatible arraylet size");
         }
#if defined(OMR_GC_COMPRESSED_POINTERS)
      else if ( hdrInCache->compressedPointerShift != TR::Compiler->om.compressedReferenceShift())
         {
         incompatibleCache(J9NLS_RELOCATABLE_CODE_PROCESSING_COMPATIBILITY_FAILURE,
                           "AOT header validation failed: incompatible compressed pointer shift");
         }
#endif // OMR_GC_COMPRESSED_POINTERS
      else
         {
         static_cast<TR_JitPrivateConfig *>(jitConfig()->privateConfig)->aotValidHeader = TR_yes;
         return true;
         }

      // not compatible, so stop looking and don't compile anything for cache
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT);
      static_cast<TR_JitPrivateConfig *>(jitConfig()->privateConfig)->aotValidHeader = TR_no;
      TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_HEADER_INVALID);

      // Generate a trace point
      Trc_JIT_IncompatibleAOTHeader(curThread);

      return false;
      }
   else // cannot find AOT header
      {
      // Leaving TR_maybe will allow the store of a header later on
      // static_cast<TR_JitPrivateConfig *>(jitConfig()->privateConfig)->aotValidHeader = TR_maybe;
      return false;
      }
   }

TR_AOTHeader *
TR_SharedCacheRelocationRuntime::createAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM());

   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   TR_AOTHeader * aotHeader = (TR_AOTHeader *)j9mem_allocate_memory(sizeof(TR_AOTHeader), J9MEM_CATEGORY_JIT);

   if (aotHeader)
      {
      aotHeader->eyeCatcher = TR_AOTHeaderEyeCatcher;

      TR_Version *aotHeaderVersion = &aotHeader->version;
      memset(aotHeaderVersion, 0, sizeof(TR_Version));
      aotHeaderVersion->structSize = sizeof(TR_Version);
      aotHeaderVersion->majorVersion = TR_AOTHeaderMajorVersion;
      aotHeaderVersion->minorVersion = TR_AOTHeaderMinorVersion;
      strncpy(aotHeaderVersion->vmBuildVersion, EsBuildVersionString, sizeof(EsBuildVersionString));
      strncpy(aotHeaderVersion->jitBuildVersion, TR_BUILD_NAME, std::min(strlen(TR_BUILD_NAME), sizeof(aotHeaderVersion->jitBuildVersion)));

      aotHeader->processorSignature = TR::Compiler->target.cpu.id();
      aotHeader->gcPolicyFlag = javaVM()->memoryManagerFunctions->j9gc_modron_getWriteBarrierType(javaVM());
      aotHeader->lockwordOptionHashValue = getCurrentLockwordOptionHashValue(pjavaVM);
#if defined(OMR_GC_COMPRESSED_POINTERS)
      aotHeader->compressedPointerShift = javaVM()->memoryManagerFunctions->j9gc_objaccess_compressedPointersShift(javaVM()->internalVMFunctions->currentVMThread(javaVM()));
#else
      aotHeader->compressedPointerShift = 0;
#endif

      aotHeader->processorFeatureFlags = TR::Compiler->target.cpu.getProcessorFeatureFlags();

      // Set up other feature flags
      aotHeader->featureFlags = generateFeatureFlags(fe);

      // Set ArrayLet Size if supported
      aotHeader->arrayLetLeafSize = TR::Compiler->om.arrayletLeafSize();
      }

   return aotHeader;
   }

bool
TR_SharedCacheRelocationRuntime::storeAOTHeader(J9JavaVM *pjavaVM, TR_FrontEnd *fe, J9VMThread *curThread)
   {

   TR_AOTHeader *aotHeader = createAOTHeader(javaVM(), fe);
   if (!aotHeader)
      {
      PORT_ACCESS_FROM_JAVAVM(javaVM());
      if (javaVM()->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
         j9nls_printf( PORTLIB, J9NLS_WARNING,  J9NLS_RELOCATABLE_CODE_PROCESSING_COMPATIBILITY_FAILURE );
      TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_HEADER_FAILED_TO_ALLOCATE);
      return false;
      }

   J9SharedDataDescriptor dataDescriptor;
   UDATA aotHeaderLen = sizeof(TR_AOTHeader);

   dataDescriptor.address = (U_8*)aotHeader;
   dataDescriptor.length = aotHeaderLen;
   dataDescriptor.type =  J9SHR_DATA_TYPE_AOTHEADER;
   dataDescriptor.flags = J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE;

   const void* store = javaVM()->sharedClassConfig->storeSharedData(curThread,
                                                                  aotHeaderKey,
                                                                  aotHeaderKeyLength,
                                                                  &dataDescriptor);
   if (store)
      {
      // If a header already exists, the old one is returned
      // Thus, we must check the validity of the header
      return validateAOTHeader(javaVM(), fe, curThread);
      }
   else
      {
      // The store failed for some odd reason; maybe the cache is full
      // Let's prevent any further store operations to avoid overhead
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);

      TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_HEADER_STORE_FAILED);
      TR_J9SharedCache::setStoreSharedDataFailedLength(aotHeaderLen);
      return false;
      }
   }


void *
TR_SharedCacheRelocationRuntime::isROMClassInSharedCaches(UDATA romClassValue, J9JavaVM *pjavaVM)
   {
   j9thread_monitor_enter(javaVM()->sharedClassConfig->configMonitor);
   J9SharedClassCacheDescriptor *currentCacheDescriptor = javaVM()->sharedClassConfig->cacheDescriptorList;
   bool matchFound = false;

   while (!matchFound && currentCacheDescriptor)
      {
      //printf("currentCacheDescriptor: %p, currentCacheDescriptor->cacheStartAddress: %p, cacheSizeBytes: %x, romClassStartAddress: %p, romClassValue :%p, descriptor->next: %p\n", currentCacheDescriptor, currentCacheDescriptor->cacheStartAddress, currentCacheDescriptor->cacheSizeBytes, currentCacheDescriptor->romclassStartAddress, romClassValue, currentCacheDescriptor->next);
      //if (((romClassValue < (UDATA)currentCacheDescriptor->romclassStartAddress + currentCacheDescriptor->cacheSizeBytes)&& (romClassValue >= (UDATA)currentCacheDescriptor->romclassStartAddress))) // Temporarily use romclassStartAddress because cachStartAddress is NULL for new cache
      if (((romClassValue < (UDATA)currentCacheDescriptor->metadataStartAddress)&& (romClassValue >= (UDATA)currentCacheDescriptor->romclassStartAddress)))
         {
         matchFound = true;
         break;
         }
      if (currentCacheDescriptor->next == javaVM()->sharedClassConfig->cacheDescriptorList)
         break; // Since list is circular, break if we are about to loop back
      currentCacheDescriptor = currentCacheDescriptor->next;
      }
   j9thread_monitor_exit(javaVM()->sharedClassConfig->configMonitor);
   if (matchFound)
      {
      return (void *)currentCacheDescriptor;
      }
   else
      {
      return NULL;
      }
   }

bool
TR_SharedCacheRelocationRuntime::isRomClassForMethodInSharedCache(J9Method *method, J9JavaVM *pjavaVM)
   {
#if 1
   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;
#if 0
   j9thread_monitor_enter(javaVM()->sharedClassConfig->configMonitor);
   UDATA cacheEnd = (UDATA)javaVM()->sharedClassConfig->cacheDescriptorList->cacheStartAddress + javaVM()->sharedClassConfig->cacheDescriptorList->cacheSizeBytes;
   bool isRomClassForMethodInSharedCache = (((UDATA)romClass >= (UDATA)javaVM()->sharedClassConfig->cacheDescriptorList->cacheStartAddress) && ((UDATA)romClass <= cacheEnd));
   j9thread_monitor_exit(javaVM()->sharedClassConfig->configMonitor);
   return isRomClassForMethodInSharedCache;
#else
   bool isRomClassForMethodInSharedCache = isROMClassInSharedCaches((UDATA)romClass, javaVM()) ? true : false;
   return isRomClassForMethodInSharedCache;
#endif
#endif
   }

TR_YesNoMaybe
TR_SharedCacheRelocationRuntime::isMethodInSharedCache(J9Method *method, J9JavaVM *pjavaVM)
   {
   if (isRomClassForMethodInSharedCache(method, javaVM()))
      return TR_maybe;
   else
      return TR_no;
   }

TR_OpaqueClassBlock *
TR_SharedCacheRelocationRuntime::getClassFromCP(J9VMThread *vmThread, J9JavaVM *pjavaVM, J9ConstantPool *constantPool, I_32 cpIndex, bool isStatic)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
   TR::VMAccessCriticalSection getClassFromCP(fe);
   /* Get the class.  Stop immediately if an exception occurs. */
   J9ROMFieldRef *romFieldRef = (J9ROMFieldRef *)&constantPool->romConstantPool[cpIndex];

   J9Class *resolvedClass = javaVM()->internalVMFunctions->resolveClassRef(vmThread, constantPool, romFieldRef->classRefCPIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);

   if (resolvedClass == NULL)
      return NULL;

   J9Class *classFromCP = J9_CLASS_FROM_CP(constantPool);
   J9ROMFieldShape *field;
   J9Class *definingClass;
   J9ROMNameAndSignature *nameAndSig;
   J9UTF8 *name;
   J9UTF8 *signature;

   nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
   name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
   signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
   if (isStatic)
      {
      void *staticAddress = javaVM()->internalVMFunctions->staticFieldAddress(vmThread, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, J9_LOOK_NO_JAVA, classFromCP);
      }
   else
      {
      IDATA fieldOffset = javaVM()->internalVMFunctions->instanceFieldOffset(vmThread, resolvedClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, J9_LOOK_NO_JAVA);
      }
   return (TR_OpaqueClassBlock *)definingClass;
   }

uintptr_t
TR_SharedCacheRelocationRuntime::generateFeatureFlags(TR_FrontEnd *fe)
   {
   uintptr_t featureFlags = 0;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   featureFlags |= TR_FeatureFlag_sanityCheckBegin;

   if (TR::Compiler->target.isSMP())
      featureFlags |= TR_FeatureFlag_IsSMP;

   if (TR::Options::useCompressedPointers())
      featureFlags |= TR_FeatureFlag_UsesCompressedPointers;

   if (useDFPHardware(fe))
      featureFlags |= TR_FeatureFlag_UseDFPHardware;

   if (TR::Options::getCmdLineOptions()->getOption(TR_DisableTraps))
      featureFlags |= TR_FeatureFlag_DisableTraps;

   if (TR::Options::getCmdLineOptions()->getOption(TR_TLHPrefetch))
      featureFlags |= TR_FeatureFlag_TLHPrefetch;

   if (TR::CodeCacheManager::instance()->codeCacheConfig().needsMethodTrampolines())
      featureFlags |= TR_FeatureFlag_MethodTrampolines;

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
      featureFlags |= TR_FeatureFlag_HCREnabled;

#ifdef TR_TARGET_S390
   if (TR::Compiler->target.cpu.getSupportsVectorFacility())
      featureFlags |= TR_FeatureFlag_SIMDEnabled;
#endif

   if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none)
      {
      featureFlags |= TR_FeatureFlag_ConcurrentScavenge;

#ifdef TR_TARGET_S390
      if (!TR::Compiler->target.cpu.getSupportsGuardedStorageFacility())
         featureFlags |= TR_FeatureFlag_SoftwareReadBarrier;
#endif
      }


   if (fej9->isAsyncCompilation())
      featureFlags |= TR_FeatureFlag_AsyncCompilation;


   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableTM) &&
       !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableTM))
      {
      if (TR::Compiler->target.cpu.supportsTransactionalMemoryInstructions())
         {
         featureFlags |= TR_FeatureFlag_UsesTM;
         }
      }

   return featureFlags;
   }

void TR_RelocationRuntime::initializeHWProfilerRecords(TR::Compilation *comp)
   {
   assert(comp != NULL);
   comp->getHWPBCMap()->clear();
   }

void TR_RelocationRuntime::addClazzRecord(uint8_t *ia, uint32_t bcIndex, TR_OpaqueMethodBlock *method)
   {
   if (getPersistentInfo()->isRuntimeInstrumentationEnabled() && _isLoading)
      {
      comp()->addHWPBCMap(_compInfo->getHWProfiler()->createBCMap(ia,
                                                                  bcIndex,
                                                                  method,
                                                                  comp()));
      }
   }

#endif
