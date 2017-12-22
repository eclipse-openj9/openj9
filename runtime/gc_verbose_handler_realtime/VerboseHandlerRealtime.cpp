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

/* Temporary file to make compilers happy */

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "EnvironmentBase.hpp"
#include "VerboseManager.hpp"
#include "VerboseHandlerOutputRealtime.hpp"
#include "VerboseWriterChain.hpp"

void verboseHandlerCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleCycleStart(hook, eventNum, eventData);
}

void verboseHandlerCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleCycleEnd(hook, eventNum, eventData);
}

void verboseHandlerTriggerStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeTriggerStartEvent* event = (MM_MetronomeTriggerStartEvent*)eventData;
	MM_VerboseHandlerOutput* handler = (MM_VerboseHandlerOutput*)userData;
	MM_VerboseManager* manager = handler->getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	handler->getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), j9time_current_time_millis());
	writer->formatAndOutput(env, 0, "<trigger-start %s />", tagTemplate);
	writer->flush(env);
}

void verboseHandlerTriggerEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeTriggerEndEvent* event = (MM_MetronomeTriggerEndEvent*)eventData;
	MM_VerboseHandlerOutput* handler = (MM_VerboseHandlerOutput*)userData;
	MM_VerboseManager* manager = handler->getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	handler->getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), j9time_current_time_millis());
	writer->formatAndOutput(env, 0, "<trigger-end %s />\n", tagTemplate);
	writer->flush(env);
}

void verboseHandlerIncrementStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeIncrementStartEvent* event = (MM_MetronomeIncrementStartEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerIncrementEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeIncrementEndEvent* event = (MM_MetronomeIncrementEndEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerSyncGCStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeSynchronousGCStartEvent* event = (MM_MetronomeSynchronousGCStartEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerSyncGCEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MetronomeSynchronousGCEndEvent* event = (MM_MetronomeSynchronousGCEndEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerMarkStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MarkStartEvent* event = (MM_MarkStartEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerMarkEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_MarkEndEvent* event = (MM_MarkEndEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerSweepStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepStartEvent* event = (MM_SweepStartEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerSweepEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerClassUnloadingStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ClassUnloadingStartEvent* event = (MM_ClassUnloadingStartEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerClassUnloadingEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ClassUnloadingEndEvent* event = (MM_ClassUnloadingEndEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerOutOFMemory(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_OutOfMemoryEvent* event = (MM_OutOfMemoryEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerUtilTrackerOverflow(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_UtilizationTrackerOverflowEvent* event = (MM_UtilizationTrackerOverflowEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}

void verboseHandlerNonMonotonicTime(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_NonMonotonicTimeEvent* event = (MM_NonMonotonicTimeEvent*) eventData;
	MM_VerboseHandlerOutputRealtime* handler = (MM_VerboseHandlerOutputRealtime*)userData;
	handler->handleEvent(event);
}
