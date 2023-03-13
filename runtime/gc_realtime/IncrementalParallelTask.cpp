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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "EnvironmentRealtime.hpp"
#include "IncrementalParallelTask.hpp"
#include "ParallelDispatcher.hpp"
#include "Scheduler.hpp"

#include "ModronAssertions.h"

void
MM_IncrementalParallelTask::synchronizeGCThreads(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	if(1 < _totalThreadCount) {
		
		if (env->isMainThread()) {
			/* ignore nested sync points */
			if (_entryCount >= 1) {
				return;
			}
		}		
		omrthread_monitor_enter(_synchronizeMutex);

		/*check synchronization point*/
		if (0 ==_synchronizeCount) {
			_syncPointUniqueId = id;
		} else {
			Assert_MM_true(_syncPointUniqueId == id);
		}

		_synchronizeCount += 1;

		if(_synchronizeCount == _threadCount) {
			_synchronizeCount = 0;
			_synchronizeIndex += 1;
			/* notify all threads last thread synced */
			_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::synchedThreads);
			omrthread_monitor_notify_all(_synchronizeMutex);
		} else {
			volatile uintptr_t index = _synchronizeIndex;
			
			do {
				if (_yieldCollaborator.getYieldCount() + _synchronizeCount >= _threadCount && _yieldCollaborator.getYieldCount() > 0) {					
					if (env->isMainThread()) {
						((MM_Scheduler*)_dispatcher)->condYieldFromGC(env);
					} else {
						/* notify main last thread synced/yielded */
						_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::notifyMain);
						omrthread_monitor_notify_all(_synchronizeMutex);
					}
				}
				
				/* A worker is only interested in synchedThreads event. For any other events it remains to be blocked */		
				/* We check synchronizeIndex, so we can exit this iteration ASAP before synchedThreads is overwritten by another event in the next iteration
				 * (We may be overly cautious here, since we are not that sure that overlap between iterations may even happen)
				 */				
				do {
					env->reportScanningSuspended();
					omrthread_monitor_wait(_synchronizeMutex);
					env->reportScanningResumed();
				} while ((index == _synchronizeIndex) && !env->isMainThread() && (_yieldCollaborator.getResumeEvent() != MM_YieldCollaborator::synchedThreads));

			} while(index == _synchronizeIndex);
		}
		omrthread_monitor_exit(_synchronizeMutex);
	}
}

bool
MM_IncrementalParallelTask::synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	bool isMainThread = false;

	if(1 < _totalThreadCount) {
		volatile uintptr_t index = _synchronizeIndex;

		if (env->isMainThread()) {
			/* This function only has to be re-entrant for the main thread */
			_entryCount += 1;
			if (_entryCount > 1) {
				isMainThread = true;
				goto done;
			}
		}
		
		omrthread_monitor_enter(_synchronizeMutex);

		/*check synchronization point*/
		if (0 == _synchronizeCount) {
			_syncPointUniqueId = id;
		} else {
			Assert_MM_true(_syncPointUniqueId == id);
		}

		_synchronizeCount += 1;
		if(_synchronizeCount == _threadCount) {
			if(env->isMainThread()) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMainThread = true;
				_synchronized = true;
				goto done;
			}
			/* notify main last thread synced */
			_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::notifyMain);
			omrthread_monitor_notify_all(_synchronizeMutex);
		}
		
		while(index == _synchronizeIndex) {
			if(env->isMainThread() && (_synchronizeCount == _threadCount)) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMainThread = true;
				_synchronized = true;
				goto done;
			}
			
			if ((_yieldCollaborator.getYieldCount() + _synchronizeCount >= _threadCount) && (_yieldCollaborator.getYieldCount() > 0)) {
				if (env->isMainThread()) {
					((MM_Scheduler*)_dispatcher)->condYieldFromGC(env);
				} else {
					/* notify main last thread synced/yielded */
					_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::notifyMain);
					omrthread_monitor_notify_all(_synchronizeMutex);
				}
			}
			
			/* A worker is only interested in synchedThreads event. For any other events it remains to be blocked */
			/* We check synchronizeIndex, so we can exit this iteration ASAP before synchedThreads is overwritten by another event in the next iteration
			 * (We may be overly cautious here, since we are not that sure that overlap between iterations may even happen)
			 */
			do {
				env->reportScanningSuspended();
				omrthread_monitor_wait(_synchronizeMutex);
				env->reportScanningResumed();
			} while ((index == _synchronizeIndex) && !env->isMainThread() && (_yieldCollaborator.getResumeEvent() != MM_YieldCollaborator::synchedThreads));
		}
		omrthread_monitor_exit(_synchronizeMutex);
	} else {
		isMainThread = true;
	}

done:
	return isMainThread;	
}

bool
MM_IncrementalParallelTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	/* this task doesn't support synchronizeGCThreadsAndReleaseSingleThread currently */
	Assert_MM_unreachable();
	/* TODO if implementing make sure to fix the isMain check in releaseSynchronizedGCThreads */
	return MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
}

void
MM_IncrementalParallelTask::releaseSynchronizedGCThreads(MM_EnvironmentBase *env)
{
	if(1 == _totalThreadCount) {
		return;
	}
	
	if(env->isMainThread()) {
		/* sync/release sequence actually takes time (excluding the work within the sync/release pair).
		 * Take advantage of the fact the all workers are blocked to check if it is time to yield */
		((MM_Scheduler*)_dispatcher)->condYieldFromGC(env);
		
		/* Could not have gotten here unless all other threads are sync'd - don't check, just release */
		_entryCount -= 1;
		if (_entryCount == 0) {
			_synchronized = false;
			omrthread_monitor_enter(_synchronizeMutex);
			_synchronizeCount = 0;
			_synchronizeIndex += 1;
			_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::synchedThreads);
			omrthread_monitor_notify_all(_synchronizeMutex);
			omrthread_monitor_exit(_synchronizeMutex);
		}
	}
}
