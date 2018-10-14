
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
#include "j9port.h"
#include "Tgc.hpp"
#include "mmomrhook.h"

#if defined(J9VM_GC_VLHGC)
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

#include "GlobalAllocationManager.hpp"

/**
 * Report Allocation Context statistics prior to a collection
 */
static void
tgcHookReportAllocationContextStatistics(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	MM_GCExtensions::getExtensions(env)->globalAllocationManager->printAllocationContextStats(env, eventNum, hook);
}


/**
 * Initialize AC tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by AC tgc tracing.
 */
bool
tgcAllocationContextInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;
	
	J9HookInterface** hooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookReportAllocationContextStatistics, OMR_GET_CALLSITE(), NULL);
	(*hooks)->J9HookRegisterWithCallSite(hooks, J9HOOK_MM_OMR_GLOBAL_GC_END, tgcHookReportAllocationContextStatistics, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* J9VM_GC_VLHGC */
