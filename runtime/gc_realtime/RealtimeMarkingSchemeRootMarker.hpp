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

#if !defined(REALTIMEROOTMARKER_HPP_)
#define REALTIMEROOTMARKER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "EnvironmentRealtime.hpp"
#include "ObjectAllocationInterface.hpp"
#include "RealtimeRootScanner.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "StackSlotValidator.hpp"

/**
 * This scanner will mark objects that pass through its doSlot.
 */
class MM_RealtimeMarkingSchemeRootMarker : public MM_RealtimeRootScanner
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
	MM_RealtimeMarkingSchemeRootMarker(MM_EnvironmentRealtime *env, MM_RealtimeGC *realtimeGC) :
		MM_RealtimeRootScanner(env, realtimeGC)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * This scanner can be instantiated so we must give it a name.
	 */
	virtual const char*
	scannerName()
	{
		return "Mark";
	}
	
	/**
	 * Wraps the scanning of one thread to only happen if it hasn't already occured in this phase of this GC,
	 * also sets the thread up for the upcoming forwarding phase.
	 * @return true if the thread was scanned (the caller should offer to yield), false otherwise.
	 * @see MM_RootScanner::scanOneThread()
	 */
	virtual void
	scanOneThreadImpl(MM_EnvironmentRealtime *env, J9VMThread* walkThread, void* localData)
	{
		MM_EnvironmentRealtime* walkThreadEnv = MM_EnvironmentRealtime::getEnvironment(walkThread->omrVMThread);
		/* Scan the thread by invoking superclass */
		MM_RootScanner::scanOneThread(env, walkThread, localData);

		/*
		 * TODO CRGTMP we should be able to premark the cache instead of flushing the cache
		 * but this causes problems in overflow.  When we have some time we should look into
		 * this again.
		 */
		/*((MM_SegregatedAllocationInterface *)walkThreadEnv->_objectAllocationInterface)->preMarkCache(walkThreadEnv);*/
		walkThreadEnv->_objectAllocationInterface->flushCache(walkThreadEnv);
		/* Disable the double barrier on the scanned thread. */
		_realtimeGC->disableDoubleBarrierOnThread(env, walkThread->omrVMThread);
	}
	
#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(j9object_t object) {
		_markingScheme->markObject(_env, object);
	}
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Simply pass the call on to the RealtimeGC.
	 * @see MM_Metronome::markObject()
	 */
	virtual void
	doSlot(J9Object** slot)
	{
		_markingScheme->markObject(_env, *slot);
	}
	
	virtual void
	doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation)
	{
		J9Object *object = *slotPtr;
		if (_markingScheme->isHeapObject(object)) {
			/* heap object - validate and mark */
			Assert_MM_validStackSlot(MM_StackSlotValidator(0, object, stackLocation, walkState).validate(_env));
			_markingScheme->markObject(_env, object);
		} else if (NULL != object) {
			/* stack object - just validate */
			Assert_MM_validStackSlot(MM_StackSlotValidator(MM_StackSlotValidator::NOT_ON_HEAP, object, stackLocation, walkState).validate(_env));
		}
	}

	virtual void
	doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator) {
		J9Object *object = *slotPtr;
		if (_markingScheme->isHeapObject(object)) {
			_markingScheme->markObject(_env, object);
		} else if (NULL != object) {
			Assert_MM_true(vmthreaditerator_state_monitor_records == vmThreadIterator->getState());
		}
	}

	/**
	 * Scans non-collectable internal objects (immortal) and classes
	 */
	virtual void
	scanIncrementalRoots(MM_EnvironmentRealtime *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (_classDataAsRoots) {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			scanClasses(env);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		} else {
			scanPermanentClasses(env);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		CompletePhaseCode status = scanClassesComplete(env);

		if (complete_phase_ABORT == status) {
			return ;
		}
	}
protected:
private:
};

#endif /* REALTIMEROOTMARKER_HPP_ */

