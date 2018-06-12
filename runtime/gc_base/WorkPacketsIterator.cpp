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

#include <string.h>

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "PacketList.hpp"
#include "Packet.hpp"
#include "WorkPackets.hpp"
#include "WorkPacketsIterator.hpp" 


MM_PacketList *
MM_PacketListIterator::nextPacketList(MM_EnvironmentBase *env)
{
	Assert_MM_true(_nextListIndex < _numPacketLists);
	MM_PacketList *nextListBase = _packetLists[_nextListIndex];
	if (NULL != nextListBase) {
		_nextListIndex += 1;
	}
	return nextListBase;
}

MM_PacketListIterator::MM_PacketListIterator(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
	: MM_BaseNonVirtual()
	, _nextListIndex(0)
{
	_typeId = __FUNCTION__;
	I_32 numLists=0;
	
	/* Initialize storage for the packet lists array*/
	for (I_32 i = 0; i < _numPacketLists +1; i++) {    
		_packetLists[i] = NULL;
	}
	
	if (!workPackets->_fullPacketList.isEmpty()) {	
		_packetLists[numLists++] = &(workPackets->_fullPacketList);
	}
	
	if (!workPackets->_relativelyFullPacketList.isEmpty()) {	
		_packetLists[numLists++] = &(workPackets->_relativelyFullPacketList);
	}
		
	if (!workPackets->_nonEmptyPacketList.isEmpty()) {
		_packetLists[numLists++] = &(workPackets->_nonEmptyPacketList);
	}
		
	if (!workPackets->_deferredPacketList.isEmpty()) {
		_packetLists[numLists++] = &(workPackets->_deferredPacketList);
	}
		
	if (!workPackets->_deferredFullPacketList.isEmpty()) {	
		_packetLists[numLists++] = &(workPackets->_deferredFullPacketList);
	}
	
	Assert_MM_true(numLists <= _numPacketLists);
}


MM_PacketList::PacketSublist *
MM_PacketSublistIterator::nextSublist(MM_EnvironmentBase *env)
{
	MM_PacketList::PacketSublist *sublist = NULL;
	if (NULL != _listToWalk) {
		/* this if is only to protect us against the degenerate case where we were initialized with a NULL list - it is otherwise invariant in the following loop */
		while ((NULL == sublist) && (_nextSublistIndex < _listToWalk->_sublistCount)) {
			sublist = &(_listToWalk->_sublists[_nextSublistIndex]);
			/* we only want to return non-empty sublists so drop this sublist if it is empty */
			if ((NULL != sublist) && (NULL == sublist->_head)) {
				sublist = NULL;
			}
			_nextSublistIndex += 1;
		}
	}
	return sublist;
}

MM_PacketSublistIterator::MM_PacketSublistIterator(MM_EnvironmentBase *env, MM_PacketList *packetList)
	: MM_BaseNonVirtual()
	, _listToWalk(packetList)
	, _nextSublistIndex(0)
{
	_typeId = __FUNCTION__;
}


MM_Packet *
MM_PacketIterator::nextPacket(MM_EnvironmentBase *env)
{
	MM_Packet *packet = _nextPacket;
	if (NULL != _nextPacket) {
		_nextPacket = _nextPacket->_next;
	}
	return packet;
}

MM_PacketIterator::MM_PacketIterator(MM_EnvironmentBase *env, MM_Packet *startPacket)
	: MM_BaseNonVirtual()
	, _nextPacket(startPacket)
{
	_typeId = __FUNCTION__;
}


MM_Packet *
MM_WorkPacketsIterator::nextPacket(MM_EnvironmentBase *env)
{
	MM_Packet *packet = _packetIterator.nextPacket(env);
	
	/* if this packet is NULL, see if we can create a new packet iterator with the next list */
	if (NULL == packet) {
		MM_PacketList::PacketSublist *nextSublist = _packetSublistIterator.nextSublist(env);
		if (NULL == nextSublist) {
			MM_PacketList *nextList = _packetListIterator.nextPacketList(env);
			if (NULL != nextList) {
				_packetSublistIterator = MM_PacketSublistIterator(env, nextList);
				nextSublist = _packetSublistIterator.nextSublist(env);
				/* the packet list iterator will never return an empty list so there must be at least one sublist in this list */
				Assert_MM_true(NULL != nextSublist);
			}
		}
		if (NULL != nextSublist) {
			MM_Packet *nextListBase = nextSublist->_head;
			/* the packet sublist iterator will never return an empty sublist and this is a new sublist (either from our existing iterator or the new one we just created) so it must be non-empty */
			Assert_MM_true(NULL != nextListBase);
			_packetIterator = MM_PacketIterator(env, nextListBase);
			packet = _packetIterator.nextPacket(env);
			/* because of our knowledge of how the packet iterator works, we know that its first returned packet must be the packet used to initialize it */
			Assert_MM_true(packet == nextListBase);
		}
	}
	
	return packet;
}

MM_WorkPacketsIterator::MM_WorkPacketsIterator(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
	: MM_BaseNonVirtual()
	, _packetListIterator(env, workPackets)
	, _packetSublistIterator(env, NULL)
	, _packetIterator(env, NULL)
{
	_typeId = __FUNCTION__;
}
