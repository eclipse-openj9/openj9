
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

#ifndef SCAVENGERROOTSCANNER_HPP_
#define SCAVENGERROOTSCANNER_HPP_

#include "j9cfg.h"
#include "j9consts.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)
#include "CollectorLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "FinalizeListManager.hpp"
#include "GCExtensions.hpp"
#include "OwnableSynchronizerObjectBuffer.hpp"
#include "ParallelTask.hpp"
#include "ReferenceObjectBuffer.hpp"
#include "RootScanner.hpp"
#include "Scavenger.hpp"
#include "ScavengerDelegate.hpp"
#include "ScavengerRootClearer.hpp"
#include "ScavengerThreadRescanner.hpp"
#include "StackSlotValidator.hpp"
#include "VMThreadIterator.hpp"

/**
 * The root set scanner for MM_Scavenger.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Standard
 */
class MM_ScavengerRootScanner : public MM_RootScanner
{
	/**
	 * Data members
	 */
private:
	MM_Scavenger *_scavenger;
	MM_ScavengerRootClearer _rootClearer;
	MM_ScavengerDelegate *_scavengerDelegate;

protected:

public:

	/**
	 * Function members
	 */
private:
#if defined(J9VM_GC_FINALIZATION)
	void startUnfinalizedProcessing(MM_EnvironmentBase *env);
	void scavengeFinalizableObjects(MM_EnvironmentStandard *env);
#endif /* defined(J9VM_GC_FINALIZATION) */

protected:

public:
	MM_ScavengerRootScanner(MM_EnvironmentBase *env, MM_Scavenger *scavenger)
		: MM_RootScanner(env)
		, _scavenger(scavenger)
		, _rootClearer(env, scavenger)
		, _scavengerDelegate(scavenger->getDelegate())
	{
		_typeId = __FUNCTION__;
		setNurseryReferencesOnly(true);

		/*
		 * In the case of Concurrent Scavenger JNI Weak Global References required to be scanned as a hard root.
		 * The reason for this VM uses elements of table without calling a Read Barrier,
		 * so JNI Weak Global References table should be treated as a hard root until VM code is fixed
		 * and Read Barrier is called for each single object.
		 */
		_jniWeakGlobalReferencesTableAsRoot = _extensions->isConcurrentScavengerEnabled();
	};

	/*
	 * Handle stack and thread slots specially so that we can auto-remember stack-referenced objects
	 */
	virtual void
	doStackSlot(omrobjectptr_t *slotPtr, void *walkState, const void* stackLocation)
	{
		if (_scavenger->isHeapObject(*slotPtr) && !_extensions->heap->objectIsInGap(*slotPtr)) {
			/* heap object - validate and mark */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::COULD_BE_FORWARDED, *slotPtr, stackLocation, walkState).validate(_env));
			_scavenger->copyAndForwardThreadSlot(MM_EnvironmentStandard::getEnvironment(_env), slotPtr);
		} else if (NULL != *slotPtr) {
			/* stack object - just validate */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, *slotPtr, stackLocation, walkState).validate(_env));
		}
	}

	/*
	 * Handle stack and thread slots specially so that we can auto-remember stack-referenced objects
	 */
	virtual void
	doVMThreadSlot(omrobjectptr_t *slotPtr, GC_VMThreadIterator *vmThreadIterator)
	{
		MM_EnvironmentStandard *envStandard = MM_EnvironmentStandard::getEnvironment(_env);
		if (_scavenger->isHeapObject(*slotPtr) && !_extensions->heap->objectIsInGap(*slotPtr)) {
			_scavenger->copyAndForwardThreadSlot(envStandard, slotPtr);
		} else if (NULL != *slotPtr) {
			Assert_GC_true_with_message4(envStandard, (vmthreaditerator_state_monitor_records == vmThreadIterator->getState()),
					"Thread %p structures scan: slot %p has bad value %p, iterator state %d\n", vmThreadIterator->getVMThread(), slotPtr, *slotPtr, vmThreadIterator->getState());
		}
	}

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

#if defined(J9VM_GC_FINALIZATION)
	virtual void
	doFinalizableObject(omrobjectptr_t object)
	{
		Assert_MM_unreachable();
	}

	virtual void
	scanFinalizableObjects(MM_EnvironmentBase *env)
	{
		reportScanningStarted(RootScannerEntity_FinalizableObjects);
		/* synchronization can be expensive so skip it if there's no work to do */
		if (_scavengerDelegate->getShouldScavengeFinalizableObjects()) {
			if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
				scavengeFinalizableObjects(MM_EnvironmentStandard::getEnvironment(env));
				env->_currentTask->releaseSynchronizedGCThreads(env);
			}
		} else {
			/* double check that there really was no work to do */
			Assert_MM_true(!MM_GCExtensions::getExtensions(env)->finalizeListManager->isFinalizableObjectProcessingRequired());
		}
		reportScanningEnded(RootScannerEntity_FinalizableObjects);
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void
	scanClearable(MM_EnvironmentBase *env)
	{
		if(env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
			/* Soft and weak references resurrected by finalization need to be cleared immediately since weak and soft processing has already completed.
			 * This has to be set before unfinalizable (and phantom) processing
			 */
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_soft;
			env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_clear_weak;
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
		Assert_GC_true_with_message(env, env->getGCEnvironment()->_referenceObjectBuffer->isEmpty(), "Non-empty reference buffer in MM_EnvironmentBase* env=%p\n", env);
		_rootClearer.scanClearable(env);
		Assert_GC_true_with_message(env, env->getGCEnvironment()->_referenceObjectBuffer->isEmpty(), "Non-empty reference buffer in MM_EnvironmentBase* env=%p\n", env);
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
		if (_scavenger->isConcurrentInProgress())
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		{
			MM_RootScanner::scanJNIWeakGlobalReferences(env);
		}
	}
	
	virtual void scanRoots(MM_EnvironmentBase *env) 
	{
		MM_RootScanner::scanRoots(env);
		
		startUnfinalizedProcessing(env);
	}

	void
	scavengeRememberedSet(MM_EnvironmentStandard *env)
	{
		reportScanningStarted(RootScannerEntity_ScavengeRememberedSet);
		_scavenger->scavengeRememberedSet(env);
		reportScanningEnded(RootScannerEntity_ScavengeRememberedSet);
	}

	void
	pruneRememberedSet(MM_EnvironmentStandard *env)
	{
		Assert_MM_true(env->getGCEnvironment()->_referenceObjectBuffer->isEmpty());
		_rootClearer.pruneRememberedSet(env);
	}

	void
	rescanThreadSlots(MM_EnvironmentStandard *env)
	{
		MM_ScavengerThreadRescanner threadRescanner(env, _scavenger);
		threadRescanner.scanThreads(env);
	}

	void
	flush(MM_EnvironmentStandard *env)
	{
		/* flush ownable synchronizer object buffer after rebuild the ownableSynchronizerObjectList during main scan phase */
		env->getGCEnvironment()->_ownableSynchronizerObjectBuffer->flush(env);
	}


};
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#endif /* SCAVENGERROOTSCANNER_HPP_ */
