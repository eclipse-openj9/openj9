/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "jithash.h"
#include "avl_api.h"
#include "util_api.h"
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/ArtifactManager.hpp"
#include "infra/Monitor.hpp"

TR_TranslationArtifactManager *TR_TranslationArtifactManager::globalManager = NULL;

TR_VMExclusiveAccess::TR_VMExclusiveAccess(J9JavaVM *vm) :
   _vm(vm),
   _currentThread(vm->internalVMFunctions->currentVMThread(vm))
   {
   if (_currentThread)
      _vm->internalVMFunctions->acquireExclusiveVMAccess(_currentThread);
   }


TR_VMExclusiveAccess::~TR_VMExclusiveAccess()
   {
   if (_currentThread)
      _vm->internalVMFunctions->releaseExclusiveVMAccess(_currentThread);
   }


bool
TR_TranslationArtifactManager::initializeGlobalArtifactManager(J9AVLTree *translationArtifacts, J9JavaVM *vm)
   {
   bool initSuccess = false;
   if (globalManager)
      {
      initSuccess = true;
      }
   else
      {
      TR::Monitor *monitor = TR::Monitor::create("JIT-ArtifactMonitor");
      if (monitor)
         {
         globalManager = new (PERSISTENT_NEW) TR_TranslationArtifactManager(translationArtifacts, vm, monitor);
         if (globalManager)
            initSuccess = true;
         }
      }
   return initSuccess;
   }

TR_TranslationArtifactManager::TR_TranslationArtifactManager(J9AVLTree *translationArtifacts, J9JavaVM *vm, TR::Monitor *monitor) :
   _translationArtifacts(translationArtifacts),
   _vm(vm),
   _portLibrary(vm->portLibrary),
   _monitor(monitor),
   _cachedPC(0),
   _cachedHashTable(NULL),
   _retrievedArtifactCache(NULL)
   {
   TR_ASSERT(translationArtifacts, "translationArtifacts must not be null");
   TR_ASSERT(vm, "vm must not be null");
   TR_ASSERT(monitor, "monitor must not be null");
   }


bool
TR_TranslationArtifactManager::addCodeCache(TR::CodeCache *codeCache)
   {
   bool success = false;
   TR_ASSERT(codeCache, "codeCache must not be null");
   TR_VMExclusiveAccess exclusiveAccess(_vm);
   J9JITHashTable *newTable = hash_jit_allocate(_portLibrary, reinterpret_cast<UDATA>(codeCache->getCodeBase()), reinterpret_cast<UDATA>(codeCache->getCodeTop()) );
   if (newTable)
      {
      success = (avl_insert(_translationArtifacts, (J9AVLTreeNode *) newTable) != NULL);
      }
   return success;
   }


bool
TR_TranslationArtifactManager::removeCodeCache(TR::CodeCache *codeCache)
   {
   TR_ASSERT(codeCache, "codeCache must not be null");
   TR_VMExclusiveAccess exclusiveAccess(_vm);
   _cachedPC = 0;
   _cachedHashTable = NULL;
   _retrievedArtifactCache = NULL;
   return false;
   }


bool
TR_TranslationArtifactManager::insertArtifact(J9JITExceptionTable *artifact)
   {
   TR_ASSERT(artifact, "artifact must not be null");
   OMR::CriticalSection insertingArtifact(_monitor);
   bool insertSuccess = false;
   insertSuccess = insertRange(artifact, artifact->startPC, artifact->endWarmPC);
   if (insertSuccess && artifact->startColdPC)
      {
      insertSuccess = insertRange(artifact, artifact->startColdPC, artifact->endPC);
      if (!insertSuccess)
         {
         removeRange(artifact, artifact->startPC, artifact->endWarmPC);
         }
      }
   return insertSuccess;
   }


bool
TR_TranslationArtifactManager::containsArtifact(J9JITExceptionTable *artifact) const
   {
   return (artifact && artifact == retrieveArtifact(artifact->startPC));
   }


bool
TR_TranslationArtifactManager::removeArtifact(J9JITExceptionTable *artifact)
   {
   TR_ASSERT(artifact, "artifact must not be null");
   OMR::CriticalSection removingArtifact(_monitor);
   bool removeSuccess = false;
   if (containsArtifact(artifact))
      {
      removeSuccess =  removeRange(artifact, artifact->startPC, artifact->endWarmPC);
      if (removeSuccess && artifact->startColdPC)
         {
         removeSuccess = removeRange(artifact, artifact->startColdPC, artifact->endPC);
         }
      }
   _retrievedArtifactCache = NULL;
   return removeSuccess;
   }


const J9JITExceptionTable *
TR_TranslationArtifactManager::retrieveArtifact(UDATA pc) const
   {
   TR_ASSERT(pc != 0, "attempting to query existing artifacts for a NULL PC");
   OMR::CriticalSection searchingArtifacts(_monitor);
   updateCache(pc);
   if (!_retrievedArtifactCache)
      {
      if (_cachedHashTable)
         {
         _retrievedArtifactCache = hash_jit_artifact_search(_cachedHashTable, pc);
         }
      }
   return _retrievedArtifactCache;
   }


bool
TR_TranslationArtifactManager::insertRange(J9JITExceptionTable *artifact, UDATA startPC, UDATA endPC)
   {
   bool insertSuccess = false;
   updateCache(artifact->startPC);
   if (_cachedHashTable)
      {
      insertSuccess = (hash_jit_artifact_insert_range(_portLibrary, _cachedHashTable, artifact, startPC, endPC) == 0);
      }
   return insertSuccess;
   }


bool
TR_TranslationArtifactManager::removeRange(J9JITExceptionTable *artifact, UDATA startPC, UDATA endPC)
   {
   bool removeSuccess = false;
   updateCache(artifact->startPC);
   if (_cachedHashTable)
      {
      removeSuccess = (hash_jit_artifact_remove_range(_portLibrary, _cachedHashTable, artifact, startPC, endPC) == 0);
      }
   return removeSuccess;
   }


void
TR_TranslationArtifactManager::updateCache(UDATA currentPC) const
   {
   TR_ASSERT(currentPC > 0, "Attempting to find a code cache's artifact hash table for a NULL PC.");
   if (currentPC != _cachedPC)
      {
      _retrievedArtifactCache = NULL;
      _cachedPC = currentPC;
      _cachedHashTable = static_cast<J9JITHashTable *>(
                                    static_cast<void *>(
                                       avl_search(
                                          _translationArtifacts,
                                          currentPC
                                       )
                                    )
                                 );
      TR_ASSERT(_cachedHashTable, "Either we lost a code cache or We attempted to find a hash table for a non-code cache startPC");
      }
   }
