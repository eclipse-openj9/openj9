/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include "omrcfg.h"

#include "j9port.h"
#include "modronopt.h"
#include "omrcomp.h"

#include "CopyForwardSchemeTask.hpp"

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
void
MM_CopyForwardSchemeTask::synchronizeGCThreads(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToSyncStallTime(startTime, endTime);
}

bool
MM_CopyForwardSchemeTask::synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToSyncStallTime(startTime, endTime);

	return result;
}

bool
MM_CopyForwardSchemeTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToSyncStallTime(startTime, endTime);

	return result;
}

bool
MM_CopyForwardSchemeTask::synchronizeGCThreadsAndReleaseMainForAbort(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToAbortStallTime(startTime, endTime);

	return result;
}

void
MM_CopyForwardSchemeTask::synchronizeGCThreadsForMark(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToMarkStallTime(startTime, endTime);
}

bool
MM_CopyForwardSchemeTask::synchronizeGCThreadsAndReleaseMainForMark(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToMarkStallTime(startTime, endTime);

	return result;
}

void
MM_CopyForwardSchemeTask::synchronizeGCThreadsForInterRegionRememberedSet(MM_EnvironmentBase *envBase, const char *id)
{
	PORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentVLHGC* env = MM_EnvironmentVLHGC::getEnvironment(envBase);
	U_64 startTime = j9time_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	U_64 endTime = j9time_hires_clock();
	env->_copyForwardStats.addToInterRegionRememberedSetStallTime(startTime, endTime);
}

#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */