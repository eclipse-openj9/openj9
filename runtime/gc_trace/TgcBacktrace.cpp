
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
#include "j9port.h"
#include "modronopt.h"

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

/**
 * @todo Provide function documentation
 */
static void
printVMThreadInformation(J9VMThread *vmThread)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);
	char *threadName = getOMRVMThreadName(vmThread->omrVMThread);
	if (threadName) {
		tgcExtensions->printf("\"%s\" (0x%p)\n", threadName, vmThread->osThread);
	}
	releaseOMRVMThreadName(vmThread->omrVMThread);
}

/**
 * Print J9VMThread information at local garbage collection.
 * Hooked by: J9HOOK_MM_OMR_LOCAL_GC_START
 */
static void
tgcHookLocalGcStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCStartEvent* event = (MM_LocalGCStartEvent*)eventData;
	printVMThreadInformation((J9VMThread *)event->currentThread->_language_vmthread);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookGlobalGcStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	printVMThreadInformation((J9VMThread*)event->currentThread->_language_vmthread);
}

/**
 * Initialize MM_TgcExtensions object fields.
 * Initialize the backtrace fields of the MM_TgcExtensions object.
 */
bool
tgcBacktraceInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookLocalGcStart, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookGlobalGcStart, OMR_GET_CALLSITE(), NULL);

	return result;
}
