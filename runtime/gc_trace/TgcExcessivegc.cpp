
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
#include "modronopt.h"
#include "mmhook.h"

#include "GCExtensions.hpp"
#include "EnvironmentBase.hpp"
#include "TgcExtensions.hpp"

/**
 * Check for excessive GC. 
 * Function called by a hook when the collector checks for excessive GC
 * 
 */
static void
tgcHookExcessiveGCCheckGCActivity(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ExcessiveGCCheckGCActivityEvent* event = (MM_ExcessiveGCCheckGCActivityEvent *)eventData;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(event->currentThread);
	
	tgcExtensions->printf("\texcessiveGC: gcid=\"%zu\" intimems=\"%llu.%03.3llu\" outtimems=\"%llu.%03.3llu\" percent=\"%2.2f\" averagepercent=\"%2.2f\" \n", 
						event->gcCount,						
						event->gcInTime / 1000, 
						event->gcInTime % 1000,
						event->gcOutTime / 1000, 
						event->gcOutTime % 1000,
						event->newGCPercent,
						event->averageGCPercent);
}

/**
 * Excessive GC raised.
 * Function called by a hook when the collector raises excessive GC condition.
 * 
 */
static void
tgcHookExcessiveGCCheckFreeSpace(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ExcessiveGCCheckFreeSpaceEvent* event = (MM_ExcessiveGCCheckFreeSpaceEvent *)eventData;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(event->currentThread);
		
	tgcExtensions->printf("\texcessiveGC: gcid=\"%zu\" percentreclaimed=\"%2.2f\" freedelta=\"%zu\" activesize=\"%zu\" currentsize=\"%zu\" maxiumumsize=\"%zu\" \n", 
		event->gcCount, 
		event->reclaimedPercent,
		event->freeMemoryDelta, 
		event->activeHeapSize, 
		event->currentHeapSize, 
		event->maximumHeapSize);
}

/**
 * Excessive GC raised.
 * Function called by a hook when the collector raises excessive GC condition.
 * 
 */
static void
tgcHookExcessiveGCRaised(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_ExcessiveGCRaisedEvent* event = (MM_ExcessiveGCRaisedEvent *)eventData;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions((J9VMThread*)(event->currentThread->_language_vmthread));
	
	tgcExtensions->printf("\texcessiveGC: gcid=\"%zu\" percentreclaimed=\"%2.2f\" minimum=\"%2.2f\" excessive gc raised \n", 
					event->gcCount, 
					event->reclaimedPercent, 
					event->triggerPercent);
}

/**
 * Initialize excessive tgc tracing.
 * Initializes the TgcExcessiveGCExtensions object associated with excessive GC  tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by excessive GC tgc tracing. 
 */
bool
tgcExcessiveGCInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	J9HookInterface** publicHooks = J9_HOOK_INTERFACE(extensions->hookInterface);
	J9HookInterface** omrPublicHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_EXCESSIVEGC_CHECK_GC_ACTIVITY, tgcHookExcessiveGCCheckGCActivity, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_EXCESSIVEGC_CHECK_FREE_SPACE, tgcHookExcessiveGCCheckFreeSpace, OMR_GET_CALLSITE(), NULL);
	(*publicHooks)->J9HookRegisterWithCallSite(omrPublicHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, tgcHookExcessiveGCRaised, OMR_GET_CALLSITE(), NULL);

	return result;
}
