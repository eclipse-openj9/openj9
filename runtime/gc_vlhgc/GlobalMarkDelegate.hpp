
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

#if !defined(GLOBALMARKDELEGATE_HPP_)
#define GLOBALMARKDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "EnvironmentVLHGC.hpp"

class MM_CycleState;
class MM_Dispatcher;
class MM_InterRegionRememberedSet;
class MM_GCExtensions;
class MM_GlobalMarkingScheme;
class MM_MarkMap;
class MM_WorkPacketsVLHGC;


class MM_GlobalMarkDelegate : public MM_BaseNonVirtual
{
	/* Data Members */
public:
protected:
private:
	J9JavaVM *_javaVM;  /**< Current JVM */
	MM_GCExtensions *_extensions;  /**< Global variable container for GC */
	MM_GlobalMarkingScheme *_markingScheme;  /**< Bit map marking implementation used to perform liveness tracing */
	MM_Dispatcher *_dispatcher;  /**< Dispatcher used for tasks */

	/* Member Functions */
public:

	/**
	 * Initialize any resources required.
	 */
	bool initialize(MM_EnvironmentVLHGC *env);

	/**
	 * Clean up any resources in use.
	 */
	void tearDown(MM_EnvironmentVLHGC *env);

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @return true if operation completes with success
	 */
	bool heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
	 * @param highValidAddress The first valid address following the highest in the heap range being removed
	 * @return true if operation completes with success
	 */
	bool heapRemoveRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	/**
	 * Set initial state for incremental Mark
	 * @param env[in] The master GC thread
	 */
	void performMarkSetInitialState(MM_EnvironmentVLHGC *env);

	/**
	 * Execute complete Mark operation.
	 * Try to use results of Incremental Mark steps done before if any
	 * 
	 * @param env[in] The thread which called driveGlobalCollectMasterThread
	 */
	void performMarkForGlobalGC(MM_EnvironmentVLHGC *env);

	/**
	 * Execute one step of incremental Mark.
	 * 
	 * @param env[in] The thread which called driveGlobalCollectMasterThread
	 * @param markIncrementEndTime[in] end of time interval scheduled for step (as observed by current_time_millis)
	 * @return true if last step of incremental Mark has been executed
	 */
	bool performMarkIncremental(MM_EnvironmentVLHGC *env, I_64 markIncrementEndTime);

	/**
	 * Execute one step of concurrent GMP Mark.
	 * 
	 * @param env[in] The master GC thread for the concurrent operation
	 * @param totalBytesToScan[in] The number of bytes which must be scanned by the concurrent task before it stops attempting to do more work
	 * @param forceExit[in] A "kill switch" which, when set to true, implies that the concurrent operation should terminate (volatile as it is written by another thread)
	 * @return The number of bytes scanned by the concurrent mark task
	 */
	UDATA performMarkConcurrent(MM_EnvironmentVLHGC *env, UDATA totalBytesToScan, volatile bool *forceExit);

	/**
	 * Executes mark initialization only and advances the mark state machine to the root step.  The caller must ensure that the mark state
	 * machine is set to the initial state, prior to calling this method (otherwise, it will assert).
	 * 
	 * @param env[in] The master GC thread
	 */
	void performMarkInit(MM_EnvironmentVLHGC *env);

	/**
	 * Perform cleanup after mark.
	 * This includes reporting object delete events, unloading classes and starting finalization
	 *
	 * @param env[in] The master thread
	 */
	void postMarkCleanup(MM_EnvironmentVLHGC *env);

	MM_GlobalMarkDelegate (MM_EnvironmentBase *env) :
		MM_BaseNonVirtual()
		, _javaVM(NULL)
		, _extensions(NULL)
		, _markingScheme(NULL)
		, _dispatcher(NULL)
	{
		_typeId = __FUNCTION__;
	}

protected:
	
private:

	/**
	 *	Mark operation - Complete Mark operation.
	 */
	void markAll(MM_EnvironmentVLHGC *env);

	/**
	 *	Mark operation - Initialization.
	 *	@param timeThreshold[in] The scheduled end time by which the operation is expected to end, expressed in system milliseconds
	 *	@return True if the operation reached timeThreshold before it completed the operation 
	 */
	bool markInit(MM_EnvironmentVLHGC *env, I_64 timeThreshold);

	/**
	 *	Mark operation - Marking Roots.
	 */
	void markRoots(MM_EnvironmentVLHGC *env);

	/**
	 *	Mark operation - Scan work packets.
	 *	@param timeThreshold[in] The scheduled end time by which the operation is expected to end, expressed in system milliseconds 
	 *	@return True if the operation reached timeThreshold before it completed the operation 
	 */
	bool markScan(MM_EnvironmentVLHGC *env, I_64 timeThreshold);

	/**
	 *	Mark operation - Use any remaining increment time to scrub the card table.
	 *	@param timeThreshold[in] The scheduled end time by which the operation is expected to end, expressed in system milliseconds 
	 *	@return True if the operation reached timeThreshold before it completed the operation 
	 */
	bool markScrubCardTable(MM_EnvironmentVLHGC *env, I_64 timeThreshold);

	/**
	 *	Mark operation - Complete.
	 */
	void markComplete(MM_EnvironmentVLHGC *env);

};

#endif /* GLOBALMARKDELEGATE_HPP_ */
