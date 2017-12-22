
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

#include "BaseNonVirtual.hpp"
#include "CollectionSetDelegate.hpp"
#include "CompactGroupManager.hpp"
#include "CompactGroupPersistentStats.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "HeapRegionIteratorVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

#define MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH 10

class MM_TgcDynamicCollectionSetData : public MM_BaseNonVirtual {
public:
protected:
private:
	struct MM_TgcDynamicCollectionSetHistoryElement {
		/* Direct measured statistics */
		UDATA regionCount;  /**< Number of regions that appear in the given group */

		/* Historical average of statistics */
		double avgHis_regionCount;  /**< Number of regions that appear in the given group */

		/* Local history average of statistics */
		double avgLcl_regionCount;  /**< Number of regions that appear in the given group */
	};
	MM_TgcDynamicCollectionSetHistoryElement *_historyTable[MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH];

	bool _historyPrimed;  /**< Flag indicating if the decay analysis history has been primed with initial values or not */

public:
	MM_TgcDynamicCollectionSetData();

	static MM_TgcDynamicCollectionSetData *newInstance(J9JavaVM *javaVM);
	void kill(J9JavaVM *javaVM);

	void dumpDynamicCollectionSetStatistics(MM_EnvironmentVLHGC *env);

protected:
private:
	bool initialize(J9JavaVM *javaVM);
	void tearDown(J9JavaVM *javaVM);

	void decayPrintValue(MM_EnvironmentVLHGC *env, UDATA value);
	void decayPrintDelta(MM_EnvironmentVLHGC *env, UDATA before, UDATA after);
};

MM_TgcDynamicCollectionSetData::MM_TgcDynamicCollectionSetData()
	: _historyPrimed(false)
{
	_typeId = __FUNCTION__;
	memset(_historyTable, 0, sizeof(_historyTable));
}

MM_TgcDynamicCollectionSetData *
MM_TgcDynamicCollectionSetData::newInstance(J9JavaVM *javaVM)
{
	MM_TgcDynamicCollectionSetData *dcsData = NULL;

	dcsData = (MM_TgcDynamicCollectionSetData *)MM_GCExtensions::getExtensions(javaVM)->getForge()->allocate(sizeof(MM_TgcDynamicCollectionSetData), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != dcsData) {
		new(dcsData) MM_TgcDynamicCollectionSetData();
		if (!dcsData->initialize(javaVM)) {
			dcsData->kill(javaVM);
			dcsData = NULL;
		}
	}

	return dcsData;

}

void
MM_TgcDynamicCollectionSetData::kill(J9JavaVM *javaVM)
{
	tearDown(javaVM);
	MM_GCExtensions::getExtensions(javaVM)->getForge()->free(this);
}

bool
MM_TgcDynamicCollectionSetData::initialize(J9JavaVM *javaVM)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	for(UDATA index = 0; index < MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH; index++) {
		_historyTable[index] = (MM_TgcDynamicCollectionSetHistoryElement *)j9mem_allocate_memory(sizeof(MM_TgcDynamicCollectionSetHistoryElement) * (extensions->tarokRegionMaxAge + 1), OMRMEM_CATEGORY_MM);
		if(NULL == _historyTable[index]) {
			goto error_no_memory;
		}
		memset(_historyTable[index], 0, sizeof(MM_TgcDynamicCollectionSetHistoryElement) * (extensions->tarokRegionMaxAge + 1));
	}

	return true;

error_no_memory:
	return false;
}


void
MM_TgcDynamicCollectionSetData::tearDown(J9JavaVM *javaVM)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	for(UDATA index = 0; index < MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH; index++) {
		if(NULL != _historyTable[index]) {
			j9mem_free_memory(_historyTable[index]);
			_historyTable[index] = NULL;
		}
	}
}

void
MM_TgcDynamicCollectionSetData::decayPrintValue(MM_EnvironmentVLHGC *env, UDATA value)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));

	char unitTable[] = { 'b', 'k', 'm', 'g', 't' };
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

void
MM_TgcDynamicCollectionSetData::decayPrintDelta(MM_EnvironmentVLHGC *env, UDATA before, UDATA after)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));

	if(before >= after) {
		if(0 == before) {
			tgcExtensions->printf("  %3zu", 0);
		} else {
			UDATA percentage = ((before - after) * 100) / before;
			tgcExtensions->printf("  %3zu", percentage);
		}
	} else {
		UDATA percentage = ((after - before) * 100) / after;
		tgcExtensions->printf("(%3zu)", percentage);
	}
}

void
MM_TgcDynamicCollectionSetData::dumpDynamicCollectionSetStatistics(MM_EnvironmentVLHGC *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	MM_CollectionSetDelegate::SetSelectionData *setSelectionDataTable = (MM_CollectionSetDelegate::SetSelectionData *)extensions->tarokTgcSetSelectionDataTable;

	/* Right now we'll just gather results and dump them out */

	/* Rotate the table depth such that "0" is the most recent */
	MM_TgcDynamicCollectionSetHistoryElement *currentHistoryTable = _historyTable[MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH - 1];
	for(UDATA index = MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH - 1; index > 0; index--) {
		_historyTable[index] = _historyTable[index - 1];
	}
	_historyTable[0] = currentHistoryTable;

	/* Clear the current table of existing data */
	memset(currentHistoryTable, 0, sizeof(MM_TgcDynamicCollectionSetHistoryElement) * (extensions->tarokRegionMaxAge + 1));

	/* Walk all regions building up statistics */
	GC_HeapRegionIteratorVLHGC regionIterator(extensions->heapRegionManager, MM_HeapRegionDescriptor::MANAGED);
	MM_HeapRegionDescriptorVLHGC *region = NULL;
	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->containsObjects()) {
			UDATA ageIndex = region->getLogicalAge();

			Assert_MM_true(region->getLogicalAge() <= extensions->tarokRegionMaxAge);
			currentHistoryTable[ageIndex].regionCount += 1;
		}
	}

	/* Calculate history */
	double weight = 0.1;
	if(!_historyPrimed) {
		_historyPrimed = true;
		weight = 1.0;
	}
	double preserve = 1.0 - weight;

	for(UDATA ageIndex = 0; ageIndex <= extensions->tarokRegionMaxAge; ageIndex++) {
		currentHistoryTable[ageIndex].avgHis_regionCount = _historyTable[1][ageIndex].avgHis_regionCount * preserve;
		currentHistoryTable[ageIndex].avgHis_regionCount += ((double)currentHistoryTable[ageIndex].regionCount) * weight;

		currentHistoryTable[ageIndex].avgLcl_regionCount = _historyTable[1][ageIndex].avgLcl_regionCount / (double)2.0;
		currentHistoryTable[ageIndex].avgLcl_regionCount += ((double)currentHistoryTable[ageIndex].regionCount) / (double)2.0;
	}

	/*
	 * Dump statistics to the TTY.
	 */
	tgcExtensions->printf("\n        ");
	for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
		tgcExtensions->printf(" %5zu", tableIndex);
	}

	tgcExtensions->printf("\n       ");
	for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
		tgcExtensions->printf("------");
	}

	/* Region count */
	tgcExtensions->printf("\nRegCnt  ");
	for(UDATA tableIndex = 0; tableIndex <= extensions->tarokRegionMaxAge; tableIndex++) {
		tgcExtensions->printf(" %5zu", currentHistoryTable[tableIndex].regionCount);
	}

	tgcExtensions->printf("\n AvgHis ");
	for(UDATA ageIndex = 0; ageIndex <= extensions->tarokRegionMaxAge; ageIndex++) {
		tgcExtensions->printf(" %5zu", (UDATA)currentHistoryTable[ageIndex].avgHis_regionCount);
	}

	tgcExtensions->printf("\n AvgH%2zu ", MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH);
	for(UDATA ageIndex = 0; ageIndex <= extensions->tarokRegionMaxAge; ageIndex++) {
		tgcExtensions->printf(" %5zu", (UDATA)currentHistoryTable[ageIndex].avgLcl_regionCount);
	}

	UDATA compactGroupCount = MM_CompactGroupManager::getCompactGroupMaxCount(env);
	tgcExtensions->printf("\nROR     ");
	MM_CompactGroupPersistentStats *persistentStats = extensions->compactGroupPersistentStats;
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		double rateOfReturn = 1.0 - persistentStats[compactGroupIndex]._historicalSurvivalRate;
		tgcExtensions->printf(" %5zu", (UDATA)(rateOfReturn * (double)1000.0));
	}

	tgcExtensions->printf("\n RgCtB  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountBefore);
	}
	tgcExtensions->printf("\n  RgLfB ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountArrayletLeafBefore);
	}
	tgcExtensions->printf("\n RgCtA  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountAfter);
	}
	tgcExtensions->printf("\n  RgLfA ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountArrayletLeafAfter);
	}

	tgcExtensions->printf("\n RgOv   ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountOverflow);
	}
	tgcExtensions->printf("\n  RgLfOv");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._regionCountArrayletLeafOverflow);
	}

	tgcExtensions->printf("\n RcRgB  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableRegionCountBefore);
	}
	tgcExtensions->printf("\n  RcLfB ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableRegionCountArrayletLeafBefore);
	}
	tgcExtensions->printf("\n RcRgA  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableRegionCountAfter);
	}
	tgcExtensions->printf("\n  RcLfA ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" %5zu", setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableRegionCountArrayletLeafAfter);
	}

	tgcExtensions->printf("\n RcBcB  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" ");
		decayPrintValue(env, setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableBytesConsumedBefore);
	}
	tgcExtensions->printf("\n RcBcA  ");
	for(UDATA compactGroupIndex = 0; compactGroupIndex < compactGroupCount; compactGroupIndex++) {
		tgcExtensions->printf(" ");
		decayPrintValue(env, setSelectionDataTable[compactGroupIndex]._reclaimStats._reclaimableBytesConsumedAfter);
	}

	tgcExtensions->printf("\n");
}

/**
 * Report dynamic collection set statistics for PGC.
 */
static void
tgcHookReportDynamicCollectionSetStatistics(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_VlhgcGarbageCollectCompletedEvent* event = (MM_VlhgcGarbageCollectCompletedEvent*)eventData;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(event->currentThread);

	if(MM_GCExtensions::getExtensions(env)->tarokEnableDynamicCollectionSetSelection) {
		MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));
		MM_TgcDynamicCollectionSetData *dcsData = (MM_TgcDynamicCollectionSetData *)tgcExtensions->_dynamicCollectionSetData;
		dcsData->dumpDynamicCollectionSetStatistics(env);
	}
}

/**
 * Dump a legend of terms to the tty.
 */
void
dumpLegend(J9JavaVM *javaVM)
{
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(javaVM);

	tgcExtensions->printf("\nTGC Dynamic Collection Set Glossary");
	tgcExtensions->printf("\n-----------------------------------");

	/* Region count */
	tgcExtensions->printf("\nRegCnt   - Number of regions found in the age group");
	tgcExtensions->printf("\n AvgHis  - Historical average number of regions found in the age group");
	tgcExtensions->printf("\n AvgHxx  - Half-life average number of regions found in the age group for the last %zu iterations", (UDATA)MM_TGC_DYNAMIC_COLLECTION_SET_HISTORY_TABLE_DEPTH);

	tgcExtensions->printf("\nROR      - Rate of return when collecting (trace) the region set (amount of currently consumed data that is rendered reclaimable)");
	tgcExtensions->printf("\n RgCtB   - Region count before collection");
	tgcExtensions->printf("\n  RgLfB  - Leaf region count before collection");
	tgcExtensions->printf("\n RgCtA   - Region count after collection");
	tgcExtensions->printf("\n  RgLfA  - Leaf region count after collection");

	tgcExtensions->printf("\n RgOv    - Regions in an overflow state (uncollectable through normal means)");
	tgcExtensions->printf("\n  RgLfOv - Leaf regions in an overflow state as a consequence of their parents being overflowed");

	tgcExtensions->printf("\n RcRgB   - Reclaimable region count before collection (those marked as part of the collection set)");
	tgcExtensions->printf("\n  RcLfB  - Reclaimable leaf region count before collection");
	tgcExtensions->printf("\n RcRgA   - Reclaimable region count after collection");
	tgcExtensions->printf("\n  RcLfA  - Reclaimable leaf region count after collection");

	tgcExtensions->printf("\n RcBcB   - Reclaimable region bytes consumed before collection");
	tgcExtensions->printf("\n RcBcA   - Reclaimable region bytes consumed after collection");

	tgcExtensions->printf("\n");
}

/**
 * Initialize dynamic collection set tgc tracing.
 * Attaches hooks to the appropriate functions handling events used by dynamic collection set tgc tracing.
 */
bool
tgcDynamicCollectionSetInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	bool result = true;

	tgcExtensions->_dynamicCollectionSetData = MM_TgcDynamicCollectionSetData::newInstance(javaVM);
	if(NULL != tgcExtensions->_dynamicCollectionSetData) {
		J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
		(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_VLHGC_GARBAGE_COLLECT_COMPLETED, tgcHookReportDynamicCollectionSetStatistics, OMR_GET_CALLSITE(), NULL);
		dumpLegend(javaVM);
	} else {
		result = false;
	}
	return result;
}

/**
 * Tear down dynamic collection set tgc tracing.
 * Cleans up any support structures related to the specific tgc facility.
 */
void
tgcDynamicCollectionSetTearDown(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	if(NULL != tgcExtensions->_dynamicCollectionSetData) {
		((MM_TgcDynamicCollectionSetData *)tgcExtensions->_dynamicCollectionSetData)->kill(javaVM);
		tgcExtensions->_dynamicCollectionSetData = NULL;
	}
}
