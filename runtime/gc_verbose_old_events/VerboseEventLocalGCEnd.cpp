
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

#include "VerboseEventLocalGCEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"
#include "VerboseEventLocalGCStart.hpp"

#if defined(J9VM_GC_MODRON_SCAVENGER)

/**
 * Create an new instance of a MM_VerboseEventLocalGCEnd event.
 * @param event Pointer to a structure containing the data passed over the hookInterface
 */
MM_VerboseEvent *
MM_VerboseEventLocalGCEnd::newInstance(MM_LocalGCEndEvent *event, J9HookInterface** hookInterface)
{
	MM_VerboseEventLocalGCEnd *eventObject;
	
	eventObject = (MM_VerboseEventLocalGCEnd *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventLocalGCEnd));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventLocalGCEnd(event, hookInterface);
	}
	return eventObject;
}

/**
 * Populate events data fields.
 * The event calls the event stream requesting the address of events it is interested in.
 * When an address is returned it populates itself with the data.
 */
void
MM_VerboseEventLocalGCEnd::consumeEvents(void)
{
	MM_VerboseEventStream *eventStream = _manager->getEventStream();
	MM_VerboseEventLocalGCStart *event = NULL;
	
	if (NULL != (event = (MM_VerboseEventLocalGCStart *)eventStream->returnEvent(J9HOOK_MM_OMR_LOCAL_GC_START, _manager->getHookInterface(), (MM_VerboseEvent *)this))){
		_localGCStartTime = event->getTimeStamp();
	} else {
		//Stream is corrupted, what now?
	}
	
	/* Set last Local collection time */
	_manager->setLastLocalGCTime(_time);
}

/**
 * Passes a format string and data to the output routine defined in the passed output agent.
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventLocalGCEnd::formattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	UDATA gcCount = _localGCCount + _globalGCCount;
	U_64 timeInMicroSeconds;

	/* Information and warning messages */
	if(_rememberedSetOverflowed) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"remembered set overflow detected\" />");
	}
	if((_causedRememberedSetOverflow)
	&& (_rememberedSetOverflowed)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"remembered set overflow triggered\" />");
	}
	if(_scanCacheOverflow) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"scan cache overflow detected\" />");
	}
	if(0 != _failedFlipCount) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<failed type=\"flipped\" objectcount=\"%zu\" bytes=\"%zu\" />",
			_failedFlipCount,
			_failedFlipBytes
		);
	}
	if(0 != _failedTenureCount) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<failed type=\"tenured\" objectcount=\"%zu\" bytes=\"%zu\" />",
			_failedTenureCount,
			_failedTenureBytes
		);
	}
	if(_backout) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"aborted collection\" />", gcCount);
	}

	/* Memory stats */
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<flipped objectcount=\"%zu\" bytes=\"%zu\" />", 
		_flipCount,
		_flipBytes
	);
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured objectcount=\"%zu\" bytes=\"%zu\" />", 
		_tenureCount,
		_tenureBytes
	);

#if defined(J9VM_GC_FINALIZATION)
	if(_finalizerCount > 0) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<finalization objectsqueued=\"%zu\" />",
			_finalizerCount
		);
	}
#endif /* J9VM_GC_FINALIZATION */
	if ((0 != _softReferenceClearCount) || (0 != _weakReferenceClearCount) || (0!= _phantomReferenceClearCount)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<refs_cleared soft=\"%zu\" weak=\"%zu\" phantom=\"%zu\" dynamicSoftReferenceThreshold=\"%zu\" maxSoftReferenceThreshold=\"%zu\" />",
			_softReferenceClearCount,
			_weakReferenceClearCount,
			_phantomReferenceClearCount,
			_dynamicSoftReferenceThreshold,
			_softReferenceThreshold
		);
	}
#if defined(J9VM_GC_TILTED_NEW_SPACE)
	if(_tilted) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<scavenger tiltratio=\"%zu\" />",
			(UDATA) ( (U_64) _nurseryTotalBytes * 100 / ((U_64) _totalMemorySize - _tenureTotalBytes) )
		);
	}
#endif /* J9VM_GC_TILTED_NEW_SPACE */

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<nursery freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" tenureage=\"%zu\" />",
		_nurseryFreeBytes,
		_nurseryTotalBytes,
		(UDATA) ( ( (U_64) _nurseryFreeBytes * 100) / (U_64) _nurseryTotalBytes),
		_tenureAge
	);

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
		
	} else
#endif /* J9VM_GC_LARGE_OBJECT_AREA*/
	{			
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_tenureFreeBytes,
			_tenureTotalBytes,
			(UDATA) ( ( (U_64) _tenureFreeBytes  * 100) / (U_64) _tenureTotalBytes)
		);
	}		

	if (!getTimeDeltaInMicroSeconds(&timeInMicroSeconds, _localGCStartTime, _time)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"clock error detected in time totalms\" />");
	}
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<time totalms=\"%llu.%03.3llu\" />",
		timeInMicroSeconds / 1000,
		timeInMicroSeconds % 1000
	);
	
	_manager->decrementIndent();
	indentLevel = _manager->getIndentLevel();

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</gc>");
}

#endif /* defined(J9VM_GC_MODRON_SCAVENGER)*/
