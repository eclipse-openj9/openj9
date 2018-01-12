
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 */ 

#include "ut_j9mm.h"

#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "GlobalGCStats.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "RealtimeMarkTask.hpp"

void
MM_RealtimeMarkTask::run(MM_EnvironmentBase *env)
{
	_markingScheme->markLiveObjects(MM_EnvironmentRealtime::getEnvironment(env));
}

void
MM_RealtimeMarkTask::setup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	env->_markStats.clear();
	env->getGCEnvironment()->_markJavaStats.clear();
	env->_workPacketStats.clear();
	
	/* record that this thread is participating in this cycle */
	env->_markStats._gcCount = extensions->globalGCStats.gcCount;
	env->_workPacketStats._gcCount = extensions->globalGCStats.gcCount;

	if(env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
}

void
MM_RealtimeMarkTask::cleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_Environment *gcEnv = env->getGCEnvironment();
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	
	MM_GlobalGCStats *finalGCStats= &extensions->globalGCStats;
	finalGCStats->markStats.merge(&env->_markStats);
	extensions->markJavaStats.merge(&gcEnv->_markJavaStats);
	finalGCStats->workPacketStats.merge(&env->_workPacketStats);

	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
	
	/* record the thread-specific paralellism stats in the trace buffer. This partially duplicates info in -Xtgc:parallel */ 
	Trc_MM_RealtimeMarkTask_parallelStats(
		env->getLanguageVMThread(),
		(U_32)env->getSlaveID(),
		(U_32)j9time_hires_delta(0, env->_workPacketStats._workStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)j9time_hires_delta(0, env->_workPacketStats._completeStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)j9time_hires_delta(0, env->_markStats._syncStallTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
		(U_32)env->_workPacketStats._workStallCount,
		(U_32)env->_workPacketStats._completeStallCount,
		(U_32)env->_markStats._syncStallCount,
		env->_workPacketStats.workPacketsAcquired,
		env->_workPacketStats.workPacketsReleased,
		env->_workPacketStats.workPacketsExchanged,
		gcEnv->_markJavaStats.splitArraysProcessed);
}
