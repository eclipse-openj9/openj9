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

#include "omr.h"
#include "omrcfg.h"
#include "omrthread.h"

#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "IncrementalOverflow.hpp"
#include "IncrementalParallelTask.hpp"
#include "RealtimeGC.hpp"
#include "WorkPacketsSATB.hpp"

#include "WorkPacketsRealtime.hpp"

/**
 * Instantiate a MM_WorkPacketsRealtime
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPacketsRealtime *
MM_WorkPacketsRealtime::newInstance(MM_EnvironmentBase *env)
{
	MM_WorkPacketsRealtime *workPackets;
	
	workPackets = (MM_WorkPacketsRealtime *)env->getForge()->allocate(sizeof(MM_WorkPacketsRealtime), MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE());
	if (workPackets) {
		new(workPackets) MM_WorkPacketsRealtime(env);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;	
		}
	}
	
	return workPackets;
}

/**
 * Initialize a MM_WorkPacketsRealtime object
 * @return true on success, false otherwise
 */
bool
MM_WorkPacketsRealtime::initialize(MM_EnvironmentBase *env)
{
	if (!MM_WorkPacketsSATB::initialize(env)) {
		return false;
	}

	if (0 == _extensions->overflowCacheCount) {
		/* If the user has not specified a value for overflowCacheCount
		 * set it to be 5% of the packet slot count.
		 */
		_extensions->overflowCacheCount = (uintptr_t)(_slotsInPacket * 0.05);
	}
		
	return true;
}

/**
 * Destroy the resources a MM_WorkPacketsRealtime is responsible for
 */
void
MM_WorkPacketsRealtime::tearDown(MM_EnvironmentBase *env)
{
	MM_WorkPacketsSATB::tearDown(env);
}

/**
 * Create the overflow handler
 */
MM_WorkPacketOverflow *
MM_WorkPacketsRealtime::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *wp)
{
	return MM_IncrementalOverflow::newInstance(env, wp);
}

/**
 * Get an input packet
 *
 * @return Pointer to an input packet
 */
MM_Packet *
MM_WorkPacketsRealtime::getInputPacket(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = (MM_EnvironmentRealtime *)envBase;
	MM_Packet *packet = NULL;
	bool doneFlag = false;
	volatile uintptr_t doneIndex = _inputListDoneIndex;

	while(!doneFlag) {
		while(inputPacketAvailable(env)) {

			/* Check if the regular cache list has work to be done */
			if(NULL != (packet = getInputPacketNoWait(env))) {
				/* Got a packet.
				 * Check if there are threads waiting that should be notified
				 * because of pending entries
				 */
				if(inputPacketAvailable(env) && _inputListWaitCount) {
					omrthread_monitor_enter(_inputListMonitor);
					if(_inputListWaitCount) {
						_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::newPacket);
						omrthread_monitor_notify(_inputListMonitor);
					}
					omrthread_monitor_exit(_inputListMonitor);
				}

				return packet;
			}
		}

		omrthread_monitor_enter(_inputListMonitor);

		if(doneIndex == _inputListDoneIndex) {
			_inputListWaitCount += 1;

			if(((NULL == env->_currentTask)
			     || (_inputListWaitCount == env->_currentTask->getThreadCount())
			     || env->_currentTask->isSynchronized())
			   && !inputPacketAvailable(env)) {
				_inputListDoneIndex += 1;
				_inputListWaitCount = 0;
				_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::synchedThreads);
				omrthread_monitor_notify_all(_inputListMonitor);
			} else {
				while(!inputPacketAvailable(env) && (_inputListDoneIndex == doneIndex)) {

					/* if all GC threads are blocked or yielded (at least one yielded), it's time for master to know about it */
					if (_yieldCollaborator.getYieldCount() + _inputListWaitCount >= env->_currentTask->getThreadCount() && _yieldCollaborator.getYieldCount() > 0) {
						if (env->isMasterThread()) {
							((MM_Scheduler *)(_extensions->dispatcher))->condYieldFromGC(env);
						} else {
							/* notify master last thread synced/yielded */
							_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::notifyMaster);
							omrthread_monitor_notify_all(_inputListMonitor);
						}
					}

					/* A slave is only interested in synchedThreads and newPacket event. For any other event it remains to be blocked */
					/* We check doneIndex, so we can exit this iteration ASAP before synchedThreads is overwritten by another event in the next iteration
					 * (We may be overly cautious here, since we are not that sure that overlap between iterations may even happen)
					 */
					do {
						env->reportScanningSuspended();
						omrthread_monitor_wait(_inputListMonitor);
						env->reportScanningResumed();
					} while ((_inputListDoneIndex == doneIndex) && !env->isMasterThread() && ((_yieldCollaborator.getResumeEvent() == MM_YieldCollaborator::notifyMaster) || (_yieldCollaborator.getResumeEvent() == MM_YieldCollaborator::fromYield)));
				}
			}
		}

		/* Set the local done flag. If we are done ,exit
		 *  if we are not yet done only decrement the wait count */
		doneFlag = (_inputListDoneIndex != doneIndex);
		if(!doneFlag) {
			_inputListWaitCount -= 1;
		}
		omrthread_monitor_exit(_inputListMonitor);
	}

	assume0(NULL == packet);
	return packet;
}

void
MM_WorkPacketsRealtime::notifyWaitingThreads(MM_EnvironmentBase *env)
{
	/* Added an entry to a null list - notify any other threads that a new entry has appeared on the list */
	omrthread_monitor_enter(_inputListMonitor);
	/* Before doing the actual notify on mutex, set the new packet event */
	_yieldCollaborator.setResumeEvent(MM_YieldCollaborator::newPacket);
	omrthread_monitor_notify(_inputListMonitor);
	omrthread_monitor_exit(_inputListMonitor);
}

