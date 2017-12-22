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
#include "modronopt.h"
#include "mmhook.h"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "VMThreadListIterator.hpp"
#include "TLHAllocationInterface.hpp"
#include "TgcExtensions.hpp"
#include "TgcAllocation.hpp"
#include "HeapStats.hpp"

static void
tgcAllocationPrintStats(OMR_VMThread* omrVMThread)
{
	MM_GCExtensions *ext = MM_GCExtensions::getExtensions(omrVMThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(ext);

	MM_AllocationStats *allocStats = &ext->allocationStats;

	tgcExtensions->printf("---------- Allocation Statistics ----------\n");
#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
	UDATA tlhRefreshCountTotal = allocStats->_tlhRefreshCountFresh + allocStats->_tlhRefreshCountReused;
	UDATA tlhAllocatedTotal = allocStats->tlhBytesAllocated();
	tgcExtensions->printf("TLH Refresh Count Total:       %12zu\n", tlhRefreshCountTotal);
	tgcExtensions->printf("TLH Refresh Count Fresh:       %12zu\n", allocStats->_tlhRefreshCountFresh);
	tgcExtensions->printf("TLH Refresh Count Reused:      %12zu\n", allocStats->_tlhRefreshCountReused);
	tgcExtensions->printf("TLH Refresh Bytes Total:       %12zu\n", tlhAllocatedTotal);
	tgcExtensions->printf("TLH Refresh Bytes Fresh:       %12zu\n", allocStats->_tlhAllocatedFresh);
	tgcExtensions->printf("TLH Discarded Bytes:           %12zu\n", allocStats->_tlhDiscardedBytes);
	tgcExtensions->printf("TLH Refresh Bytes Reused:      %12zu\n", allocStats->_tlhAllocatedReused);
	tgcExtensions->printf("TLH Requested Bytes:           %12zu\n", allocStats->_tlhRequestedBytes);
	tgcExtensions->printf("TLH Max Abandoned List Length: %12zu\n", allocStats->_tlhMaxAbandonedListSize);
#endif /* defined (J9VM_GC_THREAD_LOCAL_HEAP) */
	tgcExtensions->printf("Normal Allocated Count:        %12zu\n", allocStats->_allocationCount);
	tgcExtensions->printf("Normal Allocated Bytes:        %12zu\n", allocStats->_allocationBytes);
}

static void
tgcHookAllocationGlobalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	tgcAllocationPrintStats(event->currentThread);
}

static void
tgcHookAllocationLocalPrintStats(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCStartEvent* event = (MM_LocalGCStartEvent*)eventData;
	tgcAllocationPrintStats(event->currentThread);
}

bool
tgcAllocationInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookAllocationGlobalPrintStats, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookAllocationLocalPrintStats, OMR_GET_CALLSITE(), NULL);

	return result;
}
