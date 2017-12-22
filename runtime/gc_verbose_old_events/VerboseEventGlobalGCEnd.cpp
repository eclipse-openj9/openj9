
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
 
#include "VerboseEventGlobalGCEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"
#include "VerboseEventGlobalGCStart.hpp"
#include "VerboseEventMarkStart.hpp"
#include "VerboseEventMarkEnd.hpp"
#include "VerboseEventSweepStart.hpp"
#include "VerboseEventSweepEnd.hpp"
#include "VerboseEventCompactStart.hpp"
#include "VerboseEventCompactEnd.hpp"

class MM_VerboseEventCopyForwardAbort;

/**
 * Create an new instance of a MM_VerboseEventGlobalGCEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventGlobalGCEnd::newInstance(MM_GlobalGCIncrementEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventGlobalGCEnd *eventObject;
	
	eventObject = (MM_VerboseEventGlobalGCEnd *)MM_VerboseEvent::create(event->omrVMThread, sizeof(MM_VerboseEventGlobalGCEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventGlobalGCEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventGlobalGCEnd::consumeEvents(void)
{
	MM_VerboseEventStream *eventStream = _manager->getEventStream();
	
	MM_VerboseEventGlobalGCStart *eventGGCStart = NULL;
	MM_VerboseEventMarkStart *eventMarkStart = NULL;
	MM_VerboseEventMarkEnd *eventMarkEnd = NULL;
	MM_VerboseEventSweepStart *eventSweepStart = NULL;
	MM_VerboseEventSweepEnd *eventSweepEnd = NULL;
#if defined(J9VM_GC_MODRON_COMPACTION)
	MM_VerboseEventCompactStart *eventCompactStart = NULL;
	MM_VerboseEventCompactEnd *eventCompactEnd = NULL;
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
	/* Consume Events */
	if (NULL != (eventGGCStart = (MM_VerboseEventGlobalGCStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		_globalGCStartTime = eventGGCStart->getTimeStamp();
	} else {
		//Stream is corrupted, what now?
	}
	
	if (NULL != (eventMarkStart = (MM_VerboseEventMarkStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_MARK_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		_markStartTime = eventMarkStart->getTimeStamp();
		if (NULL != (eventMarkEnd = (MM_VerboseEventMarkEnd *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_MARK_END, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
			_markEndTime = eventMarkEnd->getTimeStamp();
		} else {
			//Stream is corrupted, what now?
		}
	} else {
		//Stream is corrupted, what now?
	}
	
	if (NULL != (eventSweepStart = (MM_VerboseEventSweepStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_SWEEP_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
		_sweepStartTime = eventSweepStart->getTimeStamp();
		if (NULL != (eventSweepEnd = (MM_VerboseEventSweepEnd *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_SWEEP_END, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this))){
			_sweepEndTime = eventSweepEnd->getTimeStamp();
		} else {
			//Stream is corrupted, what now?
		}
	} else {
		//Stream is corrupted, what now?
	}
#if defined(J9VM_GC_MODRON_COMPACTION)
	if (NULL != (eventCompactStart = (MM_VerboseEventCompactStart *)eventStream->returnEvent(J9HOOK_MM_PRIVATE_COMPACT_START, _manager->getPrivateHookInterface(), (MM_VerboseEvent *)this, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, _manager->getPrivateHookInterface()))){
		_compactStartTime = eventCompactStart->getTimeStamp();
		if (NULL != (eventCompactEnd = (MM_VerboseEventCompactEnd *)eventStream->returnEvent(J9HOOK_MM_OMR_COMPACT_END, _manager->getOMRHookInterface(), (MM_VerboseEvent *)this))){
			_compactEndTime = eventCompactEnd->getTimeStamp();
		} else {
			//Stream is corrupted, what now?
		}
	}
#endif /* defined(J9VM_GC_MODRON_COMPACTION) */
	/* Set last Global collection time */
	_manager->setLastGlobalGCTime(_time);
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventGlobalGCEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();

	if(_workStackOverflowOccured) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"work stack overflow\" count=\"%zu\" packetcount=\"%zu\" />",
			_workStackOverflowCount,
			_workpacketCount
		);
	}
	
#if defined(J9VM_GC_FINALIZATION)
	if(_finalizerCount > 0) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<finalization objectsqueued=\"%zu\" />",
			_finalizerCount
		);
	}
#endif /* J9VM_GC_FINALIZATION  */

	if ( (_extensions->verboseExtensions) && (_fixHeapForWalkReason != FIXUP_NONE) ) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<fixup reason=\"%s\" timems=\"%llu.%03.3llu\" />",
			getFixUpReasonAsString(_fixHeapForWalkReason),
			_fixHeapForWalkTime / 1000,
			_fixHeapForWalkTime % 1000
		);
	}
	
	U_64 markTimeInMicroSeconds = 0;
	if (!getTimeDeltaInMicroSeconds(&markTimeInMicroSeconds, _markStartTime, _markEndTime)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in timems mark\" />");
	}
	U_64 sweepTimeInMicroSeconds = 0;
	if (!getTimeDeltaInMicroSeconds(&sweepTimeInMicroSeconds, _sweepStartTime, _sweepEndTime)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in timems sweep\" />");
	}
	U_64 globalGCTimeInMicroSeconds = 0;
	if (!getTimeDeltaInMicroSeconds(&globalGCTimeInMicroSeconds, _globalGCStartTime, _time)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in timems total\" />");
	}
#if defined(J9VM_GC_MODRON_COMPACTION)
	U_64 compactTimeInMicroSeconds = 0;
	if (!getTimeDeltaInMicroSeconds(&compactTimeInMicroSeconds, _compactStartTime, _compactEndTime)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in timems compact\" />");
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<timesms mark=\"%llu.%03.3llu\" sweep=\"%llu.%03.3llu\" compact=\"%llu.%03.3llu\" total=\"%llu.%03.3llu\" />",
		markTimeInMicroSeconds / 1000, 
		markTimeInMicroSeconds % 1000,
		sweepTimeInMicroSeconds / 1000,
		sweepTimeInMicroSeconds % 1000,
		compactTimeInMicroSeconds / 1000,
		compactTimeInMicroSeconds % 1000,
		globalGCTimeInMicroSeconds / 1000, 
		globalGCTimeInMicroSeconds % 1000
	);
#else
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<timesms mark=\"%llu.%03.3llu\" sweep=\"%llu.%03.3llu\" total=\"%llu.%03.3llu\" />",
		markTimeInMicroSeconds / 1000, 
		markTimeInMicroSeconds % 1000,
		sweepTimeInMicroSeconds / 1000,
		sweepTimeInMicroSeconds % 1000,
		globalGCTimeInMicroSeconds / 1000, 
		globalGCTimeInMicroSeconds % 1000
	);
#endif /* J9VM_GC_MODRON_COMPACTION */

	if ((0 != _softReferenceClearCount) || (0 != _weakReferenceClearCount) || (0!= _phantomReferenceClearCount)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<refs_cleared soft=\"%zu\" weak=\"%zu\" phantom=\"%zu\" dynamicSoftReferenceThreshold=\"%zu\" maxSoftReferenceThreshold=\"%zu\" />",
			_softReferenceClearCount,
			_weakReferenceClearCount,
			_phantomReferenceClearCount,
			_dynamicSoftReferenceThreshold,
			_softReferenceThreshold
		);
	}

	if(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM->memoryManagerFunctions->j9gc_scavenger_enabled(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<nursery freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_nurseryFreeBytes,
			_nurseryTotalBytes,
			(UDATA) ( ( (U_64) _nurseryFreeBytes * 100) / (U_64) _nurseryTotalBytes)
		);
	}
	
#if defined(J9VM_GC_LARGE_OBJECT_AREA) 		
	if (_loaEnabled) {
		UDATA tenureSOATotalBytes = _tenureTotalBytes - _tenureLOATotalBytes;
		UDATA tenureSOAFreeBytes = _tenureFreeBytes - _tenureLOAFreeBytes;
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" >",
			_tenureFreeBytes,
			_tenureTotalBytes,
			(UDATA) ( ( (U_64) _tenureFreeBytes  * 100) / (U_64) _tenureTotalBytes)
		);
		
		_manager->incrementIndent();
		indentLevel = _manager->getIndentLevel();
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<soa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			tenureSOAFreeBytes,
			tenureSOATotalBytes,
			(UDATA) ( ( (U_64) tenureSOAFreeBytes  * 100) / (U_64) tenureSOATotalBytes)
		);
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<loa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_tenureLOAFreeBytes,
			_tenureLOATotalBytes,
			( _tenureLOATotalBytes == 0 ? 0 : (UDATA) ( ( (U_64) _tenureLOAFreeBytes  * 100) / (U_64) _tenureLOATotalBytes))
		);
		
		_manager->decrementIndent();
		indentLevel = _manager->getIndentLevel();
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</tenured>");
		
	} else {
#else
	{
#endif /* J9VM_GC_LARGE_OBJECT_AREA */				
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
				_tenureFreeBytes,
				_tenureTotalBytes,
			(UDATA) ( ( (U_64) _tenureFreeBytes * 100) / (U_64) _tenureTotalBytes)
		);
	}	

	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();
	
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</gc>");
}

/**
 * Return the reason for compaction as a string
 * @param reason reason code
 */
const char *
MM_VerboseEventGlobalGCEnd::getFixUpReasonAsString(UDATA reason)
{
	switch((FixUpReason)reason) {
		case FIXUP_NONE:
			return "no fixup";
		case FIXUP_CLASS_UNLOADING:
			return "class unloading";
		case FIXUP_DEBUG_TOOLING:
			return "debug tooling";
		default:
			return "unknown";
	}
}
