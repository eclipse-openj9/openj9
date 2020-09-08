/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#if defined(J9VM_GC_MODRON_SCAVENGER) || defined(J9VM_GC_VLHGC)

#include "HotFieldUtil.hpp"
#include "GCExtensions.hpp"

/* Value used to help with the incrementing of the gc count between hot field sorting for dynamicBreadthFirstScanOrdering */
#define INCREMENT_GC_COUNT_BETWEEN_HOT_FIELD_SORT 100

/* Minimum hotness value for a third hot field offset if depthCopyThreePaths is enabled for dynamicBreadthFirstScanOrdering */
#define MINIMUM_THIRD_HOT_FIELD_HOTNESS 50000

void
MM_HotFieldUtil::sortAllHotFieldData(J9JavaVM *javaVM, uintptr_t gcCount)
{	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	/* update hottest fields for all elements of the hotFieldClassInfoPool where isClassHotFieldListDirty is true */
	if ((NULL != javaVM->hotFieldClassInfoPool) && ((gcCount  % extensions->gcCountBetweenHotFieldSort) == 0)) {
		omrthread_monitor_enter(javaVM->hotFieldClassInfoPoolMutex);
		pool_state hotFieldClassInfoPoolState;
		J9ClassHotFieldsInfo *hotFieldClassInfoTemp = (struct J9ClassHotFieldsInfo*)pool_startDo(javaVM->hotFieldClassInfoPool, &hotFieldClassInfoPoolState);
		
		/* sort hot field list for the class if the hot field list of the class is dirty */
		while ((NULL != hotFieldClassInfoTemp) && (U_8_MAX != hotFieldClassInfoTemp->consecutiveHotFieldSelections)) {
			if (hotFieldClassInfoTemp->isClassHotFieldListDirty) {
				sortClassHotFieldList(javaVM, hotFieldClassInfoTemp);
			}
			hotFieldClassInfoTemp = (struct J9ClassHotFieldsInfo*)pool_nextDo(&hotFieldClassInfoPoolState);
		}
		omrthread_monitor_exit(javaVM->hotFieldClassInfoPoolMutex);
	}
	
	/* If adaptiveGcCountBetweenHotFieldSort, update the gc count required between sorting all hot fields as the application runs longer */
	if ((extensions->adaptiveGcCountBetweenHotFieldSort) && (extensions->gcCountBetweenHotFieldSort < extensions->gcCountBetweenHotFieldSortMax) && ((gcCount % INCREMENT_GC_COUNT_BETWEEN_HOT_FIELD_SORT) == 0)) {
		extensions->gcCountBetweenHotFieldSort++;
	}
	
	/* If hotFieldResettingEnabled, update the gc count required between resetting all hot fields */
	if ((extensions->hotFieldResettingEnabled) && ((gcCount % extensions->gcCountBetweenHotFieldReset) == 0)) {
		resetAllHotFieldData(javaVM);
	}
}

MMINLINE void
MM_HotFieldUtil::sortClassHotFieldList(J9JavaVM *javaVM, J9ClassHotFieldsInfo* hotFieldClassInfo)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	/* store initial hot field offsets before hotFieldClassInfo hot field offsets are updated */
	uint8_t initialHotFieldOffset1 = hotFieldClassInfo->hotFieldOffset1;
	uint8_t initialHotFieldOffset2 = hotFieldClassInfo->hotFieldOffset2;
	uint8_t initialHotFieldOffset3 = hotFieldClassInfo->hotFieldOffset3;

	/* compute and update the hot fields for each class */
	if (1 == hotFieldClassInfo->hotFieldListLength) {
		hotFieldClassInfo->hotFieldOffset1 = hotFieldClassInfo->hotFieldListHead->hotFieldOffset;
	} else {
		J9HotField* currentHotField = hotFieldClassInfo->hotFieldListHead;
		uint64_t hottest = 0;
		uint64_t secondHottest = 0;
		uint64_t thirdHottest = 0;
		uint64_t current = 0;
		while (NULL != currentHotField) {
			if(currentHotField->cpuUtil > extensions->minCpuUtil) {
				current = currentHotField->hotness;
				/* compute the three hottest fields if depthCopyThreePaths is enabled, or the two hottest fields if only depthCopyTwoPaths is enabled, otherwise, compute just the hottest field if both depthCopyTwoPaths and depthCopyThreePaths are disabled */
				if (extensions->depthCopyThreePaths) {
					if (current > hottest) {
						thirdHottest = secondHottest;
						hotFieldClassInfo->hotFieldOffset3 = hotFieldClassInfo->hotFieldOffset2;
						secondHottest = hottest;
						hotFieldClassInfo->hotFieldOffset2 = hotFieldClassInfo->hotFieldOffset1;
						hottest = current;
						hotFieldClassInfo->hotFieldOffset1 = currentHotField->hotFieldOffset;
					} else if (current > secondHottest) {
						thirdHottest = secondHottest;
						hotFieldClassInfo->hotFieldOffset3 = hotFieldClassInfo->hotFieldOffset2;
						secondHottest = current;
						hotFieldClassInfo->hotFieldOffset2 = currentHotField->hotFieldOffset;		
					} else if (current > thirdHottest) {
						thirdHottest = current;
						hotFieldClassInfo->hotFieldOffset3 = currentHotField->hotFieldOffset;
					}
				} else if (extensions->depthCopyTwoPaths) {
					if (current > hottest) {
						secondHottest = hottest;
						hotFieldClassInfo->hotFieldOffset2 = hotFieldClassInfo->hotFieldOffset1;
						hottest = current;
						hotFieldClassInfo->hotFieldOffset1 = currentHotField->hotFieldOffset;
					} else if (current > secondHottest) {
						secondHottest = current;
						hotFieldClassInfo->hotFieldOffset2 = currentHotField->hotFieldOffset;		
					}
				} else if (current > hottest) {
					hottest = current;
					hotFieldClassInfo->hotFieldOffset1 = currentHotField->hotFieldOffset;
				}
			}
			currentHotField = currentHotField->next;
		}
		if (thirdHottest < MINIMUM_THIRD_HOT_FIELD_HOTNESS) { 
			hotFieldClassInfo->hotFieldOffset3 = U_8_MAX;
		}
	}
	/* if permanantHotFields are allowed, update consecutiveHotFieldSelections counter if hot field offsets are the same as the previous time the class hot field list was sorted  */
	if (extensions->allowPermanantHotFields) {
		if ((initialHotFieldOffset1 == hotFieldClassInfo->hotFieldOffset1) && (initialHotFieldOffset2 == hotFieldClassInfo->hotFieldOffset2) && (initialHotFieldOffset3 == hotFieldClassInfo->hotFieldOffset3)) {
			hotFieldClassInfo->consecutiveHotFieldSelections++;
			if (hotFieldClassInfo->consecutiveHotFieldSelections == extensions->maxConsecutiveHotFieldSelections) { 
				hotFieldClassInfo->consecutiveHotFieldSelections = U_8_MAX;
			}
		} else {
			hotFieldClassInfo->consecutiveHotFieldSelections = 0;
		}
	}
	hotFieldClassInfo->isClassHotFieldListDirty = false;
}

/**
 * Reset all hot fields for all classes.
 * Used when dynamicBreadthFirstScanOrdering is enabled and hotFieldResettingEnabled is true.
 *
 * @param javaVM[in] pointer to the J9JavaVM
 */
MMINLINE void
MM_HotFieldUtil::resetAllHotFieldData(J9JavaVM *javaVM)
{
	omrthread_monitor_enter(javaVM->hotFieldClassInfoPoolMutex);
	pool_state hotFieldClassInfoPoolState;
	J9ClassHotFieldsInfo *hotFieldClassInfoTemp = (struct J9ClassHotFieldsInfo *)pool_startDo(javaVM->hotFieldClassInfoPool, &hotFieldClassInfoPoolState);
	while (NULL != hotFieldClassInfoTemp) {
		J9HotField* currentHotField = hotFieldClassInfoTemp->hotFieldListHead;
		while (NULL != currentHotField) {
			currentHotField->hotness = 0;
			currentHotField->cpuUtil = 0;
			currentHotField = currentHotField->next;
		}
		hotFieldClassInfoTemp = (struct J9ClassHotFieldsInfo*)pool_nextDo(&hotFieldClassInfoPoolState);
	}
	omrthread_monitor_exit(javaVM->hotFieldClassInfoPoolMutex);
}

#endif /* J9VM_GC_MODRON_SCAVENGER || J9VM_GC_VLHGC */
