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

/**
 * @file
 * @ingroup GC_Metronome
 */

#if !defined(REALTIMEROOTCLEARER_HPP_)
#define REALTIMEROOTCLEARER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "EnvironmentRealtime.hpp"
#include "RealtimeRootScanner.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"

/**
 * This scanner will mark objects that pass through its doSlot.
 */
class MM_RealtimeMarkingSchemeRootClearer : public MM_RealtimeRootScanner
{
/* Data members / types */
public:
protected:
private:
	
/* Methods */
public:

	/**
	 * Simple chained constructor.
	 */
	MM_RealtimeMarkingSchemeRootClearer(MM_EnvironmentRealtime *env, MM_RealtimeGC *realtimeGC) :
		MM_RealtimeRootScanner(env, realtimeGC)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * This should not be called
	 */
	virtual void
	doSlot(J9Object** slot)
	{
		Assert_MM_unreachable();
	}

	/**
	 * This scanner can be instantiated so we must give it a name.
	 */
	virtual const char*
	scannerName()
	{
		return "Clearable";
	}
	
	/**
	 * Destroys unmarked monitors.
	 */
	virtual void
	doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
	{
		J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
		if(!_markingScheme->isMarked((J9Object *)monitor->userData)) {
			monitorReferenceIterator->removeSlot();
			/* We must call objectMonitorDestroy (as opposed to omrthread_monitor_destroy) when the
			 * monitor is not internal to the GC */
			static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroy(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)_env->getLanguageVMThread(), (omrthread_monitor_t)monitor);
		}
	}
	
	/**
	 * Wraps the MM_RootScanner::scanMonitorReferences method to disable yielding during the scan.
	 * @see MM_RootScanner::scanMonitorReferences()
	 */
	virtual void
	scanMonitorReferences(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentRealtime *env = (MM_EnvironmentRealtime *)envBase;
		
		/* @NOTE For SRT and MT to play together this needs to be investigated */
		if(_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			reportScanningStarted(RootScannerEntity_MonitorReferences);
		
			J9ObjectMonitor *objectMonitor = NULL;
			J9MonitorTableListEntry *monitorTableList = static_cast<J9JavaVM*>(_omrVM->_language_vm)->monitorTableList;
			while (NULL != monitorTableList) {
				J9HashTable *table = monitorTableList->monitorTable;
				if (NULL != table) {
					GC_HashTableIterator iterator(table);
					iterator.disableTableGrowth();
					while (NULL != (objectMonitor = (J9ObjectMonitor*)iterator.nextSlot())) {
						doMonitorReference(objectMonitor, &iterator);
						if (shouldYieldFromMonitorScan()) {
							yield();
						}
					}
					iterator.enableTableGrowth();
				}
				monitorTableList = monitorTableList->next;
			}

			reportScanningEnded(RootScannerEntity_MonitorReferences);			
		}
	}

	virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_MonitorReferenceObjectsComplete);
		((J9JavaVM *)env->getLanguageVM())->internalVMFunctions->objectMonitorDestroyComplete((J9JavaVM *)env->getLanguageVM(), (J9VMThread *)env->getLanguageVMThread());
		reportScanningEnded(RootScannerEntity_MonitorReferenceObjectsComplete);
		return complete_phase_OK;
	}
	
	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_WeakReferenceObjects);

		_realtimeGC->getRealtimeDelegate()->scanWeakReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_WeakReferenceObjects);
	}

	virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_WeakReferenceObjectsComplete);

		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_weak;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}

		reportScanningEnded(RootScannerEntity_WeakReferenceObjectsComplete);

		return complete_phase_OK;
	}

	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_SoftReferenceObjects);

		_realtimeGC->getRealtimeDelegate()->scanSoftReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_SoftReferenceObjects);
	}

	virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_SoftReferenceObjectsComplete);
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_soft;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		reportScanningEnded(RootScannerEntity_SoftReferenceObjectsComplete);
		return complete_phase_OK;
	}

	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_PhantomReferenceObjects);

		_realtimeGC->getRealtimeDelegate()->scanPhantomReferenceObjects(env);

		reportScanningEnded(RootScannerEntity_PhantomReferenceObjects);
	}

	virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_PhantomReferenceObjectsComplete);
		if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_phantom;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		/* phantom reference processing may resurrect objects - scan them now */
		_realtimeGC->completeMarking(MM_EnvironmentRealtime::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_PhantomReferenceObjectsComplete);
		return complete_phase_OK;
	}

#if defined(J9VM_GC_FINALIZATION)
	/**
	 * Wraps the MM_RootScanner::scanUnfinalizedObjects method to disable yielding during the scan
	 * then yield after scanning.
	 * @see MM_RootScanner::scanUnfinalizedObjects()
	 */
	virtual void
	scanUnfinalizedObjects(MM_EnvironmentBase *envBase)	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		/* allow the marking scheme to handle this */
		_realtimeGC->getRealtimeDelegate()->scanUnfinalizedObjects(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}

	virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *envBase) {
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
		/* ensure that all unfinalized processing is complete before we start marking additional objects */
		env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		_realtimeGC->completeMarking(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
		return complete_phase_OK;
	}

	virtual void doFinalizableObject(j9object_t object) {
		Assert_MM_unreachable();
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void
	scanOwnableSynchronizerObjects(MM_EnvironmentBase *envBase)	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);

		reportScanningStarted(RootScannerEntity_OwnableSynchronizerObjects);
		/* allow the marking scheme to handle this */
		_realtimeGC->getRealtimeDelegate()->scanOwnableSynchronizerObjects(env);
		reportScanningEnded(RootScannerEntity_OwnableSynchronizerObjects);
	}

	/**
	 * Wraps the MM_RootScanner::scanJNIWeakGlobalReferences method to disable yielding during the scan.
	 * @see MM_RootScanner::scanJNIWeakGlobalReferences()
	 */
	virtual void
	scanJNIWeakGlobalReferences(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
		
		env->disableYield();
		MM_RootScanner::scanJNIWeakGlobalReferences(env);
		env->enableYield();
	}
	
	/**
	 * Clears slots holding unmarked objects.
	 */
	virtual void
	doJNIWeakGlobalReference(J9Object **slotPtr)
	{
		J9Object *objectPtr = *slotPtr;
		if(objectPtr && !_markingScheme->isMarked(objectPtr)) {
			*slotPtr = NULL;
		}
	}

#if defined(J9VM_OPT_JVMTI)
	/**
	 * Clears slots holding unmarked objects.
	 */
	virtual void
	doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
	{
		J9Object *objectPtr = *slotPtr;
		if(objectPtr && !_markingScheme->isMarked(objectPtr)) {
			*slotPtr = NULL;
		}
	}
#endif /* J9VM_OPT_JVMTI */
	
protected:
private:
};

#endif /* REALTIMEROOTCLEARER_HPP_ */

