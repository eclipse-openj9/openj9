/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#ifndef SCAVENGERROOTCLEARER_HPP_
#define SCAVENGERROOTCLEARER_HPP_

#include "j9cfg.h"
#include "j9consts.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "GCExtensions.hpp"
#include "ParallelTask.hpp"
#include "RootScanner.hpp"
#include "Scavenger.hpp"

class MM_HeapRegionDescriptorStandard;

/**
 * The clearable root set scanner for MM_Scavenger.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Standard
 */
class MM_ScavengerRootClearer : public MM_RootScanner
{
private:
	MM_Scavenger *_scavenger;

	void processReferenceList(MM_EnvironmentStandard *env, MM_HeapRegionDescriptorStandard* region, omrobjectptr_t headOfList, MM_ReferenceStats *referenceStats);
	void scavengeReferenceObjects(MM_EnvironmentStandard *env, uintptr_t referenceObjectType);
#if defined(J9VM_GC_FINALIZATION)
	void scavengeUnfinalizedObjects(MM_EnvironmentStandard *env);
#endif /* defined(J9VM_GC_FINALIZATION) */

	void scavengeContinuationObjects(MM_EnvironmentStandard *env);

public:
	MM_ScavengerRootClearer(MM_EnvironmentBase *env, MM_Scavenger *scavenger) :
	MM_RootScanner(env),
	_scavenger(scavenger)
	{
		_typeId = __FUNCTION__;
		setNurseryReferencesOnly(true);

		/*
		 * JNI Weak Global References table can be skipped in Clearable phase
		 * if it has been scanned as a hard root for Concurrent Scavenger already
		 */
		_jniWeakGlobalReferencesTableAsRoot = _extensions->isConcurrentScavengerEnabled();
	};

	virtual void
	doSlot(omrobjectptr_t *slotPtr)
	{
		_scavenger->copyObjectSlot(MM_EnvironmentStandard::getEnvironment(_env), slotPtr);
	}

	virtual void
	doClass(J9Class *clazz)
	{
		/* we do not process classes in the scavenger */
		assume0(0);
	}

	virtual void
	scanSoftReferenceObjects(MM_EnvironmentBase *env)
	{
		if (_scavenger->getDelegate()->getShouldScavengeSoftReferenceObjects()) {
			reportScanningStarted(RootScannerEntity_SoftReferenceObjects);
			scavengeReferenceObjects(MM_EnvironmentStandard::getEnvironment(env), J9AccClassReferenceSoft);
			reportScanningEnded(RootScannerEntity_SoftReferenceObjects);
		}
	}

	virtual CompletePhaseCode
	scanSoftReferencesComplete(MM_EnvironmentBase *env)
	{
		/* do nothing -- no new objects could have been discovered by soft reference processing */
		return complete_phase_OK;
	}

	virtual void
	scanWeakReferenceObjects(MM_EnvironmentBase *env)
	{
		if (_scavenger->getDelegate()->getShouldScavengeWeakReferenceObjects()) {
			reportScanningStarted(RootScannerEntity_WeakReferenceObjects);
			scavengeReferenceObjects(MM_EnvironmentStandard::getEnvironment(env), J9AccClassReferenceWeak);
			reportScanningEnded(RootScannerEntity_WeakReferenceObjects);
		}
	}

	virtual CompletePhaseCode
	scanWeakReferencesComplete(MM_EnvironmentBase *env)
	{
		/* No new objects could have been discovered by soft / weak reference processing,
		 * but we must complete this phase prior to unfinalized processing to ensure that
		 * finalizable referents get cleared */
		if (_scavenger->getDelegate()->getShouldScavengeSoftReferenceObjects() || _scavenger->getDelegate()->getShouldScavengeWeakReferenceObjects()) {
			env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
		}
		return complete_phase_OK;
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void
	scanUnfinalizedObjects(MM_EnvironmentBase *env)
	{
		/* allow the scavenger to handle this */
		if (_scavenger->getDelegate()->getShouldScavengeUnfinalizedObjects()) {
			reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
			scavengeUnfinalizedObjects(MM_EnvironmentStandard::getEnvironment(env));
			reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
		}
	}

	virtual CompletePhaseCode
	scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env)
	{
		CompletePhaseCode result = complete_phase_OK;
		if (_scavenger->getDelegate()->getShouldScavengeUnfinalizedObjects()) {
			reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
			/* ensure that all unfinalized processing is complete before we start marking additional objects */
			env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
			if (!_scavenger->completeScan(MM_EnvironmentStandard::getEnvironment(env))) {
				result = complete_phase_ABORT;
			}
			reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
		}
		return result;
	}
#endif /* J9VM_GC_FINALIZATION */

	/* empty, move ownable synchronizer processing in main scan phase */
	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env) {}

	virtual void
	scanContinuationObjects(MM_EnvironmentBase *env)
	{
		if (_scavenger->getDelegate()->getShouldScavengeContinuationObjects()) {
			/* allow the scavenger to handle this */
			reportScanningStarted(RootScannerEntity_ContinuationObjects);
			scavengeContinuationObjects(MM_EnvironmentStandard::getEnvironment(env));
			reportScanningEnded(RootScannerEntity_ContinuationObjects);
		}
	}

	virtual void
	iterateAllContinuationObjects(MM_EnvironmentBase *env);

	virtual void
	scanPhantomReferenceObjects(MM_EnvironmentBase *env)
	{
		if (_scavenger->getDelegate()->getShouldScavengePhantomReferenceObjects()) {
			reportScanningStarted(RootScannerEntity_PhantomReferenceObjects);
			scavengeReferenceObjects(MM_EnvironmentStandard::getEnvironment(env), J9AccClassReferencePhantom);
			reportScanningEnded(RootScannerEntity_PhantomReferenceObjects);
		}
	}

	virtual CompletePhaseCode
	scanPhantomReferencesComplete(MM_EnvironmentBase *env)
	{
		CompletePhaseCode result = complete_phase_OK;
		if (_scavenger->getDelegate()->getShouldScavengePhantomReferenceObjects()) {
			reportScanningStarted(RootScannerEntity_PhantomReferenceObjectsComplete);
			if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
				env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_phantom;
				env->_currentTask->releaseSynchronizedGCThreads(env);
			}
			/* phantom reference processing may resurrect objects - scan them now */
			if (!_scavenger->completeScan(MM_EnvironmentStandard::getEnvironment(env))) {
				result = complete_phase_ABORT;
			}

			reportScanningEnded(RootScannerEntity_PhantomReferenceObjectsComplete);
		}
		return result;
	}

	virtual void
	doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
	{
		bool const compressed = _extensions->compressObjectReferences();
		J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
		omrobjectptr_t objectPtr = (omrobjectptr_t )monitor->userData;
		_env->getGCEnvironment()->_scavengerJavaStats._monitorReferenceCandidates += 1;
		
		if (_scavenger->isObjectInEvacuateMemory(objectPtr)) {
			MM_ForwardedHeader forwardedHeader(objectPtr, compressed);
			omrobjectptr_t forwardPtr = forwardedHeader.getForwardedObject();
			if (NULL != forwardPtr) {
				monitor->userData = (uintptr_t)forwardPtr;
			} else {
				_env->getGCEnvironment()->_scavengerJavaStats._monitorReferenceCleared += 1;
				monitorReferenceIterator->removeSlot();
				/* We must call objectMonitorDestroy (as opposed to omrthread_monitor_destroy) when the
				 * monitor is not internal to the GC
				 */
				static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroy(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)_env->getLanguageVMThread(), (omrthread_monitor_t)monitor);
			}
		}
	}

	virtual CompletePhaseCode
	scanMonitorReferencesComplete(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_MonitorReferenceObjectsComplete);
		static_cast<J9JavaVM*>(_omrVM->_language_vm)->internalVMFunctions->objectMonitorDestroyComplete(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9VMThread *)env->getOmrVMThread()->_language_vmthread);
		reportScanningEnded(RootScannerEntity_MonitorReferenceObjectsComplete);
		return complete_phase_OK;
	}

	virtual void
	scanJNIWeakGlobalReferences(MM_EnvironmentBase *env)
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/*
		 * Currently Concurrent Scavenger replaces STW Scavenger, so this check is not necessary
		 * (Concurrent Scavenger is always in progress)
		 * However Concurrent Scavenger runs might be interlaced with STW Scavenger time to time
		 * (for example for reducing amount of floating garbage)
		 */
		if (!_scavenger->isConcurrentCycleInProgress())
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		{
			MM_RootScanner::scanJNIWeakGlobalReferences(env);
		}
	}

	virtual void
	doJNIWeakGlobalReference(omrobjectptr_t *slotPtr)
	{
		bool const compressed = _extensions->compressObjectReferences();
		omrobjectptr_t objectPtr = *slotPtr;
		if (objectPtr && _scavenger->isObjectInEvacuateMemory(objectPtr)) {
			MM_ForwardedHeader forwardedHeader(objectPtr, compressed);
			*slotPtr = forwardedHeader.getForwardedObject();
		}
	}

#if defined(J9VM_OPT_JVMTI)
	virtual void
	doJVMTIObjectTagSlot(omrobjectptr_t *slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
	{
		bool const compressed = _extensions->compressObjectReferences();
		omrobjectptr_t objectPtr = *slotPtr;
		if (objectPtr && _scavenger->isObjectInEvacuateMemory(objectPtr)) {
			MM_ForwardedHeader forwardedHeader(objectPtr, compressed);
			*slotPtr = forwardedHeader.getForwardedObject();
		}
	}
#endif /* J9VM_OPT_JVMTI */
#if defined(J9VM_GC_FINALIZATION)
	virtual void
	doFinalizableObject(omrobjectptr_t object)
	{
		Assert_MM_unreachable();
	}
#endif /* J9VM_GC_FINALIZATION */

	void
	pruneRememberedSet(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_RememberedSet);
		_scavenger->pruneRememberedSet(MM_EnvironmentStandard::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_RememberedSet);
	}
};
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* SCAVENGERROOTCLEARER_HPP_ */
