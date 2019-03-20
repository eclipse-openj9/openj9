/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "StaccatoGCDelegate.hpp"

#if defined(J9VM_GC_REALTIME)

#include "ClassHeapIterator.hpp"
#include "ClassIterator.hpp"
#include "ClassLoaderLinkedListIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "Heap.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "StaccatoAccessBarrier.hpp"
#include "StaccatoGC.hpp"


/**
 * Factory method for creating the access barrier. Note that the default staccato access barrier
 * doesn't handle the RTSJ checks.
 */
MM_RealtimeAccessBarrier*
MM_StaccatoGCDelegate::allocateAccessBarrier(MM_EnvironmentBase *env)
{
	return MM_StaccatoAccessBarrier::newInstance(env);
}

/**
 * Iterates over all threads and enables the double barrier for each thread by setting the
 * remebered set fragment index to the reserved index.
 */
void
MM_StaccatoGCDelegate::enableDoubleBarrier(MM_EnvironmentBase *env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)extensions->accessBarrier;
	GC_VMThreadListIterator vmThreadListIterator(_vm);
	
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
MM_StaccatoGCDelegate::disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread* vmThread)
{
	/* This gets called on a per thread basis as threads get scanned. */
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)extensions->accessBarrier;
	staccatoAccessBarrier->setDoubleBarrierInactiveOnThread(MM_EnvironmentBase::getEnvironment(vmThread));
}

/**
 * Disables the global double barrier flag. This should be called after all threads have been scanned
 * and disableDoubleBarrierOnThread has been called on each of them.
 */
void
MM_StaccatoGCDelegate::disableDoubleBarrier(MM_EnvironmentBase* env)
{
	/* The enabling of the double barrier must traverse all threads, but the double barrier gets disabled
	 * on a per thread basis as threads get scanned, so no need to traverse all threads in this method.
	 */
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	MM_StaccatoAccessBarrier* staccatoAccessBarrier = (MM_StaccatoAccessBarrier*)extensions->accessBarrier;
	staccatoAccessBarrier->setDoubleBarrierInactive();
}


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/**
 * Walk all class loaders marking their classes if the classLoader object has been
 * marked.  
 * 
 * @return true if any classloaders/classes are marked, false otherwise
 */
bool
MM_StaccatoGCDelegate::doClassTracing(MM_EnvironmentRealtime *env)
{
	J9ClassLoader *classLoader;
	bool didWork = false;
	
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	GC_ClassLoaderLinkedListIterator classLoaderIterator(env, extensions->classLoaderManager);
	
	while((classLoader = (J9ClassLoader *)classLoaderIterator.nextSlot()) != NULL) {
		if (0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			if(J9CLASSLOADER_ANON_CLASS_LOADER == (classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER)) {
				/* Anonymous classloader should be scanned on level of classes every time */
				GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
				J9MemorySegment *segment = NULL;
				while(NULL != (segment = segmentIterator.nextSegment())) {
					GC_ClassHeapIterator classHeapIterator(_vm, segment);
					J9Class *clazz = NULL;
					while(NULL != (clazz = classHeapIterator.nextClass())) {
						if((0 == (J9CLASS_EXTENDED_FLAGS(clazz) & J9ClassGCScanned)) && _staccatoGC->getMarkingScheme()->isMarked(clazz->classObject)) {
							J9CLASS_EXTENDED_FLAGS_SET(clazz, J9ClassGCScanned);

							/* Scan class */
							GC_ClassIterator objectSlotIterator(env, clazz);
							volatile j9object_t *objectSlotPtr = NULL;
							while((objectSlotPtr = objectSlotIterator.nextSlot()) != NULL) {
								didWork |= _staccatoGC->getMarkingScheme()->markObject(env, *objectSlotPtr);
							}

							GC_ClassIteratorClassSlots classSlotIterator(clazz);
							J9Class **classSlotPtr;
							while((classSlotPtr = classSlotIterator.nextSlot()) != NULL) {
								didWork |= _staccatoGC->getMarkingScheme()->markClass(env, *classSlotPtr);
							}
						}
					}
					_staccatoGC->condYield(env, 0);
				}
			} else {
				/* Check if the class loader has not been scanned but the class loader is live */
				if( !(classLoader->gcFlags & J9_GC_CLASS_LOADER_SCANNED) && _staccatoGC->getMarkingScheme()->isMarked((J9Object *)classLoader->classLoaderObject)) {
					/* Flag the class loader as being scanned */
					classLoader->gcFlags |= J9_GC_CLASS_LOADER_SCANNED;

					GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
					J9MemorySegment *segment = NULL;
					J9Class *clazz;

					while(NULL != (segment = segmentIterator.nextSegment())) {
						GC_ClassHeapIterator classHeapIterator(_vm, segment);
						while(NULL != (clazz = classHeapIterator.nextClass())) {
							/* Scan class */
							GC_ClassIterator objectSlotIterator(env, clazz);
							volatile j9object_t *objectSlotPtr = NULL;
							while((objectSlotPtr = objectSlotIterator.nextSlot()) != NULL) {
								didWork |= _staccatoGC->getMarkingScheme()->markObject(env, *objectSlotPtr);
							}

							GC_ClassIteratorClassSlots classSlotIterator(clazz);
							J9Class **classSlotPtr;
							while((classSlotPtr = classSlotIterator.nextSlot()) != NULL) {
								didWork |= _staccatoGC->getMarkingScheme()->markClass(env, *classSlotPtr);
							}
						}
						_staccatoGC->condYield(env, 0);
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
					clazz = _vm->internalVMFunctions->hashClassTableStartDo(classLoader, &walkState);
					while (NULL != clazz) {
						didWork |= _staccatoGC->getMarkingScheme()->markClass(env, clazz);
						clazz = _vm->internalVMFunctions->hashClassTableNextDo(&walkState);

						/**
						 * Jazz103 55784: We cannot rehash the table in the middle of iteration and the Space-opt hashtable cannot grow if
						 * J9HASH_TABLE_DO_NOT_REHASH is enabled. Don't yield if the hashtable is space-optimized because we run the
						 * risk of the mutator not being able to grow to accomodate new elements.
						 */
						if (!hashTableIsSpaceOptimized(classLoader->classHashTable)) {
							_staccatoGC->condYield(env, 0);
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

						didWork |= _staccatoGC->getMarkingScheme()->markObject(env, module->moduleObject);
						didWork |= _staccatoGC->getMarkingScheme()->markObject(env, module->moduleName);
						didWork |= _staccatoGC->getMarkingScheme()->markObject(env, module->version);
						modulePtr = (J9Module**)hashTableNextDo(&walkState);
					}
				}
			}
		}
		/* This yield point is for the case when there are lots of classloaders that will be unloaded */
		_staccatoGC->condYield(env, 0);
	}
	return didWork;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

bool
MM_StaccatoGCDelegate::doTracing(MM_EnvironmentRealtime* env)
{
	/* TODO CRGTMP make class tracing concurrent */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(_staccatoGC->getRealtimeDelegate()->isDynamicClassUnloadingEnabled()) {	
		return doClassTracing(env);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	return false;
}


bool
MM_StaccatoGCDelegate::initialize(MM_EnvironmentBase *env)
{
	/* create the appropriate access barrier for this configuration of Metronome */
	MM_RealtimeAccessBarrier *accessBarrier = NULL;
	accessBarrier = allocateAccessBarrier(env);
	if (NULL == accessBarrier) {
		return false;
	}
	MM_GCExtensions::getExtensions(_vm)->accessBarrier = (MM_ObjectAccessBarrier *)accessBarrier;

	return true;
}

#endif /* defined(J9VM_GC_REALTIME) */

