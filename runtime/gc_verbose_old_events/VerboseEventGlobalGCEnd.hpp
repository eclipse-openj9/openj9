
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

#if !defined(EVENT_GLOBAL_GC_END_HPP_)
#define EVENT_GLOBAL_GC_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to the end of a global garbage collection.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventGlobalGCEnd : public MM_VerboseEvent
{
private:
	/**
	 * Passed Data 
	 * @{
	 */
	UDATA _workStackOverflowOccured; /**< flag to indicate if workstack overflow has occured */
	UDATA _workStackOverflowCount; /**< the number of times concurrent work stacks have overflowed */
	UDATA _workpacketCount; /**< the number of workpackets used */
	UDATA _weakReferenceClearCount; /**< number of weak references cleared */
	UDATA _softReferenceClearCount; /**< number of soft references cleared */
	UDATA _dynamicSoftReferenceThreshold; /**< the dynamic soft reference threshold */
	UDATA _softReferenceThreshold; /**< soft reference threshold */
	UDATA _phantomReferenceClearCount; /**< number of phantom references cleared */
	UDATA _finalizerCount; /**< number of finalizers */
	UDATA _nurseryFreeBytes; /**< number of bytes free in the nursery */
	UDATA _nurseryTotalBytes; /**< total number of bytes in the nursery */
	UDATA _tenureFreeBytes; /**< number of bytes free in the tenure area */
	UDATA _tenureTotalBytes; /**< total number of bytes in the tenure area */
	UDATA _loaEnabled; /**< flag to indicate whether or not the LOA is enabled */
	UDATA _tenureLOAFreeBytes; /**< number of bytes free in the LOA */
	UDATA _tenureLOATotalBytes; /**< total number of bytes in the LOA */
	UDATA _fixHeapForWalkReason; /**< reason for fixHeapForWalk (see FixUpReason) */
	U_64 _fixHeapForWalkTime; /**< time spent in fixHeapForWalk in ms */
	/** @} */
 

	/**
	 * Consumed Data 
	 * @{
	 */
	U_64 _globalGCStartTime;
	U_64 _markStartTime;
	U_64 _markEndTime;
	U_64 _sweepStartTime;
	U_64 _sweepEndTime;
	U_64 _compactStartTime;
	U_64 _compactEndTime;
	/** @} */
	
	const char *getFixUpReasonAsString(UDATA reason);

public:

	static MM_VerboseEvent *newInstance(MM_GlobalGCIncrementEndEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };
	
	MM_VerboseEventGlobalGCEnd(MM_GlobalGCIncrementEndEvent *event, J9HookInterface** hookInterface) :
		MM_VerboseEvent(event->omrVMThread, event->timestamp, event->eventid, hookInterface),
		_workStackOverflowOccured(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured()),
		_workStackOverflowCount(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount()),
		_workpacketCount(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow()),
		_weakReferenceClearCount(MM_GCExtensions::getExtensions(event->omrVMThread)->markJavaStats._weakReferenceStats._cleared),
		_softReferenceClearCount(MM_GCExtensions::getExtensions(event->omrVMThread)->markJavaStats._softReferenceStats._cleared),
		_dynamicSoftReferenceThreshold(MM_GCExtensions::getExtensions(event->omrVMThread)->getDynamicMaxSoftReferenceAge()),
		_softReferenceThreshold(MM_GCExtensions::getExtensions(event->omrVMThread)->getMaxSoftReferenceAge()),
		_phantomReferenceClearCount(MM_GCExtensions::getExtensions(event->omrVMThread)->markJavaStats._phantomReferenceStats._cleared),
		_finalizerCount(MM_GCExtensions::getExtensions(event->omrVMThread)->markJavaStats._unfinalizedEnqueued),
		_nurseryFreeBytes(event->commonData->nurseryFreeBytes),
		_nurseryTotalBytes(event->commonData->nurseryTotalBytes),
		_tenureFreeBytes(event->commonData->tenureFreeBytes),
		_tenureTotalBytes(event->commonData->tenureTotalBytes),
		_loaEnabled(event->commonData->loaEnabled),
		_tenureLOAFreeBytes(event->commonData->tenureLOAFreeBytes),
		_tenureLOATotalBytes(event->commonData->tenureLOATotalBytes),
		_fixHeapForWalkReason(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.fixHeapForWalkReason),
		_fixHeapForWalkTime(MM_GCExtensions::getExtensions(event->omrVMThread)->globalGCStats.fixHeapForWalkTime),
		_globalGCStartTime(0),
		_markStartTime(0),
		_markEndTime(0),
		_sweepStartTime(0),
		_sweepEndTime(0),
		_compactStartTime(0),
		_compactEndTime(0)
	{}
};

#endif /* EVENT_GLOBAL_GC_END_HPP_ */
