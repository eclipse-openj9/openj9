
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "j9port.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "GCExtensions.hpp"
#include "EnvironmentBase.hpp"
#include "TgcExtensions.hpp"

/**
 * Concurrent background thread activated.
 * Function called by a hook when the concurrent background thread is started.
 * 
 * @param env The environment of the background thread
 */
static void
tgcHookConcurrentBackgroundThreadActivated(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ConcurrentBackgroundThreadActivatedEvent* event = (MM_ConcurrentBackgroundThreadActivatedEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcConcurrentExtensions *concurrentExtensions = &tgcExtensions->_concurrent;

	concurrentExtensions->gcCountAtBackgroundThreadActivation = 
#if defined(J9VM_GC_MODRON_SCAVENGER)
		extensions->scavengerStats._gcCount +
#endif /* J9VM_GC_MODRON_SCAVENGER */
		extensions->globalGCStats.gcCount;

	tgcExtensions->printf("<CONCURRENT GC BK thread 0x%08.8zx activated after GC(%zu)>\n",
		event->currentThread->_language_vmthread,
		concurrentExtensions->gcCountAtBackgroundThreadActivation
	);
}

/**
 * Concurrent background thread finished.
 * Function called by a hook when the concurrent background thread is finished.
 * 
 * @param env The environment of the background thread
 * @param traceTotal The number of bytes traced by the background thread during concurrent mark
 */
static void
tgcHookConcurrentBackgroundThreadFinished(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ConcurrentBackgroundThreadFinishedEvent* event = (MM_ConcurrentBackgroundThreadFinishedEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcConcurrentExtensions *concurrentExtensions = &tgcExtensions->_concurrent;

	tgcExtensions->printf("<CONCURRENT GC BK thread 0x%08.8zx (started after GC(%zu)) traced %zu>\n",
		event->currentThread->_language_vmthread,
		concurrentExtensions->gcCountAtBackgroundThreadActivation,
		event->traceTotal
	);
}

/**
 * Initialize concurrent tgc tracing.
 * Initializes the TgcConcurrentExtensions object associated with concurrent tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by concurrent tgc tracing.
 */
bool
tgcConcurrentInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_BACKGROUND_THREAD_ACTIVATED, tgcHookConcurrentBackgroundThreadActivated, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_BACKGROUND_THREAD_FINISHED, tgcHookConcurrentBackgroundThreadFinished, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
