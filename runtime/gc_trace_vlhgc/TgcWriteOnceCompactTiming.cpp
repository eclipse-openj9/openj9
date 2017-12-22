
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
#include "j9port.h"
#include "Tgc.hpp"
#include "mmhook.h"

#include <string.h>

#if defined(J9VM_GC_MODRON_COMPACTION)

#include "Base.hpp"
#include "CollectionSetDelegate.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "VMThreadListIterator.hpp"

static void
tgcHookCompactEndWriteOnceCompactTiming(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_CompactEndEvent* event = (MM_CompactEndEvent*)eventData;
	OMR_VMThread *omrVMThread = event->omrVMThread;
	J9VMThread *vmThread = (J9VMThread *)MM_EnvironmentBase::getEnvironment(omrVMThread)->getLanguageVMThread();
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);
	PORT_ACCESS_FROM_VMC(vmThread);

	tgcExtensions->printf("WriteOnceCompact timing details (times in microseconds):\nThread flush leaftag datainit clearmap remsetclear planning reportmove     move (   stall) fixuppacket fixupleaf fixuproots recyclebits  rebuild (   stall) clearmap rebuildnext\n");

	J9VMThread *walkThread = NULL;
	GC_VMThreadListIterator markThreadListIterator(vmThread);
	while ((walkThread = markThreadListIterator.nextVMThread()) != NULL) {
		MM_EnvironmentVLHGC *env = (MM_EnvironmentVLHGC*)walkThread->gcExtensions;
		if ((walkThread == vmThread) || (env->getThreadType() == GC_SLAVE_THREAD)) {
			tgcExtensions->printf("%5zu: %5llu %7llu %8llu %8llu %11llu %8llu %8llu (%8llu) %11llu %9llu %10llu %11llu", env->getSlaveID(),
					j9time_hires_delta(env->_compactVLHGCStats._flushStartTime, env->_compactVLHGCStats._flushEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._leafTaggingStartTime, env->_compactVLHGCStats._leafTaggingEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._regionCompactDataInitStartTime, env->_compactVLHGCStats._regionCompactDataInitEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._clearMarkMapStartTime, env->_compactVLHGCStats._clearMarkMapEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._rememberedSetClearingStartTime, env->_compactVLHGCStats._rememberedSetClearingEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._planningStartTime, env->_compactVLHGCStats._planningEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._moveStartTime, env->_compactVLHGCStats._moveEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(0, env->_compactVLHGCStats._moveStallTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._fixupExternalPacketsStartTime, env->_compactVLHGCStats._fixupExternalPacketsEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._fixupArrayletLeafStartTime, env->_compactVLHGCStats._fixupArrayletLeafEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._rootFixupStartTime, env->_compactVLHGCStats._rootFixupEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._recycleStartTime, env->_compactVLHGCStats._recycleEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS)
					);
				tgcExtensions->printf(" %8llu (%8llu) %8llu %11llu\n",
					j9time_hires_delta(env->_compactVLHGCStats._rebuildMarkBitsStartTime, env->_compactVLHGCStats._rebuildMarkBitsEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(0, env->_compactVLHGCStats._rebuildStallTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._finalClearNextMarkMapStartTime, env->_compactVLHGCStats._finalClearNextMarkMapEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS),
					j9time_hires_delta(env->_compactVLHGCStats._rebuildNextMarkMapStartTime, env->_compactVLHGCStats._rebuildNextMarkMapEndTime, J9PORT_TIME_DELTA_IN_MICROSECONDS)
					);
			
		}
	}
}

/**
 * Initialize write once compact timing tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by write once compact timing tgc tracing.
 */
bool
tgcWriteOnceCompactTimingInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_COMPACT_END, tgcHookCompactEndWriteOnceCompactTiming, OMR_GET_CALLSITE(), NULL);

	return result;
}
#endif /* J9VM_GC_MODRON_COMPACTION */
