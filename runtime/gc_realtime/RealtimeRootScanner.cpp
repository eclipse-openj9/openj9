/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"

#include <string.h>

#include "ClassModel.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#if defined(J9VM_GC_FINALIZATION)
#include "FinalizeListManager.hpp"
#include "FinalizerSupport.hpp"
#endif /* J9VM_GC_FINALIZATION */
#include "Heap.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySubSpace.hpp"
#include "modronapi.hpp"
#include "ObjectModel.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RealtimeRootScanner.hpp"
#include "RootScanner.hpp"
#include "Scheduler.hpp"
#include "SublistSlotIterator.hpp"
#include "Task.hpp"

void
MM_RealtimeRootScanner::doClass(J9Class *clazz)
{
	GC_ClassIterator objectSlotIterator(_env, clazz);
	volatile j9object_t *objectSlotPtr = NULL;
	while((objectSlotPtr = objectSlotIterator.nextSlot()) != NULL) {
		/* discard volatile since we must be in stop-the-world mode */
		doSlot((j9object_t*)objectSlotPtr);
	}
	GC_ClassIteratorClassSlots classSlotIterator(clazz);
	J9Class **classSlotPtr;
	while((classSlotPtr = classSlotIterator.nextSlot()) != NULL) {
		doClassSlot(classSlotPtr);
	}
}

/* Handle a classSlot. This handler is called for every reference to a J9Class.
 * 
 */
void
MM_RealtimeRootScanner::doClassSlot(J9Class **clazzPtr)
{
	_realtimeGC->getRealtimeDelegate()->markClass(_env, *clazzPtr);
}

MM_RootScanner::CompletePhaseCode
MM_RealtimeRootScanner::scanClassesComplete(MM_EnvironmentBase *env)
{
	/* TODO: consider reactivating this call */
	// reportScanningStarted(RootScannerEntity_ClassesComplete);
	// _realtimeGC->completeMarking(_env);
	// reportScanningEnded(RootScannerEntity_ClassesComplete);
	return complete_phase_OK;
}

/**
 * This function iterates through all the threads, calling scanOneThread on each one that
 * should be scanned.  The scanOneThread function scans exactly one thread and returns
 * either true (if it took an action that requires the thread list iterator to return to
 * the beginning) or false (if the thread list iterator should just continue with the next
 * thread).
 */
void
MM_RealtimeRootScanner::scanThreads(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_Threads);

	GC_VMThreadListIterator vmThreadListIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm));
	StackIteratorData localData;

	localData.rootScanner = this;
	localData.env = env;

	while(J9VMThread *walkThread = vmThreadListIterator.nextVMThread()) {
		MM_EnvironmentRealtime* walkThreadEnv = MM_EnvironmentRealtime::getEnvironment(walkThread->omrVMThread);
		if (GC_UNMARK == walkThreadEnv->_allocationColor) {
			if (GC_UNMARK == MM_AtomicOperations::lockCompareExchangeU32(&walkThreadEnv->_allocationColor, GC_UNMARK, GC_MARK)) {
				if (scanOneThread(env, walkThread, (void*) &localData)) {
					vmThreadListIterator.reset(static_cast<J9JavaVM*>(_omrVM->_language_vm)->mainThread);
				}
			}
		}
 	}

	reportScanningEnded(RootScannerEntity_Threads);
}

/**
 * The following override of scanOneThread performs metronome-specific processing before
 * and after the scanning of each thread.  Scanning is skipped if the thread has already
 * been scanned in this cycle.
 **/
bool
MM_RealtimeRootScanner::scanOneThread(MM_EnvironmentBase *envBase, J9VMThread* walkThread, void* localData)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	
	scanOneThreadImpl(env, walkThread, localData);

	/* Thead count is used under verbose only.
	 * Avoid the atomic add in the regular path.
	 */
	if (_realtimeGC->_sched->verbose() >= 3) {
		MM_AtomicOperations::add(&_threadCount, 1);
	}
	
	if (condYield()) {
		/* Optionally issue verbose message */
		if (_realtimeGC->_sched->verbose() >= 3) {
			PORT_ACCESS_FROM_ENVIRONMENT(env);
			j9tty_printf(PORTLIB, "Yielded during %s after scanning %d threads\n", scannerName(), _threadCount);
		}
		
		return true;
	}

	return false;
}

void
MM_RealtimeRootScanner::scanOneThreadImpl(MM_EnvironmentRealtime *env, J9VMThread* walkThread, void* localData)
{
}

void
MM_RealtimeRootScanner::reportThreadCount(MM_EnvironmentBase* env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	j9tty_printf(PORTLIB, "Scanned %d threads for %s\n", _threadCount, scannerName());
}

void
MM_RealtimeRootScanner::scanAtomicRoots(MM_EnvironmentRealtime *env)
{
	if (_classDataAsRoots || _nurseryReferencesOnly || _nurseryReferencesPossibly) {
		/* The classLoaderObject of a class loader might be in the nursery, but a class loader
		 * can never be in the remembered set, so include class loaders here.
		 */
		scanClassLoaders(env);
	}

	scanJNIGlobalReferences(env);

	if(_stringTableAsRoot && (!_nurseryReferencesOnly && !_nurseryReferencesPossibly)){
		scanStringTable(env);
	}
}

void
MM_RealtimeRootScanner::doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator)
{
	_env->getGCEnvironment()->_markJavaStats._stringConstantsCandidates += 1;
	if(!_markingScheme->isMarked(*slotPtr)) {
		_env->getGCEnvironment()->_markJavaStats._stringConstantsCleared += 1;
		stringTableIterator->removeSlot();
	}
}

/**
 * @Clear the string table cache slot if the object is not marked
 */
void 
MM_RealtimeRootScanner::doStringCacheTableSlot(J9Object **slotPtr) 
{
	J9Object *objectPtr = *slotPtr;
	if((NULL != objectPtr) && (!_markingScheme->isMarked(*slotPtr))) {
		*slotPtr = NULL;
	}	
}

/**
 * Calls the Scheduler's yielding API to determine if the GC should yield.
 * @return true if the GC should yield, false otherwise
 */ 
bool
MM_RealtimeRootScanner::shouldYield()
{
	return _realtimeGC->_sched->shouldGCYield(_env, 0);
}

/**
 * Calls the Scheduler's yielding API only if timeSlackNanoSec is non-zero or
 * if the yield count has reached 0.
 * @return true if the GC should yield, false otherwise
 */
bool
MM_RealtimeRootScanner::shouldYieldFromClassScan(UDATA timeSlackNanoSec)
{
	_yieldCount--;
	if (_yieldCount < 0 || timeSlackNanoSec != 0) {
		if (_realtimeGC->_sched->shouldGCYield(_env, 0)) {
			return true;
		}
		_yieldCount = ROOT_GRANULARITY;
	}
	return false;
}

bool
MM_RealtimeRootScanner::shouldYieldFromStringScan()
{
	_yieldCount--;
	if (_yieldCount < 0) {
		if (_realtimeGC->_sched->shouldGCYield(_env, 0)) {
			return true;
		}
		_yieldCount = ROOT_GRANULARITY;
	}
	return false;
}

bool
MM_RealtimeRootScanner::shouldYieldFromMonitorScan()
{
	_yieldCount--;
	if (_yieldCount < 0) {
		if (_realtimeGC->_sched->shouldGCYield(_env, 0)) {
			return true;
		}
		_yieldCount = ROOT_GRANULARITY;
	}
	return false;
}

/**
 * Yield from GC by calling the Scheduler's API. Also resets the yield count.
 * @note this does the same thing as condYield(). It should probably just call
 * the Scheduler's yield() method to clear up ambiguity but it's been left
 * untouched for reasons motivated purely by touching the least amount of code.
 */
void
MM_RealtimeRootScanner::yield()
{
	_realtimeGC->_sched->condYieldFromGC(_env);
	_yieldCount = ROOT_GRANULARITY;
}

/**
 * Yield only if the Scheduler deems yielding should occur at the time of the
 * call to this method.
 */ 
bool
MM_RealtimeRootScanner::condYield(U_64 timeSlackNanoSec)
{
	bool yielded = _realtimeGC->_sched->condYieldFromGC(_env, timeSlackNanoSec);
	_yieldCount = ROOT_GRANULARITY;
	return yielded;
}

/**
 * Scan the per-thread object monitor lookup caches.
 * Note that this is not a root since the cache contains monitors from the global monitor table
 * which will be scanned by scanMonitorReferences. It should be scanned first, however, since
 * scanMonitorReferences may destroy monitors that appear in caches.
 */
void
MM_RealtimeRootScanner::scanMonitorLookupCaches(MM_EnvironmentBase *env)
{
	reportScanningStarted(RootScannerEntity_MonitorLookupCaches);
	GC_VMThreadListIterator vmThreadListIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm));
	while(J9VMThread *walkThread = vmThreadListIterator.nextVMThread()) {
		MM_EnvironmentRealtime* walkThreadEnv = MM_EnvironmentRealtime::getEnvironment(walkThread->omrVMThread);
		if (FALSE == walkThreadEnv->_monitorCacheCleared) {
			if (FALSE == MM_AtomicOperations::lockCompareExchangeU32(&walkThreadEnv->_monitorCacheCleared, FALSE, TRUE)) {
#if defined(J9VM_THR_LOCK_NURSERY)
				j9objectmonitor_t *objectMonitorLookupCache = walkThread->objectMonitorLookupCache;
				UDATA cacheIndex = 0;
				for (; cacheIndex < J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE; cacheIndex++) {
					doMonitorLookupCacheSlot(&objectMonitorLookupCache[cacheIndex]);
				}
#else
				doMonitorLookupCacheSlot(&vmThread->cachedMonitor);
#endif /* J9VM_THR_LOCK_NURSERY */
				if (condYield()) {
					vmThreadListIterator.reset(static_cast<J9JavaVM*>(_omrVM->_language_vm)->mainThread);
				}
			}
		}
	}
	reportScanningEnded(RootScannerEntity_MonitorLookupCaches);
}

void
MM_RealtimeRootScanner::scanStringTable(MM_EnvironmentBase *env)
{
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		/* We can't rely on using _unmarkedImpliesCleared because clearable phase can mark more objects.
		 * Only at this point can we assume unmarked strings are truly dead.
		 */
		_realtimeGC->getRealtimeDelegate()->_unmarkedImpliesStringsCleared = true;
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	MM_RootScanner::scanStringTable(env);
}
