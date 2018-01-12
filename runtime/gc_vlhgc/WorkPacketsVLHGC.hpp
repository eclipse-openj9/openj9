
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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(WORKPACKETSVLHGC_HPP_)
#define WORKPACKETSVLHGC_HPP_


#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "WorkPackets.hpp"

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Modron_Standard
 */

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_WorkPacketsVLHGC : public MM_WorkPackets
{
private:
	MM_CycleState::CollectionType _markType; /**< The type of mark this workpackets is being used for */
protected:
public:

private:
protected:
	virtual MM_WorkPacketOverflow *createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
public:
	static MM_WorkPacketsVLHGC  *newInstance(MM_EnvironmentBase *env, MM_CycleState::CollectionType markType);
	
	/**
	 * Allows callers to determine if there might be work imbalance problem by letting them see how many
	 * threads are currently awaiting work.  Note that the number returned is highly volatile so it can't
	 * be used to determine stalling behaviour, over the course of a run, only instantaneous imbalance.
	 *
	 * @return The number of threads currently blocked acquiring an input packet
	 */
	UDATA threadsCurrentlyAwaitingWork()
	{
		return _inputListWaitCount;
	}

	/**
	 * Wakes up any threads currently blocked on the work packet monitor.
	 * @param env[in] A GC thread
	 */
	void wakeUpThreads(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPackets object.
	 * @ingroup GC_Modron_Standard methodGroup
	 */
	MM_WorkPacketsVLHGC(MM_EnvironmentBase *env, MM_CycleState::CollectionType markType) :
		MM_WorkPackets(env)
		,_markType(markType)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* WORKPACKETSVLHGC_HPP_ */

