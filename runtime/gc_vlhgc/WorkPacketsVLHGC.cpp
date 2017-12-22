
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


#include "j9.h"
#include "j9protos.h"
#include "j9port.h"
#include "omrthread.h"

#include "WorkPacketsVLHGC.hpp"

#include "Debug.hpp"
#include "EnvironmentVLHGC.hpp"
#include "RegionBasedOverflowVLHGC.hpp"

/**
 * Instantiate a MM_WorkPacketsVLHGC
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPacketsVLHGC *
MM_WorkPacketsVLHGC::newInstance(MM_EnvironmentBase *env, MM_CycleState::CollectionType markType)
{
	MM_WorkPacketsVLHGC *workPackets;
	
	workPackets = (MM_WorkPacketsVLHGC *)env->getForge()->allocate(sizeof(MM_WorkPacketsVLHGC), MM_AllocationCategory::WORK_PACKETS, J9_GET_CALLSITE());
	if (NULL != workPackets) {
		new(workPackets) MM_WorkPacketsVLHGC(env, markType);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;	
		}
	}
	
	return workPackets;
}

/**
 * Create the overflow handler
 */
MM_WorkPacketOverflow *
MM_WorkPacketsVLHGC::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *wp)
{
	U_8 flag = MM_RegionBasedOverflowVLHGC::overflowFlagForCollectionType(env, _markType);
	return MM_RegionBasedOverflowVLHGC::newInstance(env, this, flag);
}

void
MM_WorkPacketsVLHGC::wakeUpThreads(MM_EnvironmentBase *env)
{
	MM_WorkPackets::notifyWaitingThreads(env);
}
