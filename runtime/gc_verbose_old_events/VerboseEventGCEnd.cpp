
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

#include "VerboseEventGCEnd.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseManagerOld.hpp"

void
MM_VerboseEventGCEnd::initialize(void)
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
MM_VerboseEventGCEnd::gcEndFormattedOutput(MM_VerboseOutputAgent *agent)
{
	UDATA indentLevel = _manager->getIndentLevel();
	
	if(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM->memoryManagerFunctions->j9gc_scavenger_enabled(static_cast<J9VMThread*>(_omrThread->_language_vmthread)->javaVM)) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<nursery freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_gcEndData.commonData.nurseryFreeBytes,
			_gcEndData.commonData.nurseryTotalBytes,
			(UDATA) ( ( (U_64) _gcEndData.commonData.nurseryFreeBytes * 100) / (U_64) _gcEndData.commonData.nurseryTotalBytes)
		);
	}

	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<tenured freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" %s>",
		_gcEndData.commonData.tenureFreeBytes,
		_gcEndData.commonData.tenureTotalBytes,
		(UDATA) ( ( (U_64) _gcEndData.commonData.tenureFreeBytes  * 100) / (U_64) _gcEndData.commonData.tenureTotalBytes),
		hasDetailedTenuredOutput() ? "" : "/"
	);
	
	if (hasDetailedTenuredOutput()) {
		_manager->incrementIndent();
		loaFormattedOutput(agent);
		//tlhFormattedOutput(agent);
		_manager->decrementIndent();
	
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</tenured>");
	}
	
	if(_extensions->verboseExtensions) {
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<rememberedset count=\"%zu\" />",					
			_gcEndData.commonData.rememberedSetCount
		);
	}
}

/**
 * Returns true if there are subclauses to the <tenured> clause, or
 * false if the <tenured> clause can stand alone
 */
bool
MM_VerboseEventGCEnd::hasDetailedTenuredOutput()
{
	/* if verbose extensions is on, then tlh and nontlh data will be output */
	if (_extensions->verboseExtensions) {
		return true;
	}
	
#if defined(J9VM_GC_LARGE_OBJECT_AREA)
	if (_gcEndData.commonData.loaEnabled) {
		return true;
	}
#endif /* J9VM_GC_LARGE_OBJECT_AREA */
	
	return false;
}

/**
 * Output details about LOA and SOA spaces for the <tenured> clause.
 * This output is only enabled if LOAs are enabled
 */  
void
MM_VerboseEventGCEnd::loaFormattedOutput(MM_VerboseOutputAgent *agent)
{
#if defined(J9VM_GC_LARGE_OBJECT_AREA)
	if (_gcEndData.commonData.loaEnabled) {
		UDATA indentLevel = _manager->getIndentLevel();
		UDATA tenureSOATotalBytes = _gcEndData.commonData.tenureTotalBytes - _gcEndData.commonData.tenureLOATotalBytes;
		UDATA tenureSOAFreeBytes = _gcEndData.commonData.tenureFreeBytes - _gcEndData.commonData.tenureLOAFreeBytes;
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<soa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			tenureSOAFreeBytes,
			tenureSOATotalBytes,
			(UDATA) ( ( (U_64) tenureSOAFreeBytes  * 100) / (U_64) tenureSOATotalBytes)
		);
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<loa freebytes=\"%zu\" totalbytes=\"%zu\" percent=\"%zu\" />",
			_gcEndData.commonData.tenureLOAFreeBytes,
			_gcEndData.commonData.tenureLOATotalBytes,
			( _gcEndData.commonData.tenureLOATotalBytes == 0 ? 0 : (UDATA) ( ( (U_64) _gcEndData.commonData.tenureLOAFreeBytes  * 100) / (U_64) _gcEndData.commonData.tenureLOATotalBytes))
		);
	}
#endif /* J9VM_GC_LARGE_OBJECT_AREA*/
}
