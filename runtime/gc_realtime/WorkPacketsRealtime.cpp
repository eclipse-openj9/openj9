
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "j9protos.h"
#include "j9port.h"
#include "omrthread.h"

#include "Debug.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "IncrementalOverflow.hpp"
#include "IncrementalParallelTask.hpp"
#include "RememberedSetSATB.hpp"
#include "RealtimeGC.hpp"
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
	
	workPackets = (MM_WorkPacketsRealtime *)env->getForge()->allocate(sizeof(MM_WorkPacketsRealtime), MM_AllocationCategory::WORK_PACKETS, J9_GET_CALLSITE());
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
	if (!MM_WorkPackets::initialize(env)) {
		return false;
	}

	if (0 == MM_GCExtensions::getExtensions(_extensions)->overflowCacheCount) {
		/* If the user has not specified a value for overflowCacheCount
		 * set it to be 5% of the packet slot count.
		 */
		MM_GCExtensions::getExtensions(_extensions)->overflowCacheCount = (UDATA)(_slotsInPacket * 0.05);
	}

	if (!_inUseBarrierPacketList.initialize(env)) {
		return false;
	}
		
	return true;
}

/**
 * Destroy the resources a MM_WorkPacketsRealtime is responsible for
 */
void
MM_WorkPacketsRealtime::tearDown(MM_EnvironmentBase *env)
{
	MM_WorkPackets::tearDown(env);

	_inUseBarrierPacketList.tearDown(env);
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
MM_WorkPacketsRealtime::getInputPacket(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;
	bool doneFlag = false;
	volatile UDATA doneIndex = _inputListDoneIndex;

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
						omrthread_monitor_wait(_inputListMonitor);
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

/**
 * Return an empty packet for barrier processing.
 * If the emptyPacketList is empty then overflow a full packet.
 */
MM_Packet *
MM_WorkPacketsRealtime::getBarrierPacket(MM_EnvironmentBase *env)
{
	MM_Packet *barrierPacket = NULL;

	/* Check the free list */
	barrierPacket = getPacket(env, &_emptyPacketList);
	if(NULL != barrierPacket) {
		return barrierPacket;
	}

	barrierPacket = getPacketByAdddingWorkPacketBlock(env);
	if (NULL != barrierPacket) {
		return barrierPacket;
	}

	/* Adding a block of packets failed so move on to overflow processing */
	return getPacketByOverflowing(env);
}

/**
 * Get a packet by overflowing a full packet or a barrierPacket
 *
 * @return pointer to a packet, or NULL if no packets could be overflowed
 */
MM_Packet *
MM_WorkPacketsRealtime::getPacketByOverflowing(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;

	if (NULL != (packet = getPacket(env, &_fullPacketList))) {
		/* Attempt to overflow a full mark packet.
		 * Move the contents of the packet to overflow.
		 */
		emptyToOverflow(env, packet, OVERFLOW_TYPE_WORKSTACK);

		omrthread_monitor_enter(_inputListMonitor);

		/* Overflow was created - alert other threads that are waiting */
		if(_inputListWaitCount > 0) {
			omrthread_monitor_notify(_inputListMonitor);
		}
		omrthread_monitor_exit(_inputListMonitor);
	} else {
		/* Try again to get a packet off of the emptyPacketList as another thread
		 * may have emptied a packet.
		 */
		packet = getPacket(env, &_emptyPacketList);
	}

	return packet;
}

/**
 * Put the packet on the inUseBarrierPacket list.
 * @param packet the packet to put on the list
 */
void
MM_WorkPacketsRealtime::putInUsePacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_inUseBarrierPacketList.push(env, packet);
}

void
MM_WorkPacketsRealtime::removePacketFromInUseList(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_inUseBarrierPacketList.remove(packet);
}

void
MM_WorkPacketsRealtime::putFullPacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_fullPacketList.push(env, packet);
}

/**
 * Move all of the packets from the inUse list to the processing list
 * so they are available for processing.
 */
void
MM_WorkPacketsRealtime::moveInUseToNonEmpty(MM_EnvironmentBase *env)
{
	MM_Packet *head, *tail;
	UDATA count;
	bool didPop;

	/* pop the inUseList */
	didPop = _inUseBarrierPacketList.popList(&head, &tail, &count);
	/* push the values from the inUseList onto the processingList */
	if (didPop) {
		_nonEmptyPacketList.pushList(head, tail, count);
	}
}

/**
 * Return the heap capactify factor used to determine how many packets to create
 *
 * @return the heap capactify factor
 */
float
MM_WorkPacketsRealtime::getHeapCapacityFactor(MM_EnvironmentBase *env)
{
	/* Increase the factor for staccato since more packets are required */
	return (float)0.008;
}

/**
 * Get an input packet from the current overflow handler
 *
 * @return a packet if one is found, NULL otherwise
 */
MM_Packet *
MM_WorkPacketsRealtime::getInputPacketFromOverflow(MM_EnvironmentBase *env)
{
	MM_Packet *overflowPacket;

	/* Staccato spec cannot loop here as all packets may currently be on
	 * the InUseBarrierList.  If all packets are on the InUseBarrierList then this
	 * would turn into an infinite busy loop.
	 * while(!_overflowHandler->isEmpty()) {
	 */
	if(!_overflowHandler->isEmpty()) {
		if(NULL != (overflowPacket = getPacket(env, &_emptyPacketList))) {

			_overflowHandler->fillFromOverflow(env, overflowPacket);

			if(overflowPacket->isEmpty()) {
				/* If we didn't end up filling the packet with anything, don't return it and try again */
				putPacket(env, overflowPacket);
			} else {
				return overflowPacket;
			}
		}
	}

	return NULL;
}
