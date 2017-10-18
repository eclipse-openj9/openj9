
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "BarrierSynchronization.hpp"
#include "ClassLoaderLinkedListIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "GCExtensions.hpp"
#include "IncrementalParallelTask.hpp"
#include "LockingFreeHeapRegionList.hpp"
#include "ModronTypes.hpp"
#include "RememberedSetWorkPackets.hpp"
#include "StaccatoAccessBarrier.hpp"
#include "StaccatoGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "WorkPacketsRealtime.hpp"
#include "VMThreadListIterator.hpp"
#include "EnvironmentStaccato.hpp"
#include "ModronAssertions.h"

/* TuningFork name/version information for gc_staccato */
#define TUNINGFORK_STACCATO_EVENT_SPACE_NAME "com.ibm.realtime.vm.trace.gc.metronome"
#define TUNINGFORK_STACCATO_EVENT_SPACE_VERSION 200

MM_StaccatoGC *
MM_StaccatoGC::newInstance(MM_EnvironmentBase *env)
{
	MM_StaccatoGC *globalGC = (MM_StaccatoGC *)env->getForge()->allocate(sizeof(MM_StaccatoGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (globalGC) {
		new(globalGC) MM_StaccatoGC(env);
		if (!globalGC->initialize(env)) {
			globalGC->kill(env);
			globalGC = NULL;   			
		}
	}
	return globalGC;
}

void
MM_StaccatoGC::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_StaccatoGC::initialize(MM_EnvironmentBase *env)
{
	if (!MM_RealtimeGC::initialize(env)) {
		return false;
	}
	
 	_extensions->staccatoRememberedSet = MM_RememberedSetWorkPackets::newInstance(env, _workPackets);
 	if (NULL == _extensions->staccatoRememberedSet) {
 		return false;
 	}
 	
 	return true;
}

void
MM_StaccatoGC::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _extensions->staccatoRememberedSet) {
		_extensions->staccatoRememberedSet->kill(env);
		_extensions->staccatoRememberedSet = NULL;
	}
	
	MM_RealtimeGC::tearDown(env);
}

/**
 * Factory method for creating the access barrier. Note that the default staccato access barrier
 * doesn't handle the RTSJ checks.
 */
MM_RealtimeAccessBarrier*
MM_StaccatoGC::allocateAccessBarrier(MM_EnvironmentBase *env)
{
	return MM_StaccatoAccessBarrier::newInstance(env);
}

/**
 * Enables the write barrier, this should be called at the beginning of the mark phase.
 */
void
MM_StaccatoGC::enableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	extensions->staccatoRememberedSet->restoreGlobalFragmentIndex(env);
}

/**
 * Disables the write barrier, this should be called at the end of the mark phase.
 */
void
MM_StaccatoGC::disableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	extensions->staccatoRememberedSet->preserveGlobalFragmentIndex(env);
}

/**
 * Iterates over all threads and enables the double barrier for each thread by setting the
 * remebered set fragment index to the reserved index.
 */
void
MM_StaccatoGC::enableDoubleBarrier(MM_EnvironmentBase *env)
{
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)_extensions->accessBarrier;
	GC_VMThreadListIterator vmThreadListIterator(_javaVM);
	
	/* First, enable the global double barrier flag so new threads will have the double barrier enabled. */
	staccatoAccessBarrier->setDoubleBarrierActive();
	while(J9VMThread* thread = vmThreadListIterator.nextVMThread()) {
		/* Second, enable the double barrier on all threads individually. */
		staccatoAccessBarrier->setDoubleBarrierActiveOnThread(MM_EnvironmentBase::getEnvironment(thread->omrVMThread));
	}
}

/**
 * Disables the double barrier for the specified thread.
 */
void
MM_StaccatoGC::disableDoubleBarrierOnThread(MM_EnvironmentBase* env, J9VMThread* vmThread)
{
	/* This gets called on a per thread basis as threads get scanned. */
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)_extensions->accessBarrier;
	staccatoAccessBarrier->setDoubleBarrierInactiveOnThread(MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread));
}

/**
 * Disables the global double barrier flag. This should be called after all threads have been scanned
 * and disableDoubleBarrierOnThread has been called on each of them.
 */
void
MM_StaccatoGC::disableDoubleBarrier(MM_EnvironmentBase* env)
{
	/* The enabling of the double barrier must traverse all threads, but the double barrier gets disabled
	 * on a per thread basis as threads get scanned, so no need to traverse all threads in this method.
	 */
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)_extensions->accessBarrier;
	staccatoAccessBarrier->setDoubleBarrierInactive();
}

void
MM_StaccatoGC::flushRememberedSet(MM_EnvironmentRealtime *env)
{
	if (_workPackets->inUsePacketsAvailable(env)) {
		_workPackets->moveInUseToNonEmpty(env);
		_extensions->staccatoRememberedSet->flushFragments(env);
	}
}

/**
 * Perform the tracing phase.  For tracing to be complete the work stack and rememberedSet
 * have to be empty and class tracing has to complete without marking any objects.
 * 
 * If concurrentMarkingEnabled is true then tracing is completed concurrently.
 */
void
MM_StaccatoGC::doTracing(MM_EnvironmentRealtime *env)
{
	
	do {
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			flushRememberedSet(env);
			if (_extensions->concurrentTracingEnabled) {
				setCollectorConcurrentTracing();
				_sched->_barrierSynchronization->releaseExclusiveVMAccess(env, _sched->_exclusiveVMAccessRequired);
			} else {
				setCollectorTracing();
			}
					
			_moreTracingRequired = false;
			
			/* From this point on the Scheduler collaborates with WorkPacketsRealtime on yielding.
			 * Strictly speaking this should be done first thing in incrementalConsumeQueue().
			 * However, it would require another synchronizeGCThreadsAndReleaseMaster barrier.
			 * So we are just reusing the existing one.
			 */
			_sched->pushYieldCollaborator(_workPackets->getYieldCollaborator());
			
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		
		if(_markingScheme->incrementalConsumeQueue(env, MAX_UINT)) {
			_moreTracingRequired = true;
		}

		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			/* restore the old Yield Collaborator */
			_sched->popYieldCollaborator();
			
			if (_extensions->concurrentTracingEnabled) {
				_sched->_barrierSynchronization->acquireExclusiveVMAccess(env, _sched->_exclusiveVMAccessRequired);
				setCollectorTracing();
			}
			/* TODO CRGTMP make class tracing concurrent */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			if(isDynamicClassUnloadingEnabled()) {	
				_moreTracingRequired |= doClassTracing(env);
			}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			/* the workStack and rememberedSet use the same workPackets
			 * as backing store.  If all packets are empty this means the
			 * workStack and rememberedSet processing are both complete.
			 */
			_moreTracingRequired |= !_workPackets->isAllPacketsEmpty();
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	} while(_moreTracingRequired);
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/**
 * Walk all class loaders marking their classes if the classLoader object has been
 * marked.  
 * 
 * @return true if any classloaders/classes are marked, false otherwise
 */
bool
MM_StaccatoGC::doClassTracing(MM_EnvironmentRealtime *env)
{
	J9ClassLoader *classLoader;
	bool didWork = false;
	
	GC_ClassLoaderLinkedListIterator classLoaderIterator(env, _extensions->classLoaderManager);
	
	while((classLoader = (J9ClassLoader *)classLoaderIterator.nextSlot()) != NULL) {
		if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			if(J9CLASSLOADER_ANON_CLASS_LOADER == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER)) {
				/* Anonymous classloader should be scanned on level of classes every time */
				GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
				J9MemorySegment *segment = NULL;
				while(NULL != (segment = segmentIterator.nextSegment())) {
					GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
					J9Class *clazz = NULL;
					while(NULL != (clazz = classHeapIterator.nextClass())) {
						if((0 == (J9CLASS_EXTENDED_FLAGS(clazz) & J9ClassGCScanned)) && _markingScheme->isMarked(clazz->classObject)) {
							J9CLASS_EXTENDED_FLAGS_SET(clazz, J9ClassGCScanned);

							/* Scan class */
							GC_ClassIterator objectSlotIterator(env, clazz);
							volatile j9object_t *objectSlotPtr = NULL;
							while((objectSlotPtr = objectSlotIterator.nextSlot()) != NULL) {
								didWork |= _markingScheme->markObject(env, *objectSlotPtr);
							}

							GC_ClassIteratorClassSlots classSlotIterator(clazz);
							J9Class **classSlotPtr;
							while((classSlotPtr = classSlotIterator.nextSlot()) != NULL) {
								didWork |= _markingScheme->markClass(env, *classSlotPtr);
							}
						}
					}
					condYield(env, 0);
				}
			} else {
				/* Check if the class loader has not been scanned but the class loader is live */
				if( !(classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) && _markingScheme->isMarked((J9Object *)classLoader->classLoaderObject)) {
					/* Flag the class loader as being scanned */
					classLoader->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;

					GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
					J9MemorySegment *segment = NULL;
					J9Class *clazz;

					while(NULL != (segment = segmentIterator.nextSegment())) {
						GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
						while(NULL != (clazz = classHeapIterator.nextClass())) {
							/* Scan class */
							GC_ClassIterator objectSlotIterator(env, clazz);
							volatile j9object_t *objectSlotPtr = NULL;
							while((objectSlotPtr = objectSlotIterator.nextSlot()) != NULL) {
								didWork |= _markingScheme->markObject(env, *objectSlotPtr);
							}

							GC_ClassIteratorClassSlots classSlotIterator(clazz);
							J9Class **classSlotPtr;
							while((classSlotPtr = classSlotIterator.nextSlot()) != NULL) {
								didWork |= _markingScheme->markClass(env, *classSlotPtr);
							}
						}
						condYield(env, 0);
					}

					/* CMVC 131487 */
					J9HashTableState walkState;
					/*
					 * We believe that (NULL == classLoader->classHashTable) is set ONLY for DEAD class loader
					 * so, if this pointer happend to be NULL at this point let it crash here
					 */
					Assert_MM_true(NULL != classLoader->classHashTable);
					/*
					 * CMVC 178060 : disable hash table growth to prevent hash table entries from being rehashed during GC yield
					 * while GC was in the middle of iterating the hash table.
					 */
					hashTableSetFlag(classLoader->classHashTable, J9HASH_TABLE_DO_NOT_REHASH);
					clazz = _javaVM->internalVMFunctions->hashClassTableStartDo(classLoader, &walkState);
					while (NULL != clazz) {
						didWork |= _markingScheme->markClass(env, clazz);
						clazz = _javaVM->internalVMFunctions->hashClassTableNextDo(&walkState);

						/**
						 * Jazz103 55784: We cannot rehash the table in the middle of iteration and the Space-opt hashtable cannot grow if
						 * J9HASH_TABLE_DO_NOT_REHASH is enabled. Don't yield if the hashtable is space-optimized because we run the
						 * risk of the mutator not being able to grow to accomodate new elements.
						 */
						if (!hashTableIsSpaceOptimized(classLoader->classHashTable)) {
							condYield(env, 0);
						}
					}
					/*
					 * CMVC 178060 : re-enable hash table growth. disable hash table growth to prevent hash table entries from being rehashed during GC yield
					 * while GC was in the middle of iterating the hash table.
					 */
					hashTableResetFlag(classLoader->classHashTable, J9HASH_TABLE_DO_NOT_REHASH);

					Assert_MM_true(NULL != classLoader->moduleHashTable);
					J9Module **modulePtr = (J9Module **)hashTableStartDo(classLoader->moduleHashTable, &walkState);
					while (NULL != modulePtr) {
						J9Module * const module = *modulePtr;

						didWork |= _markingScheme->markObject(env, module->moduleObject);
						didWork |= _markingScheme->markObject(env, module->moduleName);
						didWork |= _markingScheme->markObject(env, module->version);
						modulePtr = (J9Module**)hashTableNextDo(&walkState);
					}
				}
			}
		}
		/* This yield point is for the case when there are lots of classloaders that will be unloaded */
		condYield(env, 0);
	}
	return didWork;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
