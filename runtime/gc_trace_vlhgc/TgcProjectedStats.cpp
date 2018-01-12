
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

#include "Base.hpp"
#include "CollectionSetDelegate.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"

static void reportCompactGroupStats(MM_EnvironmentVLHGC *env);
static void decayPrintValue(MM_EnvironmentVLHGC *env, UDATA value);
static void printPercentage(MM_EnvironmentVLHGC *env, double value);
static void reportAverageAbsoluteDeviationForCompactGroups(MM_EnvironmentVLHGC *env);

static void
decayPrintValue(MM_EnvironmentVLHGC *env, UDATA value)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));

	char unitTable[] = { 'b', 'k', 'm', 'g', 't', 'p', 'e' };
	char *units = &(unitTable[0]);
	UDATA result = value;
	UDATA remainder = 0;

	while(result >= 1024) {
		remainder = result % 1024;
		result /= 1024;
		units += 1;
	}

	if(result >= 100) {
		tgcExtensions->printf("%4zu%c", result, *units);
	} else if (result >= 10) {
		UDATA decimal = (remainder * 10) / 1024;
		tgcExtensions->printf("%2zu.%1.1zu%c", result, decimal, *units);
	} else {
		if(0 == result) {
			tgcExtensions->printf("    0");
		} else {
			UDATA decimal = (remainder * 100) / 1024;
			tgcExtensions->printf("%1zu.%2.2zu%c", result, decimal, *units);
		}
	}
}

static void
printPercentage(MM_EnvironmentVLHGC *env, double value)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));

	tgcExtensions->printf("%4zu%%", (UDATA)(100.0 * value));
}

/**
 * Report dynamic collection set statistics for PGC.
 */
static void
tgcHookReportProjectedStatsStatistics(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_VlhgcGarbageCollectCompletedEvent* event = (MM_VlhgcGarbageCollectCompletedEvent*)eventData;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(event->currentThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_CompactGroupPersistentStats *persistentStats = extensions->compactGroupPersistentStats;

	MM_CompactGroupPersistentStats::deriveProjectedLiveBytesStats(env, persistentStats);

	reportCompactGroupStats(env);
	reportAverageAbsoluteDeviationForCompactGroups(env);
}

/* Calculate the absolute deviation of the regions in every compact group */
static void
reportAverageAbsoluteDeviationForCompactGroups(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_HeapRegionManager *heapRegionManager = extensions->getHeap()->getHeapRegionManager();
	MM_CompactGroupPersistentStats *persistentStats = extensions->compactGroupPersistentStats;
	UDATA regionSize = heapRegionManager->getRegionSize();
	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	UDATA totalAbsoluteDeviation = 0;
	UDATA totalRegionCount = 0;
	
	tgcExtensions->printf("\nCmpt Grp    ");
	for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
		tgcExtensions->printf(" %5zu", tableIndex);
	}

	tgcExtensions->printf("   all");

	tgcExtensions->printf("\n            ");
	for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
		tgcExtensions->printf("------");
	}



	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
	    if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroupIndex) == 0) {
			tgcExtensions->printf("\nAvAbDv  ");
            UDATA allocationContextNumber = MM_CompactGroupManager::getAllocationContextNumberFromGroup(env, compactGroupIndex);
            if (0 == allocationContextNumber) {
                    tgcExtensions->printf("    ");
            } else {
                    tgcExtensions->printf("AC%2zu", allocationContextNumber);
            }
	    }
	
		UDATA liveBytesAbsoluteDeviation = persistentStats[compactGroupIndex]._liveBytesAbsoluteDeviation;
		UDATA regionCount = persistentStats[compactGroupIndex]._regionCount;

		if (0 != regionCount) {
			double averageAbsoluteDeviation = (double)liveBytesAbsoluteDeviation / (double)regionCount;
			double averageAbsoluteDeviationInRegionSize = averageAbsoluteDeviation / regionSize;

			totalAbsoluteDeviation += liveBytesAbsoluteDeviation;
			totalRegionCount += regionCount;

			tgcExtensions->printf(" %.3f", averageAbsoluteDeviationInRegionSize);
		} else {
			tgcExtensions->printf(" NoRgn");
		}
	}

	if (0 != totalRegionCount) {
		double totalAverageAbsoluteDeviation = (double)totalAbsoluteDeviation / (double)totalRegionCount;
		double totalAverageAbsoluteDeviationInRegionSize = totalAverageAbsoluteDeviation / (double)regionSize;
		tgcExtensions->printf(" %.3f", totalAverageAbsoluteDeviationInRegionSize);
	} else {
		tgcExtensions->printf(" NoRgn");
	}

	tgcExtensions->printf("\n");
	/* TODO: show distribution of regions vs their deviations */
}


static void
reportCompactGroupStats(MM_EnvironmentVLHGC *env)
{
    MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
    MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
    MM_CompactGroupPersistentStats *persistentStats = extensions->compactGroupPersistentStats;
    UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);

    /*
     * Dump statistics to the TTY.
     */
    tgcExtensions->printf("\n            ");
    for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
            tgcExtensions->printf(" %5zu", tableIndex);
    }

    tgcExtensions->printf("\n            ");
    for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
            tgcExtensions->printf("------");
    }

    for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
            if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroupIndex) == 0) {
                    tgcExtensions->printf("\nPrLvB   ");
                    UDATA allocationContextNumber = MM_CompactGroupManager::getAllocationContextNumberFromGroup(env, compactGroupIndex);
                    if (0 == allocationContextNumber) {
                            tgcExtensions->printf("    ");
                    } else {
                            tgcExtensions->printf("AC%2zu", allocationContextNumber);
                    }

            }
            tgcExtensions->printf(" ");
            decayPrintValue(env, persistentStats[compactGroupIndex]._projectedLiveBytes);
    }
    for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
            if (MM_CompactGroupManager::getRegionAgeFromGroup(env, compactGroupIndex) == 0) {
                    tgcExtensions->printf("\nPrISR   ");
                    UDATA allocationContextNumber = MM_CompactGroupManager::getAllocationContextNumberFromGroup(env, compactGroupIndex);
                    if (0 == allocationContextNumber) {
                            tgcExtensions->printf("    ");
                    } else {
                            tgcExtensions->printf("AC%2zu", allocationContextNumber);
                    }
            }
            tgcExtensions->printf(" ");
            printPercentage(env, persistentStats[compactGroupIndex]._projectedInstantaneousSurvivalRate);
    }

    tgcExtensions->printf("\n");

}

/**
 * Initialize dynamic collection set tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by dynamic collection set tgc tracing.
 */
bool
tgcProjectedStatsInitialize(J9JavaVM *javaVM)
{
	bool result = true;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	if (0 != (*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_VLHGC_GARBAGE_COLLECT_COMPLETED, tgcHookReportProjectedStatsStatistics, OMR_GET_CALLSITE(), NULL)) {
		result = false;
	}

	return result;
}


