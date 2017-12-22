
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
 * @ingroup GC_Modron_Tarok
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "ModronAssertions.h"

#include "GlobalMarkDelegate.hpp"

#include "CardTable.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderManager.hpp"
#include "ClassLoaderRememberedSet.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentVLHGC.hpp"
#include "FinalizeListManager.hpp"
#include "FinalizerSupport.hpp"
#include "GCExtensions.hpp"
#include "GlobalMarkCardScrubber.hpp"
#include "GlobalMarkingScheme.hpp"
#include "HeapMapIterator.hpp"
#include "InterRegionRememberedSet.hpp"
#include "MarkMap.hpp"
#include "WorkPacketsVLHGC.hpp"


bool
MM_GlobalMarkDelegate::initialize(MM_EnvironmentVLHGC *env)
{
	_javaVM = (J9JavaVM *)env->getLanguageVM();
	_extensions = MM_GCExtensions::getExtensions(env);

	if(NULL == (_markingScheme = MM_GlobalMarkingScheme::newInstance(env))) {
		goto error_no_memory;
	}

	_dispatcher = _extensions->dispatcher;

	return true;

error_no_memory:
	return false;
}

void
MM_GlobalMarkDelegate::tearDown(MM_EnvironmentVLHGC *env)
{
	_dispatcher = NULL;

	if(NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
	}
}

bool
MM_GlobalMarkDelegate::heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress)
{
	return _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);
}

bool
MM_GlobalMarkDelegate::heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	return _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
}

void
MM_GlobalMarkDelegate::performMarkSetInitialState(MM_EnvironmentVLHGC *env)
{
	/* set state to launch incremental mark cycle */
	Assert_MM_true (env->_cycleState->_markDelegateState == MM_CycleState::state_mark_idle);
	env->_cycleState->_markDelegateState = MM_CycleState::state_mark_map_init;
}

void
MM_GlobalMarkDelegate::performMarkInit(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true (MM_CycleState::state_mark_map_init == env->_cycleState->_markDelegateState);
	bool didTimeout = markInit(env, I_64_MAX);
	Assert_MM_false(didTimeout);
	env->_cycleState->_markDelegateState = MM_CycleState::state_initial_mark_roots;
}

void 
MM_GlobalMarkDelegate::performMarkForGlobalGC(MM_EnvironmentVLHGC *env)
{
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._globalMarkIncrementType = MM_VLHGCIncrementStats::mark_global_collection;

	switch (env->_cycleState->_markDelegateState) {
	case MM_CycleState::state_mark_idle:
		/* provide complete mark operation */
		markAll(env);
	    break;
	
	case MM_CycleState::state_mark_map_init:
	{
		bool didTimeout = markInit(env, I_64_MAX);
		Assert_MM_false(didTimeout);
	}
		/* continue in next case - no break here */

	case MM_CycleState::state_initial_mark_roots:
	case MM_CycleState::state_process_work_packets_after_initial_mark:
	case MM_CycleState::state_final_roots_complete:
	{
		/* re-mark the roots, clean any dirty cards, process all work packets, and then perform final mark operations */
		markRoots(env);
		bool didTimeout = markScan(env, I_64_MAX);
		Assert_MM_false(didTimeout);
		markComplete(env);
	}
		break;

	default:
		Assert_MM_unreachable();
	}

	env->_cycleState->_markDelegateState = MM_CycleState::state_mark_idle;
}

/*
 * 	Incremental Mark State Machine:
 * 
 * 			  Each block except final steps must handle two exits:
 * 							1. Operation complete
 * 							2. Timeout detected
 * 
 * 			  Final operations must be executed at once, so we have three final cases:
 * 							1. Complete
 * 							2. Mark Roots, Process Work Packets, Complete
 * 							3. Mark Roots, Cleaning Cards, Process Work Packets, Complete
 * 
 * 			  Operations like "Mark Roots" or "Cleaning Cards" might be joint together with followed "Process Work Packets" in one state
 * 
 * 
 * 				timeout
 * 		Note: ===========>	This is exit on timeout and continue an another incremental step (timeout detected)
 * 
 * 			  ----------->	This is switching _inside_ current incremental step (complete)
 * 
 * 
 * 		:-----------------------: 
 * 		:	  	  IDLE			:
 * 		:-----------------------:
 * 					|
 * 					V
 * 		:-----------------------: timeout
 * 		:   Clear Card Table	:========
 * 		:-----------------------:		|									=================
 * 					|					|									|				|
 * 					V					|									V				|
 * 		:-----------------------:<=======		timeout	:-----------------------:			|
 * 		:   	Mark Init		:======================>:	Mark Init Continue	: 			|
 * 		:-----------------------:						:-----------------------:			|
 * 					|												|		|	timeout		|
 * 					V												|		=================
 * 		:-----------------------:									|		
 * 		:   	Mark Roots		:<-----------------------------------
 * 		:						:		timeout
 * 		:  Process Work Packets :====================================		=================
 * 		:-----------------------:									|		|				|
 * 					|												V		V				|
 * 					|									:-----------------------:	timeout	|
 * 					|									:  Process Work Packets	:============
 * 					|									:-----------------------:
 * 					|												|
 * 					V												V
 * 		:-----------------------:						:-----------------------:	timeout
 * 		:	  Final Complete	:						:		Clean Cards		:====================================	
 * 		:-----------------------:						:  Process Work Packets :									|		=================
 * 					|									:-----------------------:									|		|				|
 * 					V												|												V		V				|
 * 		:-----------------------:									|									:-----------------------:	timeout	|
 * 		:	  end, goto IDLE	:									|									:  Process Work Packets :============
 * 		:-----------------------:									|									:-----------------------:
 *																	|												|
 *																	V												V
 *													:-------------------------------:				:-------------------------------:
 * 													:		Final Mark Roots		:				:		 Final Mark Roots		:
 * 													:								:				:								:
 * 													:	Final Process Work Packets	:				:		Final Cleaning Cards	:
 * 													:								:				:								:				
 * 													:		Final Complete			:				:	Final Process Work Packets	:
 * 													:-------------------------------:				:	                      		:
 * 																	|								:		  Final Complete		:
 * 																	|								:								:
 * 																	|								:-------------------------------:
 * 																	|												|
 * 																	V												V
 * 														:-----------------------:						:-----------------------:
 * 														:	  end, goto IDLE	:						:	end, goto IDLE		:
 * 														:-----------------------:						:-----------------------:
 *
 */
bool 
MM_GlobalMarkDelegate::performMarkIncremental(MM_EnvironmentVLHGC *env, I_64 markIncrementEndTime)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_GlobalMarkDelegate_performMarkIncremental_Entry(env->getLanguageVMThread(), markIncrementEndTime);
	MM_CycleState *cycleState = env->_cycleState;
	bool result = false;
	bool timeout = false;

	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._globalMarkIncrementType = MM_VLHGCIncrementStats::mark_incremental;

	while (!timeout) {
		switch (cycleState->_markDelegateState) {
		case MM_CycleState::state_mark_map_init:
		{
			Trc_MM_GlobalMarkDelegate_performMarkIncremental_State(env->getLanguageVMThread(), "state_mark_map_init", MM_CycleState::state_mark_map_init);

			/* initialize the mark map */
			timeout = markInit(env, markIncrementEndTime);

			Assert_MM_false(timeout);
			/* no timeout - next state initial_mark_roots */
			cycleState->_markDelegateState = MM_CycleState::state_initial_mark_roots;
		}
			break;

		case MM_CycleState::state_initial_mark_roots:
		{
			Trc_MM_GlobalMarkDelegate_performMarkIncremental_State(env->getLanguageVMThread(), "state_initial_mark_roots", MM_CycleState::state_initial_mark_roots);

			/* initialize all the roots */
			markRoots(env);

			timeout = j9time_current_time_millis() >= markIncrementEndTime;
			if (timeout) {
				/* Process work packets at the next incremental mark step */
				cycleState->_markDelegateState = MM_CycleState::state_process_work_packets_after_initial_mark;
			} else {
				/* time still left - process work packets now */
				timeout = markScan(env, markIncrementEndTime);

				if (timeout) {
					/* Continue process work packets at the next incremental step */
					cycleState->_markDelegateState = MM_CycleState::state_process_work_packets_after_initial_mark;
				} else {
					Assert_MM_true(env->_cycleState->_workPackets->isAllPacketsEmpty());
					cycleState->_markDelegateState = MM_CycleState::state_final_roots_complete;
					/* even if no timeout occurred act as if one did so that final root processing runs in a separate increment */
					timeout = true;	
				}
			}
		}
			break;
		
		case MM_CycleState::state_process_work_packets_after_initial_mark:
		{
			Trc_MM_GlobalMarkDelegate_performMarkIncremental_State(env->getLanguageVMThread(), "state_process_work_packets_after_initial_mark", MM_CycleState::state_process_work_packets_after_initial_mark);

			timeout = markScan(env, markIncrementEndTime);

			if (!timeout) {
				Assert_MM_true(env->_cycleState->_workPackets->isAllPacketsEmpty());
				if (_extensions->tarokEnableCardScrubbing) {
					markScrubCardTable(env, markIncrementEndTime);
				}
				cycleState->_markDelegateState = MM_CycleState::state_final_roots_complete;
				/* even if no timeout occurred act as if one did so that final root processing runs in a separate increment */
				timeout = true;	
			}
		}
			break;
		
		case MM_CycleState::state_final_roots_complete:
		{
			Trc_MM_GlobalMarkDelegate_performMarkIncremental_State(env->getLanguageVMThread(), "state_final_roots_complete", MM_CycleState::state_final_roots_complete);

			/* final mark increment - mark roots and complete*/
			/* re-mark the roots and then perform final mark operations */
			markRoots(env);
			bool finalScanDidTimeout = markScan(env, I_64_MAX);
			Assert_MM_false(finalScanDidTimeout);
			markComplete(env);

			/* this is the end of Incremental Mark cycle */
			/* report about it */
			result = true;
			/* switch state for idle state */
			cycleState->_markDelegateState = MM_CycleState::state_mark_idle;
			timeout = true;
		}
			break;
		
		case MM_CycleState::state_mark_idle:
		default:
			Trc_MM_GlobalMarkDelegate_performMarkIncremental_State(env->getLanguageVMThread(), "unexpected", cycleState->_markDelegateState);
			Assert_MM_unreachable();
		}
	}
	
	Trc_MM_GlobalMarkDelegate_performMarkIncremental_Exit(env->getLanguageVMThread(), result ? "true" : "false");
	
	return result;
}

UDATA
MM_GlobalMarkDelegate::performMarkConcurrent(MM_EnvironmentVLHGC *env, UDATA totalBytesToScan, volatile bool *forceExit)
{
	Assert_MM_true(MM_CycleState::state_process_work_packets_after_initial_mark == env->_cycleState->_markDelegateState);
	
	static_cast<MM_CycleStateVLHGC*>(env->_cycleState)->_vlhgcIncrementStats._globalMarkIncrementType = MM_VLHGCIncrementStats::mark_concurrent;

	MM_ConcurrentGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_SCAN, totalBytesToScan, forceExit, env->_cycleState);
	_dispatcher->run(env, &markTask);
	
	UDATA bytesScanned = markTask.getBytesScanned();
	bool didReturnEarly = markTask.didReturnEarly();
	if (!didReturnEarly) {
		/* we finished the task without being forced out so we should proceed to the next state */
		Assert_MM_true(env->_cycleState->_workPackets->isAllPacketsEmpty());
		env->_cycleState->_markDelegateState = MM_CycleState::state_final_roots_complete;
	}
	return bytesScanned;
}

void
MM_GlobalMarkDelegate::markAll(MM_EnvironmentVLHGC *env)
{
	_markingScheme->masterSetupForGC(env);
	/* run the mark */
	MM_ParallelGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_ALL, I_64_MAX, env->_cycleState);
	_dispatcher->run(env, &markTask);

	/* Do any post mark checks */
	_markingScheme->masterCleanupAfterGC(env);
}

bool
MM_GlobalMarkDelegate::markInit(MM_EnvironmentVLHGC *env, I_64 timeThreshold)
{
	_markingScheme->masterSetupForGC(env);
	/* run the mark */
	MM_ParallelGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_INIT, timeThreshold, env->_cycleState);
	_dispatcher->run(env, &markTask);
	
	return markTask.didTimeout();
}

void
MM_GlobalMarkDelegate::markRoots(MM_EnvironmentVLHGC *env)
{
	MM_ParallelGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_ROOTS, I_64_MAX, env->_cycleState);
	_dispatcher->run(env, &markTask);
}

bool
MM_GlobalMarkDelegate::markScan(MM_EnvironmentVLHGC *env, I_64 timeThreshold)
{
	MM_ParallelGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_SCAN, timeThreshold, env->_cycleState);
	_dispatcher->run(env, &markTask);
	
	return markTask.didTimeout();
}

bool
MM_GlobalMarkDelegate::markScrubCardTable(MM_EnvironmentVLHGC *env, I_64 timeThreshold)
{
	MM_ParallelScrubCardTableTask scrubTask(env, _dispatcher, timeThreshold, env->_cycleState);
	_dispatcher->run(env, &scrubTask);
	
	return scrubTask.didTimeout();
}

void
MM_GlobalMarkDelegate::markComplete(MM_EnvironmentVLHGC *env)
{
	MM_ParallelGlobalMarkTask markTask(env, _dispatcher, _markingScheme, MARK_COMPLETE, I_64_MAX, env->_cycleState);
	_dispatcher->run(env, &markTask);

	/* Do any post mark checks */
	_markingScheme->masterCleanupAfterGC(env);
}

void 
MM_GlobalMarkDelegate::postMarkCleanup(MM_EnvironmentVLHGC *env)
{
}
