
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(WORKPACKETSREALTIME_HPP_)
#define WORKPACKETSREALTIME_HPP_

#include "EnvironmentBase.hpp"
#include "WorkPackets.hpp"
#include "YieldCollaborator.hpp"

class MM_IncrementalOverflow;

class MM_WorkPacketsRealtime : public MM_WorkPackets
{
public:
protected:
	MM_YieldCollaborator _yieldCollaborator;
	
private:
	MM_PacketList _inUseBarrierPacketList;  /**< List for packets currently being used for the remembered set*/

public:
	static MM_WorkPacketsRealtime *newInstance(MM_EnvironmentBase *env);
	
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	MM_IncrementalOverflow *getIncrementalOverflowHandler() const { return (MM_IncrementalOverflow*)_overflowHandler; }
	MM_YieldCollaborator *getYieldCollaborator() { return &_yieldCollaborator; }

	virtual MM_Packet *getInputPacket(MM_EnvironmentBase *env);
	
	MMINLINE bool inUsePacketsAvailable(MM_EnvironmentBase *env) { return !_inUseBarrierPacketList.isEmpty();}

	MM_Packet *getBarrierPacket(MM_EnvironmentBase *env);
	void putInUsePacket(MM_EnvironmentBase *env, MM_Packet *packet);
	void removePacketFromInUseList(MM_EnvironmentBase *env, MM_Packet *packet);
	void putFullPacket(MM_EnvironmentBase *env, MM_Packet *packet);
	void moveInUseToNonEmpty(MM_EnvironmentBase *env);

	/**
	 * Create a MM_WorkPacketsRealtime object.
	 */
	MM_WorkPacketsRealtime(MM_EnvironmentBase *env) :
		MM_WorkPackets(env)
		, _yieldCollaborator(&_inputListMonitor, &_inputListWaitCount, MM_YieldCollaborator::WorkPacketsRealtime)
		, _inUseBarrierPacketList(NULL)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual MM_WorkPacketOverflow *createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
	virtual void notifyWaitingThreads(MM_EnvironmentBase *env);
	virtual MM_Packet *getPacketByOverflowing(MM_EnvironmentBase *env);
	virtual float getHeapCapacityFactor(MM_EnvironmentBase *env);
	virtual MM_Packet *getInputPacketFromOverflow(MM_EnvironmentBase *env);

private:
};

#endif /* WORKPACKETSREALTIME_HPP_ */

