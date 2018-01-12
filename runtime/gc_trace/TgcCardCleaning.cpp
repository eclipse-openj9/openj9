/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "TgcCardCleaning.hpp"

#if defined(J9VM_GC_HEAP_CARD_TABLE)

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "VMThreadListIterator.hpp"

static void printCardCleaningStats(OMR_VMThread *omrVMThread);
static void tgcHookGlobalGcCycleEnd(J9HookInterface** hook, UDATA eventNumber, void* eventData, void* userData);

UDATA
tgcCardCleaningInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	J9HookInterface** mmOmrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*mmOmrHooks)->J9HookRegisterWithCallSite(mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, tgcHookGlobalGcCycleEnd, OMR_GET_CALLSITE(), NULL);

	return 0;
}

static void 
printCardCleaningStats(OMR_VMThread *omrVMThread)
{
	J9VMThread *currentThread = (J9VMThread *)MM_EnvironmentBase::getEnvironment(omrVMThread)->getLanguageVMThread();
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(currentThread);
	PORT_ACCESS_FROM_VMC(currentThread);
	char timestamp[32];
	
	U_64 totalTime = 0;
	UDATA totalCardsCleaned = 0;
	
	j9str_ftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", j9time_current_time_millis());
	tgcExtensions->printf("<cardcleaning timestamp=\"%s\">\n", timestamp);
	
	GC_VMThreadListIterator threadIterator(currentThread);
	J9VMThread* thread = NULL;
	while (NULL != (thread = threadIterator.nextVMThread())) {
		MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(thread->omrVMThread);

		if ((GC_SLAVE_THREAD == env->getThreadType()) || (thread == currentThread)) {
			U_64 cleanTimeInMicros = j9time_hires_delta(0, env->_cardCleaningStats._cardCleaningTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
			tgcExtensions->printf("\t<thread id=\"%zu\" cardcleaningtime=\"%llu.%03.3llu\" cardscleaned=\"%zu\" />\n", 
					env->getSlaveID(),
					cleanTimeInMicros / 1000,
					cleanTimeInMicros % 1000,
					env->_cardCleaningStats._cardsCleaned);
			
			totalTime += env->_cardCleaningStats._cardCleaningTime;
			totalCardsCleaned += env->_cardCleaningStats._cardsCleaned;
			
			/* Clear card cleaning statistics collected for this thread, so data printed during this pass will not be duplicated */ 
			env->_cardCleaningStats.clear();
		}
	}
	
	/* Print totals for each root scanner entity */ 
	U_64 totalTimeInMicros = j9time_hires_delta(0, totalTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	tgcExtensions->printf("\t<total cardcleaningtime=\"%llu.%03.3llu\" cardscleaned=\"%zu\" />\n", 
			totalTimeInMicros / 1000,
			totalTimeInMicros % 1000,
			totalCardsCleaned);
	
	tgcExtensions->printf("</cardcleaning>\n");
}

static void
tgcHookGlobalGcCycleEnd(J9HookInterface** hook, UDATA eventNumber, void* eventData, void* userData)
{
	MM_GCCycleEndEvent* event = (MM_GCCycleEndEvent*) eventData;
	
	printCardCleaningStats(event->omrVMThread);
}

#endif /* defined(J9VM_GC_HEAP_CARD_TABLE) */
