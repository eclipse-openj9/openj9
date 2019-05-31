
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "TgcExclusiveaccess.hpp"
#include "j9port.h"
#include "modronopt.h"
#include "mmhook.h"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

/**
 * @todo Provide function documentation
 */
static void
printExclusiveAccessTimes(J9VMThread *vmThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);

	U_64 exlusiveAccessTime = j9time_hires_delta(0, env->getExclusiveAccessTime(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
	/* Note that these values were not being calculated in the GC and have been returning 0 for some time. */
	U_64 preAcquireExclusiveTime = (U_64)0;
	U_64 postAcquireExclusiveTime = (U_64)0;

	tgcExtensions->printf("ExclusiveAccess Time(ms): total=\"%llu.%03.3llu\", preAcquire=\"%llu.%03.3llu\", postAcquire=\"%llu.%03.3llu\"\n",
		exlusiveAccessTime /1000, exlusiveAccessTime % 1000,
		preAcquireExclusiveTime /1000, preAcquireExclusiveTime % 1000,
		postAcquireExclusiveTime /1000, postAcquireExclusiveTime % 1000);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookExclusiveAccess(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ExclusiveAccessEvent* event = (MM_ExclusiveAccessEvent*)eventData;
	J9VMThread* vmThread = static_cast<J9VMThread*>(event->currentThread->_language_vmthread);
	printExclusiveAccessTimes(vmThread);
}

/**
 * @todo Provide function documentation
 */
bool
tgcExclusiveAccessInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS, tgcHookExclusiveAccess, OMR_GET_CALLSITE(), NULL);

	return result;
}
