
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

#include "BarrierSynchronization.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronTypes.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RememberedSetSATB.hpp"
#include "StaccatoGCDelegate.hpp"
#include "Task.hpp"
#include "WorkPacketsRealtime.hpp"

#include "StaccatoGC.hpp"

/* TuningFork name/version information for gc_staccato */
#define TUNINGFORK_STACCATO_EVENT_SPACE_NAME "com.ibm.realtime.vm.trace.gc.metronome"
#define TUNINGFORK_STACCATO_EVENT_SPACE_VERSION 200

MM_StaccatoGC *
MM_StaccatoGC::newInstance(MM_EnvironmentBase *env)
{
	MM_StaccatoGC *globalGC = (MM_StaccatoGC *)env->getForge()->allocate(sizeof(MM_StaccatoGC), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
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
	
 	_extensions->sATBBarrierRememberedSet = MM_RememberedSetSATB::newInstance(env, _workPackets);
 	if (NULL == _extensions->sATBBarrierRememberedSet) {
 		return false;
 	}
 	
 	return true;
}

void
MM_StaccatoGC::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _extensions->sATBBarrierRememberedSet) {
		_extensions->sATBBarrierRememberedSet->kill(env);
		_extensions->sATBBarrierRememberedSet = NULL;
	}
	
	MM_RealtimeGC::tearDown(env);
}

/**
 * Enables the write barrier, this should be called at the beginning of the mark phase.
 */
void
MM_StaccatoGC::enableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->sATBBarrierRememberedSet->restoreGlobalFragmentIndex(env);
}

/**
 * Disables the write barrier, this should be called at the end of the mark phase.
 */
void
MM_StaccatoGC::disableWriteBarrier(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->sATBBarrierRememberedSet->preserveGlobalFragmentIndex(env);
}

void
MM_StaccatoGC::flushRememberedSet(MM_EnvironmentRealtime *env)
{
	if (_workPackets->inUsePacketsAvailable(env)) {
		_workPackets->moveInUseToNonEmpty(env);
		_extensions->sATBBarrierRememberedSet->flushFragments(env);
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
			_moreTracingRequired |= _staccatoDelegate.doTracing(env);

			/* the workStack and rememberedSet use the same workPackets
			 * as backing store.  If all packets are empty this means the
			 * workStack and rememberedSet processing are both complete.
			 */
			_moreTracingRequired |= !_workPackets->isAllPacketsEmpty();
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	} while(_moreTracingRequired);
}

MM_RealtimeAccessBarrier* 
MM_StaccatoGC::allocateAccessBarrier(MM_EnvironmentBase *env)
{
	return _staccatoDelegate.allocateAccessBarrier(env);
}

void
MM_StaccatoGC::enableDoubleBarrier(MM_EnvironmentBase* env)
{
	_staccatoDelegate.enableDoubleBarrier(env);
}

void
MM_StaccatoGC::disableDoubleBarrierOnThread(MM_EnvironmentBase* env, OMR_VMThread *vmThread)
{
	_staccatoDelegate.disableDoubleBarrierOnThread(env, vmThread);
}

void
MM_StaccatoGC::disableDoubleBarrier(MM_EnvironmentBase* env)
{
	_staccatoDelegate.disableDoubleBarrier(env);
}

