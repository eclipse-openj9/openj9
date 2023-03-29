/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "TgcRootScanner.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "VMThreadListIterator.hpp"

static void printRootScannerStats(OMR_VMThread *omrVMThread);
static void tgcHookGCEnd(J9HookInterface** hook, uintptr_t eventNumber, void* eventData, void* userData);

/**
 * XML attribute names corresponding to entities in the root scanner enumeration.
 *  
 * @ref RootScannerEntity
 */
const static char *attributeNames[] = {
	"unknown", /* RootScannerEntity_None */
	"scavengerememberedset", /* RootScannerEntity_ScavengeRememberedSet */
	"classes", /* RootScannerEntity_Classes */
	"vmclassslots",	/* RootScannerEntity_VMClassSlots */
	"permanentclasses",	/* RootScannerEntity_PermanentClasses */
	"classloaders", /* RootScannerEntity_ClassLoaders */
	"threads", /* RootScannerEntity_Threads */
	"finalizableobjects", /* RootScannerEntity_FinalizableObjects */
	"unfinalizedobjects", /* RootScannerEntity_UnfinalizedObjects */
	"ownablesynchronizerobjects", /* RootScannerEntity_OwnableSynchronizerObjects */
	"continuationobjects", /* RootScannerEntity_ContinuationObjects */
	"stringtable", /* RootScannerEntity_StringTable */
	"jniglobalrefs", /* RootScannerEntity_JNIGlobalReferences */
	"jniweakglobalrefs", /* RootScannerEntity_JNIWeakGlobalReferences */
	"debuggerrefs", /* RootScannerEntity_DebuggerReferences */
	"debuggerclassrefs", /* RootScannerEntity_DebuggerClassReferences */
	"monitorrefs", /* RootScannerEntity_MonitorReferences */
	"weakrefs", /* RootScannerEntity_WeakReferenceObjects */
	"softrefs", /* RootScannerEntity_SoftReferenceObjects */
	"phantomrefs", /* RootScannerEntity_PhantomReferenceObjects */
	"jvmtiobjecttagtables", /* RootScannerEntity_JVMTIObjectTagTables */
	"noncollectableobjects", /* RootScannerEntity_NonCollectableObjects */
	"rememberedset", /* RootScannerEntity_RememberedSet */
	"memoryareaobjects", /* RootScannerEntity_MemoryAreaObjects */
	"metronomerememberedset", /* RootScannerEntity_MetronomeRememberedSet */
	"classescomplete", /* RootScannerEntity_ClassesComplete */
	"weakrefscomplete", /* RootScannerEntity_WeakReferenceObjectsComplete */
	"softrefscomplete", /* RootScannerEntity_SoftReferenceObjectsComplete */
	"phantomrefscomplete", /* RootScannerEntity_PhantomReferenceObjectsComplete */
	"unfinalizedobjectscomplete", /* RootScannerEntity_UnfinalizedObjectsComplete */
	"ownablesynchronizerobjectscomplete", /* RootScannerEntity_OwnableSynchronizerObjectsComplete */
	"continuationobjectscomplete", /* RootScannerEntity_ContinuationObjectsComplete */
	"monitorlookupcaches", /* RootScannerEntity_MonitorLookupCaches */
	"monitorlookupcachescomplete", /* RootScannerEntity_MonitorLookupCachesComplete */
	"monitorreferenceobjectscomplete", /* RootScannerEntity_MonitorReferenceObjectsComplete */
	"doubleMappedOrVirtualLargeObjectHeapObjects", /* RootScannerEntity_DoubleMappedOrVirtualLargeObjectHeapObjects */
};

bool
tgcRootScannerInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	if (!extensions->rootScannerStatsEnabled) {
		extensions->rootScannerStatsEnabled = true;

		J9HookInterface** mmOmrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
		(*mmOmrHooks)->J9HookRegisterWithCallSite(mmOmrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookGCEnd, OMR_GET_CALLSITE(), NULL);
		(*mmOmrHooks)->J9HookRegisterWithCallSite(mmOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, tgcHookGCEnd, OMR_GET_CALLSITE(), NULL);
	}
	
	return true;
}

static void 
printRootScannerStats(OMR_VMThread *omrVMThread)
{
	J9VMThread *currentThread = (J9VMThread *)MM_EnvironmentBase::getEnvironment(omrVMThread)->getLanguageVMThread();
	PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(currentThread->javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(currentThread);
	char timestamp[32];
	U_64 entityScanTimeTotal[RootScannerEntity_Count] = { 0 };
	J9VMThread *thread;
	
	/* print only if at least one thread reported stats on at least one of its roots */
	if (extensions->rootScannerStatsUsed) {
		OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
		omrstr_ftime_ex(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", j9time_current_time_millis(), OMRSTR_FTIME_FLAG_LOCAL);
		tgcExtensions->printf("<scan timestamp=\"%s\">\n", timestamp);

		GC_VMThreadListIterator threadIterator(currentThread);
		while (NULL != (thread = threadIterator.nextVMThread())) {
			MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(thread->omrVMThread);
	
			/* print stats for this thread only if it reported stats on at least one of its roots */
			if (((GC_WORKER_THREAD == env->getThreadType()) || (thread == currentThread)) && env->_rootScannerStats._statsUsed) {
				tgcExtensions->printf("\t<thread id=\"%zu\"", env->getWorkerID());

				/* Scan collected entity data and print attribute/value pairs for entities that have
				 * collected data.  Skip RootScannerEntity_None, located at index 0. */
				for (uintptr_t entityIndex = 1; entityIndex < RootScannerEntity_Count; entityIndex++) {
					if (0 != env->_rootScannerStats._entityScanTime[entityIndex]) {
						U_64 scanTime = j9time_hires_delta(0, env->_rootScannerStats._entityScanTime[entityIndex], J9PORT_TIME_DELTA_IN_MICROSECONDS);

						tgcExtensions->printf(" %s=\"%llu.%03.3llu\"",
								attributeNames[entityIndex],
								scanTime / 1000,
								scanTime % 1000);

						entityScanTimeTotal[entityIndex] += env->_rootScannerStats._entityScanTime[entityIndex];
					}
				}

				if (extensions->isMetronomeGC()) {
					tgcExtensions->printf(" maxincrementtime=\"%llu.%03.3llu\" maxincremententity=\"%s\"",
								env->_rootScannerStats._maxIncrementTime / 1000,
								env->_rootScannerStats._maxIncrementTime % 1000,
								attributeNames[env->_rootScannerStats._maxIncrementEntity]);
				}

				tgcExtensions->printf("/>\n");

				/* Clear root scanner statistics collected for this thread, so data printed during
				 * this pass will not be duplicated */
				env->_rootScannerStats.clear();
			}
		}

		/* Print totals for each root scanner entity */
		tgcExtensions->printf("\t<total");
		for (uintptr_t entityIndex = 1; entityIndex < RootScannerEntity_Count; entityIndex++) {
			if (0 != entityScanTimeTotal[entityIndex]) {
				U_64 scanTime = j9time_hires_delta(0, entityScanTimeTotal[entityIndex], J9PORT_TIME_DELTA_IN_MICROSECONDS);

				tgcExtensions->printf(" %s=\"%llu.%03.3llu\"",
						attributeNames[entityIndex],
						scanTime / 1000,
						scanTime % 1000);
			}
		}

		tgcExtensions->printf("/>\n</scan>\n");
		extensions->rootScannerStatsUsed = false;
	}
}

static void
tgcHookGCEnd(J9HookInterface** hook, uintptr_t eventNumber, void* eventData, void* userData)
{
	MM_GCCycleEndEvent* event = (MM_GCCycleEndEvent*) eventData;
	
	printRootScannerStats(event->omrVMThread);
}
