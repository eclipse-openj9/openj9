
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

#if !defined(COPYFORWARDSCHEMETASK_HPP_)
#define COPYFORWARDSCHEMETASK_HPP_

#include "omrcfg.h"

#include "modronopt.h"
#include "omrcomp.h"
#include "omrmodroncore.h"

#include "CopyForwardScheme.hpp"
#include "EnvironmentVLHGC.hpp"
#include "InterRegionRememberedSet.hpp"
#include "ParallelTask.hpp"

class MM_CycleState;

class MM_CopyForwardSchemeTask : public MM_ParallelTask
{
private:
	MM_CopyForwardScheme *_copyForwardScheme;  /**< Tasks controlling scheme instance */
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_SCAVENGE; };

	virtual void run(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
		_copyForwardScheme->workThreadGarbageCollect(env);
	}

	void setup(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);
		if (env->isMainThread()) {
			Assert_MM_true(_cycleState == env->_cycleState);
		} else {
			Assert_MM_true(NULL == env->_cycleState);
			env->_cycleState = _cycleState;
		}
		
		env->_workPacketStats.clear();
		env->_copyForwardStats.clear();
		
		/* record that this thread is participating in this cycle */
		env->_copyForwardStats._gcCount = MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount;
		env->_workPacketStats._gcCount = MM_GCExtensions::getExtensions(env)->globalVLHGCStats.gcCount;
	}

	void cleanup(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

		if (env->isMainThread()) {
			Assert_MM_true(_cycleState == env->_cycleState);
		} else {
			env->_cycleState = NULL;
		}

		env->_lastOverflowedRsclWithReleasedBuffers = NULL;
	}

	void mainCleanup(MM_EnvironmentBase *envBase)
	{
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(envBase);

		MM_GCExtensions::getExtensions(env)->interRegionRememberedSet->resetOverflowedList();
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	bool synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *env, const char *id);
	bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
	
	bool synchronizeGCThreadsAndReleaseMainForAbort(MM_EnvironmentBase *env, const char *id);
	void synchronizeGCThreadsForMark(MM_EnvironmentBase *env, const char *id);
	bool synchronizeGCThreadsAndReleaseMainForMark(MM_EnvironmentBase *env, const char *id);
	void synchronizeGCThreadsForInterRegionRememberedSet(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/**
	 * Create a CopyForwardSchemeTask object.
	 */
	MM_CopyForwardSchemeTask(MM_EnvironmentVLHGC *env, MM_ParallelDispatcher *dispatcher, MM_CopyForwardScheme *copyForwardScheme, MM_CycleState *cycleState) :
		MM_ParallelTask((MM_EnvironmentBase *)env, dispatcher)
		, _copyForwardScheme(copyForwardScheme)
		, _cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* COPYFORWARDSCHEMETASK_HPP_ */