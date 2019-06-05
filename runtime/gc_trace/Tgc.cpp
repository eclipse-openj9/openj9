
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

#include <string.h>

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

#include "j9consts.h"
#include "j9port.h"
#include "j9protos.h"
#include "modronopt.h"
#include "TgcAllocation.hpp"
#include "TgcAllocationContext.hpp"
#include "TgcBacktrace.hpp"
#if defined(J9VM_GC_HEAP_CARD_TABLE)
#include "TgcCardCleaning.hpp"
#endif /* defined(J9VM_GC_HEAP_CARD_TABLE) */
#if defined(J9VM_GC_MODRON_STANDARD) && defined(J9VM_GC_MODRON_COMPACTION)
#include "TgcCompaction.hpp"
#endif /* J9VM_GC_MODRON_STANDARD && J9VM_GC_MODRON_COMPACTION */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "TgcConcurrent.hpp"
#include "TgcConcurrentcardcleaning.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#include "TgcDump.hpp"
#include "TgcExclusiveaccess.hpp"
#include "TgcFreelist.hpp"
#include "TgcExcessivegc.hpp"
#include "TgcHeap.hpp"
#if defined(J9VM_GC_MODRON_STANDARD)
#include "TgcFreeListSummary.hpp"
#include "TgcLargeAllocation.hpp"
#endif /* defined(J9VM_GC_MODRON_STANDARD) */
#if defined(J9VM_GC_VLHGC)
#include "TgcIntelligentCompact.hpp"
#include "TgcInterRegionReferences.hpp"
#include "TgcInterRegionRememberedSet.hpp"
#include "TgcInterRegionRememberedSetDemographics.hpp"
#include "TgcCopyForward.hpp"
#include "TgcDynamicCollectionSet.hpp"
#include "TgcProjectedStats.hpp"
#include "TgcWriteOnceCompaction.hpp"
#include "TgcWriteOnceCompactTiming.hpp"
#endif /* defined(J9VM_GC_VLHGC) */
#include "TgcParallel.hpp"
#include "TgcRootScanner.hpp"
#if defined(J9VM_GC_MODRON_SCAVENGER)
#include "TgcScavenger.hpp"
#endif /* J9VM_GC_MODRON_SCAVENGER */
#include "TgcTerse.hpp"

bool
tgcInstantiateExtensions(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	
	if(NULL == tgcExtensions) {
		tgcExtensions = MM_TgcExtensions::newInstance(extensions);
		if (NULL == tgcExtensions) {
			return false;
		}
		extensions->tgcExtensions = tgcExtensions;
	}
	return true;
}

/**
 * Consume arguments found in the -Xtgc: (tgc_colon) argument list.
 * @return true if parsing was successful, 0 otherwise.
 */
bool
tgcParseArgs(J9JavaVM *javaVM, char *optArg)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	/* TODO: how should we handle initialization error? */
	char *scan_start = optArg;
	char *scan_limit = optArg + strlen(optArg);
	char *error_scan;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	
	if(!tgcInstantiateExtensions(javaVM)) {
		return false;
	}

	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	while (scan_start < scan_limit) {
		/* ignore separators */
		try_scan(&scan_start, ",");

		error_scan = scan_start;

		if (try_scan(&scan_start, "file=")) {
			char *filename = scan_to_delim(PORTLIB, &scan_start, ',');
			if (NULL != filename) {
				tgcExtensions->setOutputFile(filename);
				j9mem_free_memory(filename);
				continue;
			}
		}
		
		if (try_scan(&scan_start, "backtrace")) {
			tgcExtensions->_backtraceRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "compaction")) {
			tgcExtensions->_compactionRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "concurrent")) {
			tgcExtensions->_concurrentRequested = true;
			continue;
		}
			
		if (try_scan(&scan_start, "cardcleaning")) {
			tgcExtensions->_cardCleaningRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "dump")) {
			tgcExtensions->_dumpRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "exclusiveaccess")) {
			tgcExtensions->_exclusiveAccessRequested = true;
			continue;
		}
		
		if (try_scan(&scan_start, "excessivegc")) {
			tgcExtensions->_excessiveGCRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "freeListSummary")) {
			tgcExtensions->_freeListSummaryRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "freeList")) {
			tgcExtensions->_freeListRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "heap")) {
			tgcExtensions->_heapRequested = true;
			continue;
		}
	
		if (try_scan(&scan_start, "parallel")) {
			tgcExtensions->_parallelRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "rootscantime")) {
			tgcExtensions->_rootScannerRequested = true;
			continue;
		}
		
		if (try_scan(&scan_start, "rememberedSetCardList")) {
			tgcExtensions->_interRegionRememberedSetRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "rememberedSetDemographics")) {
			tgcExtensions->_interRegionRememberedSetDemographicsRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "numa")) {
			tgcExtensions->_numaRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "allocationContext")) {
			tgcExtensions->_allocationContextRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "intelligentCompact")) {
			tgcExtensions->_intelligentCompactRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "dynamicCollectionSet")) {
			tgcExtensions->_dynamicCollectionSetRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "projectedStats")) {
			tgcExtensions->_projectedStatsRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "writeOnceCompactTiming")) {
			tgcExtensions->_writeOnceCompactTimingRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "copyForward")) {
			tgcExtensions->_copyForwardRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "interRegionReferences")) {
			tgcExtensions->_interRegionReferencesRequested = true;
			continue;
		}
		
		if (try_scan(&scan_start, "scavengerSurvivalStats")) {
			tgcExtensions->_scavengerSurvivalStatsRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "scavengerMemoryStats")) {
			tgcExtensions->_scavengerMemoryStatsRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "scavenger")) {
			tgcExtensions->_scavengerRequested = true;
			tgcExtensions->_scavengerSurvivalStatsRequested = true;
			tgcExtensions->_scavengerMemoryStatsRequested = true;
			continue;
		}
		
		if (try_scan(&scan_start, "terse")) {
			tgcExtensions->_terseRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "allocation")) {
			tgcExtensions->_allocationRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "largeAllocationVerbose")) {
			tgcExtensions->_largeAllocationVerboseRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "largeAllocation")) {
			tgcExtensions->_largeAllocationRequested = true;
			continue;
		}

		/* Couldn't find a match for arguments */
		scan_failed(PORTLIB, "GC", error_scan);
		return false;
	}

	return true;
}

bool
tgcInitializeRequestedOptions(J9JavaVM *javaVM)
{
	bool result = true;

#if (defined(J9VM_GC_MODRON_STANDARD) || defined(J9VM_GC_VLHGC) || defined(J9VM_GC_SEGREGATED_HEAP))

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	/* StandardGC, VLHGC , and Segregated heap options*/
	if (extensions->isStandardGC() || extensions->isVLHGC() || extensions->isSegregatedHeap()) {
		if (tgcExtensions->_heapRequested) {
			result = result && tgcHeapInitialize(javaVM);
		}

		if (tgcExtensions->_rootScannerRequested) {
			result = result && tgcRootScannerInitialize(javaVM);
		}
	}

	/* StandardGC and VLHGC both options*/
	if (extensions->isStandardGC() || extensions->isVLHGC()) {
		if (tgcExtensions->_backtraceRequested) {
			result = result && tgcBacktraceInitialize(javaVM);
		}

		if (tgcExtensions->_dumpRequested) {
			result = result && tgcDumpInitialize(javaVM);
		}

		if (tgcExtensions->_exclusiveAccessRequested) {
			result = result && tgcExclusiveAccessInitialize(javaVM);
		}

		if (tgcExtensions->_excessiveGCRequested) {
			result = result && tgcExcessiveGCInitialize(javaVM);
		}

		if (tgcExtensions->_freeListRequested) {
			result = result && tgcFreeListInitialize(javaVM);
		}

		if (tgcExtensions->_parallelRequested) {
			result = result && tgcParallelInitialize(javaVM);
		}

		if (tgcExtensions->_terseRequested) {
			result = result && tgcTerseInitialize(javaVM);
		}

		if (tgcExtensions->_allocationRequested) {
			result = result && tgcAllocationInitialize(javaVM);
		}
		
		if (tgcExtensions->_largeAllocationVerboseRequested || tgcExtensions->_largeAllocationRequested) {
			result = result && tgcLargeAllocationInitialize(javaVM);
		}

		if (tgcExtensions->_numaRequested) { 
			result = result && tgcNumaInitialize(javaVM);
		}		
	}

	/* StandardGC only options */
	if (extensions->isStandardGC()) {
#if defined(J9VM_GC_MODRON_STANDARD)
#if defined(J9VM_GC_MODRON_COMPACTION)
		if (tgcExtensions->_compactionRequested) {
			result = result && tgcCompactionInitialize(javaVM);
		}
#endif /* J9VM_GC_MODRON_COMPACTION */
		if (tgcExtensions->_concurrentRequested) {
			result = result && tgcConcurrentInitialize(javaVM);
		}

		if (tgcExtensions->_cardCleaningRequested) {
			result = result && tgcConcurrentCardCleaningInitialize(javaVM);
		}

		if (tgcExtensions->_freeListSummaryRequested) {
			result = result && tgcFreeListSummaryInitialize(javaVM);
		}

		if (tgcExtensions->_scavengerSurvivalStatsRequested) {
			result = result && tgcScavengerSurvivalStatsInitialize(javaVM);
		}
		
		if (tgcExtensions->_scavengerMemoryStatsRequested) {
			result = result && tgcScavengerMemoryStatsInitialize(javaVM);
		}

		if (tgcExtensions->_scavengerRequested) {
			result = result && tgcScavengerInitialize(javaVM);
		}
#endif /* defined(J9VM_GC_MODRON_STANDARD) */
	}

	/* VLHGC only options */
	if (extensions->isVLHGC()) {
#if defined(J9VM_GC_VLHGC)
		if (tgcExtensions->_compactionRequested) {
			result = result && tgcWriteOnceCompactionInitialize(javaVM);
		}

		if (tgcExtensions->_cardCleaningRequested) {
			result = result && tgcCardCleaningInitialize(javaVM);
		}

		if (tgcExtensions->_interRegionRememberedSetRequested) {
			result = result && tgcInterRegionRememberedSetInitialize(javaVM);
		}

		if (tgcExtensions->_interRegionRememberedSetDemographicsRequested) {
			result = result && tgcInterRegionRememberedSetDemographicsInitialize(javaVM);
		}

		if (tgcExtensions->_allocationContextRequested) {
			result = result && tgcAllocationContextInitialize(javaVM);
		}
#if defined(J9VM_GC_MODRON_COMPACTION)
		if (tgcExtensions->_intelligentCompactRequested) {
			result = result && tgcIntelligentCompactInitialize(javaVM);
		}
#endif /* J9VM_GC_MODRON_COMPACTION */
		if (tgcExtensions->_dynamicCollectionSetRequested) {
			result = result && tgcDynamicCollectionSetInitialize(javaVM);
		}

		if (tgcExtensions->_projectedStatsRequested) {
			result = result && tgcProjectedStatsInitialize(javaVM);
		}
#if defined(J9VM_GC_MODRON_COMPACTION)
		if (tgcExtensions->_writeOnceCompactTimingRequested) {
			result = result && tgcWriteOnceCompactTimingInitialize(javaVM);
		}
#endif /* J9VM_GC_MODRON_COMPACTION */
		if (tgcExtensions->_copyForwardRequested) {
			result = result && tgcCopyForwardInitialize(javaVM);
		}

		if (tgcExtensions->_interRegionReferencesRequested) {
			result = result && tgcInterRegionReferencesInitialize(javaVM);
		}

#endif /* J9VM_GC_VLHGC */
	}


#endif /* defined(J9VM_GC_MODRON_STANDARD) || defined(J9VM_GC_VLHGC)  || defined(J9VM_GC_SEGREGATED_HEAP) */

	return result;
}

/**
 * @todo Provide function documentation
 */
void
tgcTearDownExtensions(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	if (NULL != extensions->tgcExtensions) {
		if (extensions->isVLHGC()) {
#if defined(J9VM_GC_VLHGC)
			tgcInterRegionRememberedSetTearDown(javaVM);
			tgcInterRegionRememberedSetDemographicsTearDown(javaVM);
			tgcDynamicCollectionSetTearDown(javaVM);
			tgcInterRegionReferencesTearDown(javaVM);
#endif /* J9VM_GC_VLHGC */
		}
		MM_TgcExtensions::getExtensions(extensions)->kill(extensions);
		extensions->tgcExtensions = NULL;
	}
}

/**
 * @todo Provide function documentation
 */
void
tgcPrintClass(J9JavaVM *javaVM, J9Class* clazz)
{
	J9ROMClass* romClass;
	J9UTF8* utf;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(javaVM);

	/* TODO: In Sov, if the class is char[], the string is printed instead of the class name */
	romClass = clazz->romClass;
	if(romClass->modifiers & J9AccClassArray) {
		J9ArrayClass* arrayClass = (J9ArrayClass*) clazz;
		UDATA arity = arrayClass->arity;
		utf = J9ROMCLASS_CLASSNAME(arrayClass->leafComponentType->romClass);

		tgcExtensions->printf("%.*s", (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf));
		while(arity--) {
			tgcExtensions->printf("[]");
		}
	} else {
		utf = J9ROMCLASS_CLASSNAME(romClass);
		tgcExtensions->printf("%.*s", (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf));
	}
	return;
}
