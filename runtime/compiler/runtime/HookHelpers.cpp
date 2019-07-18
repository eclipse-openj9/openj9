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

#include "runtime/HookHelpers.hpp"

#include "avl_api.h"
#include "jithash.h"
#include "j9.h"
#include "util_api.h"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/IO.hpp"
#include "il/DataTypes.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/DataCache.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"

namespace  {

#if defined(RECLAMATION_DEBUG)
   void printUnloadingInformation(J9JITExceptionTable *metaData, const char *message)
      {
      J9UTF8 *className;
      J9UTF8 *name;
      J9UTF8 *signature;
      getClassNameSignatureFromMethod(metaData->ramMethod, className, name, signature);
      fprintf(
         stderr,
         "%s causing reclamation of  %.*s.%.*s%.*s @ %p\n",
         message,
         J9UTF8_LENGTH(className), J9UTF8_DATA(className),
         J9UTF8_LENGTH(name), J9UTF8_DATA(name),
         J9UTF8_LENGTH(signature), J9UTF8_DATA(signature),
         reinterpret_cast<U_8 *>(metaData->startPC)
      );
      }
#endif

   void dispatchUnloadHooks(J9JITConfig *jitConfig, J9VMThread *vmThread, J9JITExceptionTable *metaData)
      {
      J9JavaVM * vm = jitConfig->javaVM;
      OMR::CodeCacheMethodHeader* ccMethodHeader = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);
      if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_DYNAMIC_CODE_UNLOAD))
         {
         ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_UNLOAD
            (vm->hookInterface, vmThread, metaData->ramMethod,
             (U_8*)metaData->startPC);

         if (metaData->startColdPC)
            ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_UNLOAD
               (vm->hookInterface, vmThread, metaData->ramMethod,
                (U_8*)metaData->startColdPC);
         // At this point the the bodyInfo may have already been reclaimed
         // The bodyInfo is reclaimed only for class unloading, not for recompilation
         if (ccMethodHeader && (metaData->bodyInfo != NULL))
            {
            TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get((void *)metaData->startPC);
            if (linkageInfo->isRecompMethodBody())
               {
               ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_UNLOAD(vm->hookInterface, vmThread, metaData->ramMethod, (U_8 *)((char*)ccMethodHeader->_eyeCatcher+4));
               }
            }
         }
      }

   // mark the code cache as not full
   inline void renewCodeCachePessimism(J9VMThread *vmThread)
      {
      if (
         !TR::Options::getCmdLineOptions()->getOption(TR_DisableCodeCacheReclamation)
         && !TR::Options::getCmdLineOptions()->getOption(TR_DisableClearCodeCacheFullFlag)
         )
         {
         // Clear the flag to allow compilation to continue
         vmThread->javaVM->jitConfig->runtimeFlags &= ~J9JIT_CODE_CACHE_FULL;
         }
      }

   inline void markMetaDataAsUnloaded (J9JITExceptionTable *metaData)
      {
      metaData->constantPool = 0;
      }

   inline void reclaimAssumptions(J9JITConfig *jitConfig, J9JITExceptionTable *metaData, bool reclaimPrePrologueAssumptions)
      {
      // expunge the runtime assumptions that exist on this method body
      pointer_cast<TR_PersistentMemory *>( jitConfig->scratchSegment )->getPersistentInfo()->getRuntimeAssumptionTable()->reclaimAssumptions(metaData, reclaimPrePrologueAssumptions);
      }

   inline void markAssumptionsAndDetach(J9JITConfig *jitConfig, J9JITExceptionTable *metaData, bool reclaimPrePrologueAssumptions)
      {
      // mark and detach the runtime assumptions, which will then be deleted from the RAT in the GC end hook
      pointer_cast<TR_PersistentMemory *>( jitConfig->scratchSegment )->getPersistentInfo()->getRuntimeAssumptionTable()->markAssumptionsAndDetach(metaData, reclaimPrePrologueAssumptions);
      }

   inline void reclaimMarkedAssumptionsFromRAT(int cleanUpCount)
      {
      // expunge any runtime assumptions in the RAT that have previously been marked
      pointer_cast<TR_PersistentMemory *>( jitConfig->scratchSegment )->getPersistentInfo()->getRuntimeAssumptionTable()->reclaimMarkedAssumptionsFromRAT(cleanUpCount);
      }
   
   // We need to update the nextMethod and prevMethod pointers of the J9JITExceptionTable that
   // point to the old J9JITExceptionTable to now point to the new (stub) J9JITExceptionTable.
   inline void updateExceptionTableLinkedList(J9VMThread *vmThread, J9JITExceptionTable *oldMetadata, J9JITExceptionTable *newMetadata)
      {
      // newMetaData's prevMethod and nextMethod should be the
      // same as oldMetaData's due to the memcpy
      J9JITExceptionTable *prev = oldMetadata->prevMethod;
      J9JITExceptionTable *next = oldMetadata->nextMethod;

      if (prev)
         {
         prev->nextMethod = newMetadata;
         }
      else
         {
         J9Class *j9class = J9_CLASS_FROM_METHOD(oldMetadata->ramMethod);
         TR_J9VMBase *fe = TR_J9VMBase::get(vmThread->javaVM->jitConfig, NULL);

         if (fe->isAnonymousClass((TR_OpaqueClassBlock *)j9class))
            {
            if (j9class->jitMetaDataList == oldMetadata)
               j9class->jitMetaDataList = newMetadata;
            }
         else
            {
            if (j9class->classLoader->jitMetaDataList == oldMetadata)
               j9class->classLoader->jitMetaDataList = newMetadata;
            }
         }

      if (next)
         next->prevMethod = newMetadata;
      }
   
   inline J9JITExceptionTable * createStubExceptionTable(J9VMThread *vmThread, J9JITExceptionTable *metaData)
      {
      J9JITExceptionTable *stubMetadata = NULL;
      
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableMetadataReclamation))
         {
         uint32_t numBytes = sizeof(J9JITExceptionTable);
         uint32_t size = 0;
         stubMetadata = (J9JITExceptionTable *)TR_DataCacheManager::getManager()->allocateDataCacheRecord(numBytes, J9_JIT_DCE_EXCEPTION_INFO, &size);
         
         if (stubMetadata)
            {
            // Make a copy of just the J9JITExceptionTable Struct (excluding the variable length section)
            memcpy((uint8_t *)stubMetadata, (uint8_t *)metaData, numBytes);
            
            // Set the various pointers to NULL since this J9JITExceptionTable has no variable length section
            // However, the bodyInfo needs to exist.
            stubMetadata->inlinedCalls = NULL;
            stubMetadata->gcStackAtlas = NULL;
            stubMetadata->gpuCode = NULL;
            stubMetadata->riData = NULL;
            stubMetadata->osrInfo = NULL;
            stubMetadata->runtimeAssumptionList = NULL;
            
            // FASTWALK
            freeFastWalkCache(vmThread, stubMetadata);
            
            // Update J9JITExceptionTable Linked List
            updateExceptionTableLinkedList(vmThread, metaData, stubMetadata);
            
            // Set the IS_STUB flag
            stubMetadata->flags |= JIT_METADATA_IS_STUB;
            }
         }
      
      return stubMetadata;
      }
   
}

/*Below is a mirror struct with the one in MethodMetaData.c.*/
/*Please apply to any modification to both methods.*/
#define JIT_EXCEPTION_HANDLER_CACHE_SIZE 256
typedef struct TR_jitExceptionHandlerCache
   {
   UDATA pc;
   J9Class * thrownClass;
   } TR_jitExceptionHandlerCache;

void cleanUpJitExceptionHandlerCache(J9VMThread *vmThread, J9JITExceptionTable *metaData)
   {
   struct J9JavaVM* javaVM=vmThread->javaVM;
   J9VMThread * currentThread = javaVM->mainThread;
   do
      {
      if(currentThread->jitExceptionHandlerCache)
         {
         TR_jitExceptionHandlerCache * searchCache=(TR_jitExceptionHandlerCache *)(currentThread->jitExceptionHandlerCache);
         for(int counter=0;counter<JIT_EXCEPTION_HANDLER_CACHE_SIZE;counter++)
            {
            if((searchCache[counter].pc >= metaData->startPC && searchCache[counter].pc <= metaData->endWarmPC)
               ||(metaData->startColdPC
               && searchCache[counter].pc >= metaData->startColdPC && searchCache[counter].pc <= metaData->endPC))
               {
               searchCache[counter].pc=0;
               }
            }
         }
      }
   while ((currentThread = currentThread->linkNext) != javaVM->mainThread);
   return;
   }

/*Below is a mirror struct with the one in jswalk.c.*/
/*Please apply to any modification to both methods.*/
#define JIT_ARTIFACT_SEARCH_CACHE_SIZE 256
typedef struct TR_jit_artifact_search_cache
   {
      UDATA searchValue;
      J9JITExceptionTable * exceptionTable;
   } TR_jit_artifact_search_cache;

void cleanUpJitArtifactSearchCache(J9VMThread *vmThread, J9JITExceptionTable *metaData)
   {
   struct J9JavaVM* javaVM=vmThread->javaVM;
   J9VMThread * currentThread = javaVM->mainThread;
   do
      {
      if(currentThread->jitArtifactSearchCache)
         {
         TR_jit_artifact_search_cache * searchCache=(TR_jit_artifact_search_cache *)(currentThread->jitArtifactSearchCache);
         for(int counter=0;counter<JIT_ARTIFACT_SEARCH_CACHE_SIZE;counter++)
            {
            if((searchCache[counter].searchValue >= metaData->startPC && searchCache[counter].searchValue <= metaData->endWarmPC)
               ||(metaData->startColdPC
               && searchCache[counter].searchValue >= metaData->startColdPC && searchCache[counter].searchValue <= metaData->endPC))
               {
               searchCache[counter].searchValue=0;
               }
            }
         }
      }
   while ((currentThread = currentThread->linkNext) != javaVM->mainThread);
   return;
   }

void jitReleaseCodeCollectMetaData(J9JITConfig *jitConfig, J9VMThread *vmThread, J9JITExceptionTable *metaData, OMR::FaintCacheBlock *faintCacheBlock)
   {
   static const char *useOldRAReclaim = feGetEnv("TR_useOldRAReclaim");
   TR_TranslationArtifactManager *artifactManager = TR_TranslationArtifactManager::getGlobalArtifactManager();
   bool freeExistingExceptionTable = false;
   
   if (
      !TR::Options::getCmdLineOptions()->getOption(TR_DisableCodeCacheReclamation)
      && artifactManager->containsArtifact(metaData)
      )
      {
      //clean up exception table cache
      cleanUpJitExceptionHandlerCache(vmThread,metaData);
      cleanUpJitArtifactSearchCache(vmThread,metaData);

      if (useOldRAReclaim)
         {
         // 3rd parameter to reclaimAssumptions determines whether we reclaim preprologue assumptions.
         // We should only do that if we are in class unloading and will be reclaiming the entire method body,
         // which means faintCacheBlock is NULL.
         reclaimAssumptions(jitConfig, metaData, (NULL == faintCacheBlock));
         }
      else
         {
         // Mark the runtime assumptions so they can be deleted at the end of the GC cycle.
         // The 3rd parameter to markAssumptionsAndDetach determines whether we mark preprologue assumptions.
         // We should only do that if we are in class unloading and will be reclaiming the entire method body,
         // which means faintCacheBlock is NULL.
         markAssumptionsAndDetach(jitConfig, metaData, (NULL == faintCacheBlock));
         }

      artifactManager->removeArtifact(metaData);
      dispatchUnloadHooks(jitConfig, vmThread, metaData);

      // Remove the reference to profiling information in the body info.
      // Depending on whether other persistent data structures hold references to it as well, the profiling
      // information may or may not be cleaned up.
      TR_PersistentJittedBodyInfo *bodyInfo = metaData->bodyInfo ? (TR_PersistentJittedBodyInfo *) metaData->bodyInfo : NULL;
      if (bodyInfo && bodyInfo->getProfileInfo())
         {
         bodyInfo->getProfileInfo()->setActive(false);
         TR_PersistentProfileInfo::decRefCount(bodyInfo->getProfileInfo());
         bodyInfo->setProfileInfo(NULL);
         }

      vlogReclamation("Reclaiming", metaData, faintCacheBlock ? faintCacheBlock->_bytesToSaveAtStart : 0);

      if (faintCacheBlock)
         {
         // This updates the existing J9JITExceptionTable to keep track of the stub left behind
         TR::CodeCacheManager::instance()->freeFaintCacheBlock(faintCacheBlock, (uint8_t *) faintCacheBlock->_metaData->startPC);
         
         // Create a new J9JITExceptionTable for the stub
         J9JITExceptionTable *stubMetadata = createStubExceptionTable(vmThread, metaData);
         
         if (stubMetadata)
            {
            // Mark the stub's artifact as unloaded
            markMetaDataAsUnloaded(stubMetadata);
            
            // We need to add the stub's artifact so as to be able to remove it on class unloading.
            artifactManager->insertArtifact(stubMetadata);
            
            freeExistingExceptionTable = true;
            
            if (TR::Options::getVerboseOption(TR_VerboseReclamation))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_RECLAMATION, "Reclaiming old metadata 0x%p, new metadata = 0x%p", metaData, stubMetadata);
               }
            }
         else
            {
            if (TR::Options::getVerboseOption(TR_VerboseReclamation))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_RECLAMATION, "Did not allocate stub metadata, reusing existing metadata 0x%p", metaData);
               }
            
            // We failed to allocate a new J9JITExceptionTable so we
            // need to re-add the stub's artifact so as to be able to remove it on class unloading.
            artifactManager->insertArtifact(metaData);
            }
         }
      else
         {
         TR::CodeCacheManager::instance()->addFreeBlock(reinterpret_cast<void *>(metaData), reinterpret_cast<uint8_t *>(metaData->startPC));
         freeExistingExceptionTable = true;
         }
      renewCodeCachePessimism(vmThread);
      }
   
   // Mark the stub's artifact as unloaded
   markMetaDataAsUnloaded(metaData);
   
   if (freeExistingExceptionTable)
      {
      /*
      // We currently don't need this because when we delete the list of J9JITExceptionTables,
      // we always delete from the head and never from the middle. However, this comment exists
      // as a reminder that if that changes, this code should be uncommented.

      if (metaData->prevMethod)
         {
         metaData->prevMethod->nextMethod = metaData->nextMethod;
         }
      else
         {
         J9Class *j9class = J9_CLASS_FROM_METHOD(metaData->ramMethod);
         TR_J9VMBase *fe = TR_J9VMBase::get(vmThread->javaVM->jitConfig, NULL);

         if (fe->isAnonymousClass((TR_OpaqueClassBlock *)j9class))
            {
            if (j9class->jitMetaDataList == metaData)
               j9class->jitMetaDataList = metaData->nextMethod;
            }
         else
            {
            if (j9class->classLoader->jitMetaDataList == metaData)
               j9class->classLoader->jitMetaDataList = metaData->nextMethod;
            }
         }

      if (metaData->nextMethod)
         metaData->nextMethod->prevMethod = metaData->prevMethod;
      */
      
      TR_DataCacheManager::getManager()->freeDataCacheRecord(metaData);
      }
   }

/*
 * walk all hash tables, deleting nodes which refer to the classloader
 * being unloaded
 */

void jitRemoveAllMetaDataForClassLoader(J9VMThread * vmThread, J9ClassLoader * classLoader)
   {
   TR::CodeCacheManager::instance()->purgeClassLoaderFromFaintBlocks(classLoader);
   J9JITExceptionTable *nextMetaData = classLoader->jitMetaDataList;
   J9JITExceptionTable* currentMetaData = NULL;
   while (nextMetaData)
      {
      currentMetaData = nextMetaData;
      nextMetaData = currentMetaData->nextMethod;
      jitReleaseCodeCollectMetaData(vmThread->javaVM->jitConfig, vmThread, currentMetaData);
      }
   classLoader->jitMetaDataList = NULL;
   }

void jitReclaimMarkedAssumptions(bool isEager)
   {
   static char *forceAggressiveRATCleaning = feGetEnv("TR_forceAggressiveRATCleaning");
   if (isEager && forceAggressiveRATCleaning)
      {
      reclaimMarkedAssumptionsFromRAT(-1);
      }
   else if (!isEager)
      {
      reclaimMarkedAssumptionsFromRAT(100);
      }
   }

void freeFastWalkCache(J9VMThread *vmThread, J9JITExceptionTable *metaData)
      {
      if (metaData->bodyInfo)
         {
         void * mapTable = ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->getMapTable();
         if (mapTable && mapTable != (void *)-1)
            {
            PORT_ACCESS_FROM_VMC(vmThread);
            j9mem_free_memory(mapTable);
            }
         ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->setMapTable(NULL);
         }
      }

void vlogReclamation(const char * prefix, J9JITExceptionTable *metaData, size_t bytesToSaveAtStart)
   {
   if (TR::Options::getVerboseOption(TR_VerboseReclamation))
      {

      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(
      TR_Vlog_RECLAMATION,
      "%s %.*s.%.*s%.*s @ %s [" POINTER_PRINTF_FORMAT "-",
      prefix,
      J9UTF8_LENGTH(metaData->className), J9UTF8_DATA(metaData->className),
      J9UTF8_LENGTH(metaData->methodName), J9UTF8_DATA(metaData->methodName),
      J9UTF8_LENGTH(metaData->methodSignature), J9UTF8_DATA(metaData->methodSignature),
      TR::Compilation::getHotnessName(TR_Hotness(metaData->hotness)),
      metaData->startPC + bytesToSaveAtStart
      );

      if (metaData->startColdPC)
         {
         TR_VerboseLog::write(
         POINTER_PRINTF_FORMAT "] & [" POINTER_PRINTF_FORMAT "-",
         metaData->endWarmPC,
         metaData->startColdPC
         );
         }

      TR_VerboseLog::write(
      POINTER_PRINTF_FORMAT "]",
      metaData->endPC
      );

      TR_VerboseLog::vlogRelease();
      }
   }
