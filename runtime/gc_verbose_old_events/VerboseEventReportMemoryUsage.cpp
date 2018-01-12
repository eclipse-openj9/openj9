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

#include "VerboseEventReportMemoryUsage.hpp"

#include "j9.h"

#include "VerboseManagerOld.hpp"

MM_VerboseEvent*
MM_VerboseEventReportMemoryUsage::newInstance(MM_ReportMemoryUsageEvent* event, J9HookInterface** hookInterface)
{
	MM_VerboseEventReportMemoryUsage *eventObject;
	
	eventObject = (MM_VerboseEventReportMemoryUsage *)MM_VerboseEvent::create(event->currentThread, sizeof(MM_VerboseEventReportMemoryUsage));
	if(NULL != eventObject) {
		new(eventObject) MM_VerboseEventReportMemoryUsage(event, hookInterface);
	}
	return eventObject;
}

void
MM_VerboseEventReportMemoryUsage::formattedOutput(MM_VerboseOutputAgent *agent)
{
	const char* categoryName;
	UDATA indentLevel = _manager->getIndentLevel();

	/* Print report header */
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<memory>");
	
	/* Print statistics for each allocation category */
	indentLevel++;
	
	MM_MemoryStatistics* statistics = _statistics;
	for (UDATA i = 0; i < MM_AllocationCategory::CATEGORY_COUNT; i++) {
		switch (statistics->category) {
			case MM_AllocationCategory::FIXED:
				categoryName = "fixed";
				break;
			case MM_AllocationCategory::WORK_PACKETS:
				categoryName = "workpackets";
				break;
			case MM_AllocationCategory::REFERENCES:
				categoryName = "references";
				break;
			case MM_AllocationCategory::FINALIZE:
				categoryName = "finalize";
				break;
			case MM_AllocationCategory::DIAGNOSTIC:
				categoryName = "diagnostic";
				break;
			case MM_AllocationCategory::REMEMBERED_SET:
				categoryName = "rememberedset";
				break;
			case MM_AllocationCategory::JAVA_HEAP:
				categoryName = "javaheap";
				break;
			case MM_AllocationCategory::OTHER:
				categoryName = "other";
				break;
			default:
				categoryName = "unknown";
		}
		
		agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "<category type=\"%s\" allocatedbytes=\"%zu\" highwater=\"%zu\"/>", 
				categoryName, statistics->allocated, statistics->highwater);

		// Move to the next memory statistics
		statistics++;
	}
	indentLevel--;
	
	/* Print report footer */
	agent->formatAndOutput(static_cast<J9VMThread*>(_omrThread->_language_vmthread), indentLevel, "</memory>");
}
