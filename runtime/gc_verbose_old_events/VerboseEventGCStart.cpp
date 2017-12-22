
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

#include "VerboseEventGCStart.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

void
MM_VerboseEventGCStart::initialize(void)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	_timeInMilliSeconds = omrtime_current_time_millis();
}


/**
 * Output common data for the every GC start event. This method is expected
 * be to invoked by subclasses from their formattedOutput routines
 * 
 * @param agent Pointer to an output agent.
 */
void
MM_VerboseEventGCStart::gcStartFormattedOutput(MM_VerboseOutputAgent *agent)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(_omrThread);
	UDATA indentLevel = _manager->getIndentLevel();

	U_64 exclusiveAccessTimeMicros = omrtime_hires_delta(0, _gcStartData.exclusiveAccessTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	U_64 meanExclusiveAccessIdleTimeMicros = omrtime_hires_delta(0, _gcStartData.meanExclusiveAccessIdleTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);

	char* threadName = getOMRVMThreadName(_gcStartData.lastResponder);
	char escapedThreadName[64];

	escapeXMLString(OMRPORTLIB, escapedThreadName, sizeof(escapedThreadName), threadName, strlen(threadName));
	releaseOMRVMThreadName(_gcStartData.lastResponder);

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel,  "<time exclusiveaccessms=\"%llu.%03.3llu\" meanexclusiveaccessms=\"%llu.%03.3llu\" threads=\"%zu\" lastthreadtid=\"0x%p\" lastthreadname=\"%s\" />",
		exclusiveAccessTimeMicros / 1000,
		exclusiveAccessTimeMicros % 1000,
		meanExclusiveAccessIdleTimeMicros / 1000,
		meanExclusiveAccessIdleTimeMicros % 1000,
		_gcStartData.haltedThreads,
		_gcStartData.lastResponder->_language_vmthread,
		escapedThreadName
	);
	if(_gcStartData.beatenByOtherThread) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<warning details=\"gc start was delayed by previous garbage collections\" />");
	}
	
	if(_extensions->verboseExtensions) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<rememberedset count=\"%zu\" />",					
			_gcStartData.commonData.rememberedSetCount
		);
	}
	
	if(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM->memoryManagerFunctions->j9gc_scavenger_enabled(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<nursery freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_gcStartData.commonData.nurseryFreeBytes,
			_gcStartData.commonData.nurseryTotalBytes,
			(UDATA) ( ( (U_64) _gcStartData.commonData.nurseryFreeBytes * 100) / (U_64) _gcStartData.commonData.nurseryTotalBytes)
		);
	}

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" %s>",
		_gcStartData.commonData.tenureFreeBytes,
		_gcStartData.commonData.tenureTotalBytes,
		(UDATA) ( ( (U_64) _gcStartData.commonData.tenureFreeBytes  * 100) / (U_64) _gcStartData.commonData.tenureTotalBytes),
		hasDetailedTenuredOutput() ? "" : "/"
	);
	
	if (hasDetailedTenuredOutput()) {
		_manager->incrementIndent();
		loaFormattedOutput(agent);
		tlhFormattedOutput(agent);
		_manager->decrementIndent();
	
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</tenured>");
	}
	
}

/**
 * Returns true if there are subclauses to the <tenured> clause, or
 * false if the <tenured> clause can stand alone
 */
bool
MM_VerboseEventGCStart::hasDetailedTenuredOutput()
{
	/* if verbose extensions is on, then tlh and nontlh data will be output */
	if (_extensions->verboseExtensions) {
		return true;
	}
	
#if defined(J9VM_GC_LARGE_OBJECT_AREA)
	if (_gcStartData.commonData.loaEnabled) {
		return true;
	}
#endif /* J9VM_GC_LARGE_OBJECT_AREA */
	
	return false;
}

/**
 * Output details about TLH and non-TLH allocates for the <tenured> clause.
 * This output is only enabled if -Xgc:verboseExtensions is specified on the
 * command line
 */  
void
MM_VerboseEventGCStart::tlhFormattedOutput(MM_VerboseOutputAgent *agent)
{
	if (_extensions->verboseExtensions) {
		UDATA indentLevel = _manager->getIndentLevel();
	
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tlh alloccount=\"%zu\" allocbytes=\"%zu\" requestedbytes=\"%zu\" /> ",
			_gcStartData.tlhAllocCount,
			_gcStartData.tlhAllocBytes,
			_gcStartData.tlhRequestedBytes
		);
#endif

		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<nontlh alloccount=\"%zu\" allocbytes=\"%zu\" />",
			_gcStartData.nonTlhAllocCount,
			_gcStartData.nonTlhAllocBytes
		);
	}
}

/**
 * Output details about LOA and SOA spaces for the <tenured> clause.
 * This output is only enabled if LOAs are enabled
 */  
void
MM_VerboseEventGCStart::loaFormattedOutput(MM_VerboseOutputAgent *agent)
{
#if defined(J9VM_GC_LARGE_OBJECT_AREA)
	if (_gcStartData.commonData.loaEnabled) {
		UDATA indentLevel = _manager->getIndentLevel();
		UDATA tenureSOATotalBytes = _gcStartData.commonData.tenureTotalBytes - _gcStartData.commonData.tenureLOATotalBytes;
		UDATA tenureSOAFreeBytes = _gcStartData.commonData.tenureFreeBytes - _gcStartData.commonData.tenureLOAFreeBytes;
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<soa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			tenureSOAFreeBytes,
			tenureSOATotalBytes,
			(UDATA) ( ( (U_64) tenureSOAFreeBytes  * 100) / (U_64) tenureSOATotalBytes)
		);
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<loa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_gcStartData.commonData.tenureLOAFreeBytes,
			_gcStartData.commonData.tenureLOATotalBytes,
			( _gcStartData.commonData.tenureLOATotalBytes == 0 ? 0 : (UDATA) ( ( (U_64) _gcStartData.commonData.tenureLOAFreeBytes  * 100) / (U_64) _gcStartData.commonData.tenureLOATotalBytes))
		);
	}
#endif /* J9VM_GC_LARGE_OBJECT_AREA*/
}
