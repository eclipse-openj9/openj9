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


#include "ut_j9mm.h"

#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalGCStats.hpp"
#include "RealtimeSweepTask.hpp"
#include "SweepSchemeRealtime.hpp"

void
MM_RealtimeSweepTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase->getOmrVMThread());
	_sweepScheme->sweep(env);
}

void
MM_RealtimeSweepTask::setup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase->getOmrVMThread());

	env->_sweepStats.clear();

	/* record that this thread is participating in this cycle */
	env->_sweepStats._gcCount = env->getExtensions()->globalGCStats.gcCount;
}

void
MM_RealtimeSweepTask::cleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(envBase->getOmrVMThread());
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	MM_GlobalGCStats *finalGCStats = &env->getExtensions()->globalGCStats;
	finalGCStats->sweepStats.merge(&env->_sweepStats);

	Trc_MM_RealtimeSweepTask_parallelStats(
		env->getLanguageVMThread(),
		(U_32)env->getSlaveID(), 
		(U_32)omrtime_hires_delta(0, env->_sweepStats.idleTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS),
		env->_sweepStats.sweepChunksProcessed, 
		(U_32)omrtime_hires_delta(0, env->_sweepStats.mergeTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS));
}
