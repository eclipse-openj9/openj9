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
 
/**
 * @file
 * @ingroup GC_Modron_Startup
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jni.h"
#include "jvminit.h"
#include "j9port.h"
#include "modronnls.h"
#include "gcutils.h"

#include "mmparse.h"

#include "Configuration.hpp"
#include "GCExtensions.hpp"
#if defined(J9VM_GC_REALTIME)
#include "Scheduler.hpp"
#endif /* J9VM_GC_REALTIME */
#include "Wildcard.hpp"

/**
 * Parse the command line looking for private GC options.
 * @params optArg string to be parsed
 */
jint
gcParseXXgcArguments(J9JavaVM *vm, char *optArg)
{
	char *scan_start = optArg;
	char *scan_limit = optArg + strlen(optArg);
	char *error_scan;
	MM_GCExtensions *extensions;
	PORT_ACCESS_FROM_JAVAVM(vm);

	extensions = MM_GCExtensions::getExtensions(vm);

	jint returnValue = JNI_OK;

	while (scan_start < scan_limit) {
		/* ignore separators */
		try_scan(&scan_start, ",");

		error_scan = scan_start;

#if defined(J9VM_GC_REALTIME)
		if (try_scan(&scan_start, "beatsPerMeasure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->beatMicro), "beatsPerMeasure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		
		if (try_scan(&scan_start, "perfTraceSocket=")) {
			continue;
		}
		if (try_scan(&scan_start, "perfTraceLog=")) {
			continue;
		}		
		if (try_scan(&scan_start, "debug=")) {
			if (scan_udata(&scan_start, &(extensions->debug))) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "debugWriteBarrier=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->debugWriteBarrier), "debugWriteBarrier=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tuningFork")) {
			continue;
		}
		if (try_scan(&scan_start, "stw")) {
			MM_Scheduler::initializeForVirtualSTW(extensions);
			/* Stop the world collects should not do any concurrent work */
			extensions->concurrentSweepingEnabled = false;
			extensions->concurrentTracingEnabled = false;
			continue;
		}
		if (try_scan(&scan_start, "headroom=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->headRoom), "headroom=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "minimumFreeEntrySize=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->minimumFreeEntrySize), "minimumFreeEntrySize=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}		
		if (try_scan(&scan_start, "traceCostToCheckYield=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->traceCostToCheckYield), "traceCostToCheckYield=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "sweepCostToCheckYield=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->sweepCostToCheckYield), "sweepCostToCheckYield=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}		
		if (try_scan(&scan_start, "verbose=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->verbose), "verbose=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "trigger=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &extensions->gcInitialTrigger, "trigger=")) {
				returnValue = JNI_EINVAL;
				break;
			}
		
			extensions->gcTrigger = extensions->gcInitialTrigger;
			continue;
		}
		if (try_scan(&scan_start, "timeInterval=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->timeWindowMicro), "timeInterval=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "verboseExtensions")) {
			extensions->verboseExtensions = true;
			continue;
		}
		if(try_scan(&scan_start, "enableNonDeterministicSweep")) {
			extensions->nonDeterministicSweep = true;
			continue;
		}
		if(try_scan(&scan_start, "disableNonDeterministicSweep")) {
			extensions->nonDeterministicSweep = false;
			continue;
		}
		if(try_scan(&scan_start, "fixHeapForWalk")) {
			extensions->fixHeapForWalk = true;
			continue;
		}
		if (try_scan(&scan_start, "overflowCacheCount=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->overflowCacheCount), "overflowCacheCount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "allocationTrackerMaxTotalError=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &extensions->allocationTrackerMaxTotalError, "allocationTrackerMaxTotalError=")) {
				extensions->allocationTrackerFlushThreshold = OMR_MIN(extensions->allocationTrackerMaxThreshold, extensions->allocationTrackerMaxTotalError);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "allocationTrackerMaxThreshold=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &extensions->allocationTrackerMaxThreshold, "allocationTrackerMaxThreshold=")) {
				extensions->allocationTrackerFlushThreshold = OMR_MIN(extensions->allocationTrackerMaxThreshold, extensions->allocationTrackerMaxTotalError);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if(try_scan(&scan_start, "noConcurrentSweep")) {
			extensions->concurrentSweepingEnabled = false;
			continue;
		}
		if(try_scan(&scan_start, "noConcurrentTrace")) {
			extensions->concurrentTracingEnabled = false;
			continue;
		}
		if(try_scan(&scan_start, "concurrentSweep")) {
			extensions->concurrentSweepingEnabled = true;
			continue;
		}
		if(try_scan(&scan_start, "concurrentTrace")) {
			extensions->concurrentTracingEnabled = true;
			continue;
		}

		if (try_scan(&scan_start, "allocationContextCount=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->managedAllocationContextCount), "allocationContextCount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			
			if (extensions->managedAllocationContextCount <= 0) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "allocationContextCount=", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			
			continue;
		}
#endif /* J9VM_GC_REALTIME */

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		/* see if we should be using the hot field optimization for the scavenger (this is the default) */
		if (try_scan(&scan_start, "scvHotAlignment")) {
			extensions->scavengerAlignHotFields = true;
			continue;
		}

		/* see if we should disable the hot field optimization in the scavenger */
		if (try_scan(&scan_start, "scvNoHotAlignment")) {
			extensions->scavengerAlignHotFields = false;
			continue;
		}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (extensions->compressObjectReferences()) {
			/* see if we are to force disable shifting in compressed refs */
			if (try_scan(&scan_start, "noShiftingCompression")) {
				extensions->shouldAllowShiftingCompression = false;
				continue;
			}

			/* see if we are forcing shifting to a specific value */
			if (try_scan(&scan_start, "forcedShiftingCompressionAmount=")) {
				extensions->shouldForceSpecifiedShiftingCompression = true;
				if(!scan_udata_helper(vm, &scan_start, &(extensions->forcedShiftingCompressionAmount), "forcedShiftingCompressionAmount=")) {
					returnValue = JNI_EINVAL;
					break;
				}
	
				if (extensions->forcedShiftingCompressionAmount > LOW_MEMORY_HEAP_CEILING_SHIFT) {
					returnValue = JNI_EINVAL;
					break;
				}
	
				continue;
			}
		}
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */

#if defined (J9VM_GC_VLHGC)
		/* parse the maximum age a region can have to be included in the nursery set, if specified */
		if (try_scan(&scan_start, "tarokNurseryMaxAge=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokNurseryMaxAge._valueSpecified), "tarokNurseryMaxAge=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokNurseryMaxAge._wasSpecified = true;
			continue;
		}

		/* parse the RememberedSet Card List maximum size */
		if (try_scan(&scan_start, "tarokRememberedSetCardListMaxSize=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokRememberedSetCardListMaxSize), "tarokRememberedSetCardListMaxSize=")) {
				returnValue = JNI_EINVAL;
				break;
			}
		
			continue;
		}
		
		if (try_scan(&scan_start, "tarokRememberedSetCardListSize=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokRememberedSetCardListSize), "tarokRememberedSetCardListSize=")) {
				returnValue = JNI_EINVAL;
				break;
			}

			continue;
		}

		/* parse the maximum age a region can be incremented to (increments occurring after a PGC) */
		if (try_scan(&scan_start, "tarokRegionMaxAge=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokRegionMaxAge), "tarokRegionMaxAge=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "tarokKickoffHeadroomRegionRate=")) {
			if(!scan_u32_helper(vm, &scan_start, &(extensions->tarokKickoffHeadroomRegionRate), "tarokKickoffHeadroomRegionRate=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (50 < extensions->tarokKickoffHeadroomRegionRate) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "tarokKickoffHeadroomRegionRate=", (UDATA)0, (UDATA)50);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "tarokKickoffHeadroomInBytes=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokKickoffHeadroomInBytes), "tarokKickoffHeadroomInBytes=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokForceKickoffHeadroomInBytes = true;
			continue;
		}

		/* see if we are being asked to enable our debugging capabilities (free region poisoning, etc)  */
		if (try_scan(&scan_start, "tarokDebugEnabled")) {
			extensions->tarokDebugEnabled = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableScoreBasedAtomicCompact")) {
			extensions->tarokEnableScoreBasedAtomicCompact = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableScoreBasedAtomicCompact")) {
			extensions->tarokEnableScoreBasedAtomicCompact = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableIncrementalGMP")) {
			extensions->tarokEnableIncrementalGMP = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableIncrementalGMP")) {
			extensions->tarokEnableIncrementalGMP = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableExpensiveAssertions")) {
			extensions->tarokEnableExpensiveAssertions = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableExpensiveAssertions")) {
			extensions->tarokEnableExpensiveAssertions = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokTgcEnableRememberedSetDuplicateDetection")) {
			extensions->tarokTgcEnableRememberedSetDuplicateDetection = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokTgcDisableRememberedSetDuplicateDetection")) {
			extensions->tarokTgcEnableRememberedSetDuplicateDetection = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokPGCOnlyCopyForward")) {
			extensions->tarokPGCShouldMarkCompact = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokPGCOnlyMarkCompact")) {
			extensions->tarokPGCShouldCopyForward = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableDynamicCollectionSetSelection")) {
			extensions->tarokEnableDynamicCollectionSetSelection = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableDynamicCollectionSetSelection")) {
			extensions->tarokEnableDynamicCollectionSetSelection = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokDynamicCollectionSetSelectionAbsoluteBudget=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->tarokDynamicCollectionSetSelectionAbsoluteBudget, "tarokDynamicCollectionSetSelectionAbsoluteBudget=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokDynamicCollectionSetSelectionPercentageBudget = 0.0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokDynamicCollectionSetSelectionPercentageBudget=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "tarokDynamicCollectionSetSelectionPercentageBudget=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokDynamicCollectionSetSelectionPercentageBudget = ((double)percentage) / 100.0;
			extensions->tarokDynamicCollectionSetSelectionAbsoluteBudget = 0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokCoreSamplingAbsoluteBudget=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->tarokCoreSamplingAbsoluteBudget, "tarokCoreSamplingAbsoluteBudget=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokCoreSamplingPercentageBudget = 0.0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokCoreSamplingPercentageBudget=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "tarokCoreSamplingPercentageBudget=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokCoreSamplingPercentageBudget = ((double)percentage) / 100.0;
			extensions->tarokCoreSamplingAbsoluteBudget = 0;
			continue ;
		}

		if (try_scan(&scan_start, "tarokGlobalMarkIncrementTimeMillis=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->tarokGlobalMarkIncrementTimeMillis, "tarokGlobalMarkIncrementTimeMillis=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokPGCtoGMP=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->tarokPGCtoGMPNumerator, "tarokPGCtoGMP=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (try_scan(&scan_start, ":")) {
				if(!scan_udata_helper(vm, &scan_start, &extensions->tarokPGCtoGMPDenominator, "tarokPGCtoGMP=")) {
					returnValue = JNI_EINVAL;
					break;
				}
			}
			if ( (1 != extensions->tarokPGCtoGMPNumerator) && (1 != extensions->tarokPGCtoGMPDenominator) ) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTION_MALFORMED, "tarokPGCtoGMP=");
				returnValue = JNI_EINVAL;
				break;
			}
			
			continue;
		}
		if (try_scan(&scan_start, "tarokGMPIntermission=")) {
			if (try_scan(&scan_start, "auto")) {
				extensions->tarokAutomaticGMPIntermission = true;
				extensions->tarokGMPIntermission = UDATA_MAX;
			} else if(scan_udata_helper(vm, &scan_start, &extensions->tarokGMPIntermission, "tarokGMPIntermission=")) {
				extensions->tarokAutomaticGMPIntermission = false;
			} else {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokNoCommonThreads")) {
			extensions->tarokAttachedThreadsAreCommon = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokCopyForwardFragmentationTarget=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "tarokCopyForwardFragmentationTarget=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokCopyForwardFragmentationTarget = ((double)percentage) / 100.0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokDefragmentEmptinessThreshold=")) {
			UDATA percentage = 0;
			if (try_scan(&scan_start, "auto")) {
				extensions->tarokAutomaticDefragmentEmptinessThreshold = true;
				extensions->tarokDefragmentEmptinessThreshold = 0.0;
			} else if (scan_udata_helper(vm, &scan_start, &percentage, "tarokDefragmentEmptinessThreshold=")) {
				if (percentage > 100) {
					j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "-XXgc:tarokDefragmentEmptinessThreshold=", (UDATA)0, (UDATA)100);
					returnValue = JNI_EINVAL;
					break;
				}
				extensions->tarokAutomaticDefragmentEmptinessThreshold = false;
				extensions->tarokDefragmentEmptinessThreshold = ((double)percentage) / 100.0;
			} else {
				returnValue = JNI_EINVAL;
				break;
			}

			continue ;
		}
		if (try_scan(&scan_start, "tarokConcurrentMarkingCostWeight=")) {
			UDATA percentage = 0;
			if (!scan_udata_helper(vm, &scan_start, &percentage, "tarokConcurrentMarkingCostWeight=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokConcurrentMarkingCostWeight = ((double)percentage) / 100.0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokEnableCardScrubbing")) {
			extensions->tarokEnableCardScrubbing = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableCardScrubbing")) {
			extensions->tarokEnableCardScrubbing = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableConcurrentGMP")) {
			extensions->tarokEnableConcurrentGMP = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableConcurrentGMP")) {
			extensions->tarokEnableConcurrentGMP = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableIncrementalClassGC")) {
			extensions->tarokEnableIncrementalClassGC = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableIncrementalClassGC")) {
			extensions->tarokEnableIncrementalClassGC = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableCompressedCardTable")) {
			extensions->tarokEnableCompressedCardTable = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableCompressedCardTable")) {
			extensions->tarokEnableCompressedCardTable = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableLeafFirstCopying")) {
			extensions->tarokEnableLeafFirstCopying = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableLeafFirstCopying")) {
			extensions->tarokEnableLeafFirstCopying = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableStableRegionDetection")) {
			extensions->tarokEnableStableRegionDetection = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableStableRegionDetection")) {
			extensions->tarokEnableStableRegionDetection = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokAllocationAgeEnabled")) {
			extensions->tarokAllocationAgeEnabled = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokAllocationAgeDisabled")) {
			extensions->tarokAllocationAgeEnabled = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokAllocationAgeExponentBase=")) {
			UDATA exponentBase = 0;
			if(!scan_udata_helper(vm, &scan_start, &exponentBase, "tarokAllocationAgeExponentBase=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->tarokAllocationAgeExponentBase = ((double)exponentBase) / 100.0;
			continue ;
		}
		if (try_scan(&scan_start, "tarokAllocationAgeUnit=")) {
			if(!scan_udata_memory_size_helper(vm, &scan_start, &(extensions->tarokAllocationAgeUnit), "tarokAllocationAgeUnit=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokMaximumAgeInBytes=")) {
			if(!scan_u64_memory_size_helper(vm, &scan_start, &(extensions->tarokMaximumAgeInBytes), "tarokMaximumAgeInBytes=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokMaximumNurseryAgeInBytes=")) {
			if(!scan_u64_memory_size_helper(vm, &scan_start, &(extensions->tarokMaximumNurseryAgeInBytes), "tarokMaximumNurseryAgeInBytes=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokEnableProjectedSurvivalCollectionSet")) {
			extensions->tarokUseProjectedSurvivalCollectionSet = true;
			continue;
		}
		if (try_scan(&scan_start, "tarokDisableProjectedSurvivalCollectionSet")) {
			extensions->tarokUseProjectedSurvivalCollectionSet = false;
			continue;
		}
		if (try_scan(&scan_start, "tarokWorkSplittingPeriod=")) {
			if(!scan_hex_helper(vm, &scan_start, &(extensions->tarokWorkSplittingPeriod), "tarokWorkSplittingPeriod=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "tarokMinimumGMPWorkTargetBytes=")) {
			UDATA workTarget = 0;
			if(!scan_udata_memory_size_helper(vm, &scan_start, &workTarget, "tarokMinimumGMPWorkTargetBytes=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(0 == workTarget) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:tarokMinimumGMPWorkTargetBytes", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}

			extensions->tarokMinimumGMPWorkTargetBytes._wasSpecified = true;
			extensions->tarokMinimumGMPWorkTargetBytes._valueSpecified = workTarget;

			continue;
		}
#endif /* defined (J9VM_GC_VLHGC) */

		if(try_scan(&scan_start, "packetListLockSplit=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->packetListSplit, "packetListLockSplit=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(0 == extensions->packetListSplit) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:packetListLockSplit", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->configuration->_packetListSplitForced = true;
			continue;
		}
		
#if defined(J9VM_GC_MODRON_SCAVENGER)
		if(try_scan(&scan_start, "cacheListLockSplit=")) {
			if(!scan_udata_helper(vm, &scan_start, &extensions->cacheListSplit, "cacheListLockSplit=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(0 == extensions->cacheListSplit) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:cacheListLockSplit", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->configuration->_cacheListSplitForced = true;
			continue;
		}
#endif /* J9VM_GC_MODRON_SCAVENGER */

		if (try_scan(&scan_start, "markingArraySplitMinimumAmount=")) {
			UDATA arraySplitAmount = 0;
			if(!scan_udata_helper(vm, &scan_start, &arraySplitAmount, "markingArraySplitMinimumAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->markingArraySplitMinimumAmount = (0 == arraySplitAmount) ? UDATA_MAX : arraySplitAmount;
			continue;
		}
		if (try_scan(&scan_start, "markingArraySplitMaximumAmount=")) {
			UDATA arraySplitAmount = 0;
			if(!scan_udata_helper(vm, &scan_start, &arraySplitAmount, "markingArraySplitMaximumAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->markingArraySplitMaximumAmount = (0 == arraySplitAmount) ? UDATA_MAX : arraySplitAmount;

			if(extensions->markingArraySplitMaximumAmount < extensions->markingArraySplitMinimumAmount) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:markingArraySplitMaximumAmount", (UDATA)extensions->markingArraySplitMinimumAmount);
				returnValue = JNI_EINVAL;
				break;
			}

			continue;
		}

		if (try_scan(&scan_start, "stdSplitFreeListSplitAmount=h")) {
			j9tty_printf(PORTLIB, "stdSplitFreeListSplitAmount=h   %s\n", scan_start);

			if(!scan_udata_helper(vm, &scan_start, &extensions->splitFreeListSplitAmount, "stdSplitFreeListSplitAmount=h")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(1 >= extensions->splitFreeListSplitAmount) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:stdSplitFreeListSplitAmount=h", (UDATA)1);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->enableHybridMemoryPool = true;
			extensions->configuration->_splitFreeListAmountForced = true;
			continue;
		}

		if (try_scan(&scan_start, "stdSplitFreeListSplitAmount=")) {
			j9tty_printf(PORTLIB, "stdSplitFreeListSplitAmount=   %s\n", scan_start);
			if(!scan_udata_helper(vm, &scan_start, &extensions->splitFreeListSplitAmount, "stdSplitFreeListSplitAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(0 == extensions->splitFreeListSplitAmount) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:stdSplitFreeListSplitAmount", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->configuration->_splitFreeListAmountForced = true;
			continue;
		}

/* Start of options relating to dynamicBreadthFirstScanOrdering */
#if defined(J9VM_GC_MODRON_SCAVENGER) || defined (J9VM_GC_VLHGC)	
		if(try_scan(&scan_start, "dbfGcCountBetweenHotFieldSort=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfGcCountBetweenHotFieldSort=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 10) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfGcCountBetweenHotFieldSort=", (UDATA)0, (UDATA)10);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->gcCountBetweenHotFieldSort = value;
			continue;
		}

		if(try_scan(&scan_start, "dbfGcCountBetweenHotFieldSortMax=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfGcCountBetweenHotFieldSortMax=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 50) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfGcCountBetweenHotFieldSortMax=", (UDATA)0, (UDATA)50);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->gcCountBetweenHotFieldSortMax = value;
			continue;
		}

		if(try_scan(&scan_start, "dbfDisableAdaptiveGcCountBetweenHotFieldSort")) {
			extensions->adaptiveGcCountBetweenHotFieldSort = false;
			continue;
		}

		if(try_scan(&scan_start, "dbfDisableDepthCopyTwoPaths")) {
			extensions->depthCopyTwoPaths = false;
			continue;
		}
		
		if(try_scan(&scan_start, "dbfDepthCopyThreePaths")) {
			extensions->depthCopyThreePaths = true;
			continue;
		}
		
		if(try_scan(&scan_start, "dbfEnableAlwaysDepthCopyFirstOffset")) {
			extensions->alwaysDepthCopyFirstOffset = true;
			continue;
		} 

		if(try_scan(&scan_start, "dbfEnablePermanantHotFields")) {
			extensions->allowPermanantHotFields = true;
			continue;
		}
		
		if(try_scan(&scan_start, "dbfMaxConsecutiveHotFieldSelections=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfMaxConsecutiveHotFieldSelections=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 50) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfMaxConsecutiveHotFieldSelections=", (UDATA)0, (UDATA)50);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->maxConsecutiveHotFieldSelections = value;
			continue;
		}

		if(try_scan(&scan_start, "dbfEnableHotFieldResetting")) {
			extensions->hotFieldResettingEnabled = true;
			continue;
		}

		if(try_scan(&scan_start, "dbfGcCountBetweenHotFieldReset=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfGcCountBetweenHotFieldReset=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 5000) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfGcCountBetweenHotFieldReset=", (UDATA)0, (UDATA)5000);
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->gcCountBetweenHotFieldReset = value;
			continue;
		}

		if(try_scan(&scan_start, "dbfDepthCopyMax=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfDepthCopyMax=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 10) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfDepthCopyMax=", (UDATA)0, (UDATA)10);
				returnValue = JNI_EINVAL;
				break;
			}	
			extensions->depthCopyMax = value;
			continue;
		}

		if(try_scan(&scan_start, "dbfMaxHotFieldListLength=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfMaxHotFieldListLength=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 20) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfMaxHotFieldListLength=", (UDATA)0, (UDATA)20);
				returnValue = JNI_EINVAL;
				break;
			}	
			extensions->maxHotFieldListLength = ((uint32_t)value);
			continue;
		}

		if(try_scan(&scan_start, "dbfMinCpuUtil=")) {
			UDATA value;
			if(!scan_udata_helper(vm, &scan_start, &value, "dbfMinCpuUtil=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(value > 15) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "dbfMinCpuUtil=", (UDATA)0, (UDATA)15);
				returnValue = JNI_EINVAL;
				break;
			}	
			extensions->minCpuUtil = value;
			continue;
		}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) || defined (J9VM_GC_VLHGC) */
/* End of options relating to dynamicBreadthFirstScanOrdering */

#if defined(J9VM_ENV_DATA64)
		if (try_scan(&scan_start, "enableIndexableDualHeaderShape")) {
			vm->isIndexableDualHeaderShapeEnabled = TRUE;
			continue;
		}

		if (try_scan(&scan_start, "disableIndexableDualHeaderShape")) {
			vm->isIndexableDualHeaderShapeEnabled = FALSE;
			continue;
		}
#endif /* defined(J9VM_ENV_DATA64) */

#if defined(J9VM_GC_MODRON_SCAVENGER)	
		if (try_scan(&scan_start, "scanCacheMinimumSize=")) {
			/* Read in restricted scan cache size */
			if(!scan_udata_helper(vm, &scan_start, &extensions->scavengerScanCacheMinimumSize, "scanCacheMinimumSize=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(0 == extensions->scavengerScanCacheMinimumSize) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:scanCacheMinimumSize", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "scanCacheMaximumSize=")) {
			/* Read in restricted scan cache size */
			if(!scan_udata_helper(vm, &scan_start, &extensions->scavengerScanCacheMaximumSize, "scanCacheMaximumSize=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(extensions->scavengerScanCacheMinimumSize > extensions->scavengerScanCacheMaximumSize) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:scanCacheMaximumSize", (UDATA)extensions->scavengerScanCacheMinimumSize);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "scvArraySplitAmount=")) {
			UDATA arraySplitAmount = 0;
			if(!scan_udata_helper(vm, &scan_start, &arraySplitAmount, "scvArraySplitAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->scvArraySplitMinimumAmount = (0 == arraySplitAmount) ? UDATA_MAX : arraySplitAmount;
			extensions->scvArraySplitMaximumAmount = extensions->scvArraySplitMinimumAmount;
			continue;
		}
		if (try_scan(&scan_start, "scvArraySplitMinimumAmount=")) {
			UDATA arraySplitAmount = 0;
			if(!scan_udata_helper(vm, &scan_start, &arraySplitAmount, "scvArraySplitMinimumAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->scvArraySplitMinimumAmount = (0 == arraySplitAmount) ? UDATA_MAX : arraySplitAmount;
			continue;
		}
		if (try_scan(&scan_start, "scvArraySplitMaximumAmount=")) {
			UDATA arraySplitAmount = 0;
			if(!scan_udata_helper(vm, &scan_start, &arraySplitAmount, "scvArraySplitMaximumAmount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->scvArraySplitMaximumAmount = (0 == arraySplitAmount) ? UDATA_MAX : arraySplitAmount;

			if(extensions->scvArraySplitMaximumAmount < extensions->scvArraySplitMinimumAmount) {
				j9nls_printf(PORTLIB,J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:scvArraySplitMaximumAmount", (UDATA)extensions->scvArraySplitMinimumAmount);
				returnValue = JNI_EINVAL;
				break;
			}

			continue;
		}
		if (try_scan(&scan_start, "aliasInhibitingThresholdPercentage=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "aliasInhibitingThresholdPercentage=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->aliasInhibitingThresholdPercentage = ((double)percentage) / 100.0;

			continue;
		}

		if (try_scan(&scan_start, "adaptiveThreadingSensitivityFactor=")) {
			UDATA sensitivityFactor = 0;
			if (!scan_udata_helper(vm, &scan_start, &sensitivityFactor, "adaptiveThreadingSensitivityFactor=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->adaptiveThreadingSensitivityFactor = ((float)sensitivityFactor) / 10.0f;

			continue;
		}

		if (try_scan(&scan_start, "adaptiveThreadingWeightActiveThreads=")) {
			UDATA adaptiveThreadingWeightActiveThreads = 0;
			if (!scan_udata_helper(vm, &scan_start, &adaptiveThreadingWeightActiveThreads, "adaptiveThreadingWeightActiveThreads=")) {
				returnValue = JNI_EINVAL;
				break;
			}

			extensions->adaptiveThreadingWeightActiveThreads = ((float)adaptiveThreadingWeightActiveThreads) / 100.0f;

			continue;
		}

		if (try_scan(&scan_start, "adaptiveThreadBooster=")) {
			UDATA adaptiveThreadBooster = 0;
			if (!scan_udata_helper(vm, &scan_start, &adaptiveThreadBooster, "adaptiveThreadBooster=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->adaptiveThreadBooster = ((float)adaptiveThreadBooster) / 100.0f;

			continue;
		}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (try_scan(&scan_start, "debugConcurrentScavengerPageAlignment")) {
			extensions->setDebugConcurrentScavengerPageAlignment(true);
			continue;
		}
		if(try_scan(&scan_start, "softwareRangeCheckReadBarrier")) {
			extensions->softwareRangeCheckReadBarrierForced = true;
			continue;
		}

		if (try_scan(&scan_start, "enableConcurrentScavengeExhaustiveTermination")) {
			extensions->concurrentScavengeExhaustiveTermination = true;
			continue;
		}

		if (try_scan(&scan_start, "disableConcurrentScavengeExhaustiveTermination")) {
			extensions->concurrentScavengeExhaustiveTermination = false;
			continue;
		}
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */

#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

		if (try_scan(&scan_start, "enableFrequentObjectAllocationSampling")) {
			extensions->doFrequentObjectAllocationSampling = true;
			continue;
		}

		if (try_scan(&scan_start, "disableFrequentObjectAllocationSampling")) {
			extensions->doFrequentObjectAllocationSampling = false;
			continue;
		}

		if (try_scan(&scan_start, "frequentObjectAllocationSamplingDepth=")) {
			if (!scan_u32_helper(vm, &scan_start, &extensions->frequentObjectAllocationSamplingDepth, "frequentObjectAllocationSamplingDepth=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (0 == extensions->frequentObjectAllocationSamplingDepth) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_VALUE_MUST_BE_ABOVE, "-XXgc:frequentObjectAllocationSamplingDepth", (UDATA)0);
				returnValue = JNI_EINVAL;
				break;
			}
			
			extensions->doFrequentObjectAllocationSampling = true;
			continue;
		}

		if (try_scan(&scan_start, "frequentObjectAllocationSamplingRate=")) {
			UDATA percentage = 0;
			if (!scan_udata_helper(vm, &scan_start, &percentage, "frequentObjectAllocationSamplingRate=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			
			extensions->frequentObjectAllocationSamplingRate = percentage;
			extensions->doFrequentObjectAllocationSampling = true;
			continue;
		}

#if defined(J9VM_ENV_DATA64)
		if (try_scan(&scan_start, "enableVirtualLargeObjectHeap")) {
			extensions->isVirtualLargeObjectHeapRequested = true;
			continue;
		}

		if (try_scan(&scan_start, "disableVirtualLargeObjectHeap")) {
			extensions->isVirtualLargeObjectHeapRequested = false;
			continue;
		}
#endif /* defined(J9VM_ENV_DATA64) */

		if (try_scan(&scan_start, "largeObjectAllocationProfilingThreshold=")) {
			if (!scan_udata_helper(vm, &scan_start, &extensions->largeObjectAllocationProfilingThreshold, "largeObjectAllocationProfilingThreshold=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "largeObjectAllocationProfilingVeryLargeObjectThreshold=")) {
			if (!scan_udata_helper(vm, &scan_start, &extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold, "largeObjectAllocationProfilingVeryLargeObjectThreshold=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "largeObjectAllocationProfilingTopK=")) {
			if (!scan_u32_helper(vm, &scan_start, &extensions->largeObjectAllocationProfilingTopK, "largeObjectAllocationProfilingTopK=")) {
				returnValue = JNI_EINVAL;
				break;
			}

			if ((extensions->largeObjectAllocationProfilingTopK < 1) || (128 < extensions->largeObjectAllocationProfilingTopK)) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "largeObjectAllocationProfilingTopK=", (UDATA)1, (UDATA)128);
				returnValue = JNI_EINVAL;
				break;
			}

			continue;
		}

		if (try_scan(&scan_start, "largeObjectAllocationProfilingSizeClassRatio=")) {
			if (!scan_u32_helper(vm, &scan_start, &extensions->largeObjectAllocationProfilingSizeClassRatio, "largeObjectAllocationProfilingSizeClassRatio=")) {
				returnValue = JNI_EINVAL;
				break;
			}

			if ((extensions->largeObjectAllocationProfilingSizeClassRatio < 101) || (200 < extensions->largeObjectAllocationProfilingSizeClassRatio)) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "largeObjectAllocationProfilingSizeClassRatio=", (UDATA)101, (UDATA)200);
				returnValue = JNI_EINVAL;
				break;
			}

			continue;
		}

		if (try_scan(&scan_start, "enableEstimateFragmentationGlobalGCOnly")) {
			extensions->estimateFragmentation = GLOBALGC_ESTIMATE_FRAGMENTATION;
			extensions->processLargeAllocateStats = true;
			continue;
		}

		if (try_scan(&scan_start, "enableEstimateFragmentation")) {
			extensions->estimateFragmentation = (GLOBALGC_ESTIMATE_FRAGMENTATION | LOCALGC_ESTIMATE_FRAGMENTATION);
			extensions->processLargeAllocateStats = true;
			continue;
		}

		if (try_scan(&scan_start, "disableEstimateFragmentation")) {
			extensions->estimateFragmentation = NO_ESTIMATE_FRAGMENTATION;
			extensions->darkMatterSampleRate = 0;
			continue;
		}

		if (try_scan(&scan_start, "enableProcessLargeAllocateStats")) {
			extensions->processLargeAllocateStats = true;
			continue;
		}

		if (try_scan(&scan_start, "disableProcessLargeAllocateStats")) {
			extensions->processLargeAllocateStats = false;
			extensions->estimateFragmentation = NO_ESTIMATE_FRAGMENTATION;
			continue;
		}
		if (try_scan(&scan_start, "verboseNewFormat")) {
			extensions->verboseNewFormat = true;
			continue;
		}
		if (try_scan(&scan_start, "verboseOldFormat")) {
			extensions->verboseNewFormat = false;
			continue;
		}

		if (try_scan(&scan_start, "heapSizeStartupHintConservativeFactor=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "heapSizeStartupHintConservativeFactor=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->heapSizeStartupHintConservativeFactor = ((float)percentage) / 100.0f;
			continue ;
		}

		if (try_scan(&scan_start, "heapSizeStartupHintWeightNewValue=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "heapSizeStartupHintWeightNewValue=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->heapSizeStartupHintWeightNewValue = ((float)percentage) / 100.0f;
			continue ;
		}

		if (try_scan(&scan_start, "darkMatterCompactThreshold=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "darkMatterCompactThreshold=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->darkMatterCompactThreshold = ((float)percentage) / 100.0f;
			continue;
		}
		
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
		if (try_scan(&scan_start, "gcOnIdleCompactThreshold=")) {
			UDATA percentage = 0;
			if(!scan_udata_helper(vm, &scan_start, &percentage, "gcOnIdleCompactThreshold=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if(percentage > 100) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->pageFragmentationCompactThreshold = ((float)percentage) / 100.0f;
			continue;
		}
#endif /* defined(OMR_GC_IDLE_HEAP_MANAGER) */

#if defined (J9VM_GC_VLHGC)
		if (try_scan(&scan_start, "fvtest_tarokSimulateNUMA=")) {
			UDATA simulatedNodeCount = 0;
			if(!scan_udata_memory_size_helper(vm, &scan_start, &simulatedNodeCount, "fvtest_tarokSimulateNUMA=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->_numaManager.setSimulatedNodeCountForFVTest(simulatedNodeCount);
			continue;
		}
		if (try_scan(&scan_start, "fvtest_tarokPGCRotateCollectors")) {
			extensions->tarokPGCShouldMarkCompact = true;
			extensions->tarokPGCShouldCopyForward = true;
			continue;
		}
#endif /* defined (J9VM_GC_VLHGC) */

		if (try_scan(&scan_start, "fvtest_disableExplictMainThread")) {
			extensions->fvtest_disableExplictMainThread = true;
			continue;
		}
		
		if (try_scan(&scan_start, "fvtest_holdRandomThreadBeforeHandlingWorkUnitPeriod=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->_holdRandomThreadBeforeHandlingWorkUnitPeriod), "fvtest_holdRandomThreadBeforeHandlingWorkUnitPeriod=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->_holdRandomThreadBeforeHandlingWorkUnit = true;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_holdRandomThreadBeforeHandlingWorkUnit")) {
			extensions->_holdRandomThreadBeforeHandlingWorkUnit = true;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceRandomBackoutsAfterScanPeriod=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->_forceRandomBackoutsAfterScanPeriod), "fvtest_forceRandomBackoutsAfterScanPeriod=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->_forceRandomBackoutsAfterScan = true;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceRandomBackoutsAfterScan")) {
			extensions->_forceRandomBackoutsAfterScan = true;
			continue;
		}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		if (try_scan(&scan_start, "fvtest_concurrentCardTablePreparationDelay=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_concurrentCardTablePreparationDelay), "fvtest_concurrentCardTablePreparationDelay=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceConcurrentTLHMarkMapCommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceConcurrentTLHMarkMapCommitFailure), "fvtest_forceConcurrentTLHMarkMapCommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceConcurrentTLHMarkMapCommitFailure > 0) {
				extensions->fvtest_forceConcurrentTLHMarkMapCommitFailureCounter = extensions->fvtest_forceConcurrentTLHMarkMapCommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceConcurrentTLHMarkMapCommitFailure")) {
			extensions->fvtest_forceConcurrentTLHMarkMapCommitFailure = 1;
			extensions->fvtest_forceConcurrentTLHMarkMapCommitFailureCounter = 0;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceConcurrentTLHMarkMapDecommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailure), "fvtest_forceConcurrentTLHMarkMapDecommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailure > 0) {
				extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailureCounter = extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceConcurrentTLHMarkMapDecommitFailure")) {
			extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailure = 1;
			extensions->fvtest_forceConcurrentTLHMarkMapDecommitFailureCounter = 0;
			continue;
		}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#if defined (J9VM_GC_HEAP_CARD_TABLE)
		if (try_scan(&scan_start, "fvtest_forceCardTableCommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceCardTableCommitFailure), "fvtest_forceCardTableCommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceCardTableCommitFailure > 0) {
				extensions->fvtest_forceCardTableCommitFailureCounter = extensions->fvtest_forceCardTableCommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceCardTableCommitFailure")) {
			extensions->fvtest_forceCardTableCommitFailure = 1;
			extensions->fvtest_forceCardTableCommitFailureCounter = 0;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceCardTableDecommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceCardTableDecommitFailure), "fvtest_forceCardTableDecommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceCardTableDecommitFailure > 0) {
				extensions->fvtest_forceCardTableDecommitFailureCounter = extensions->fvtest_forceCardTableDecommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceCardTableDecommitFailure")) {
			extensions->fvtest_forceCardTableDecommitFailure = 1;
			extensions->fvtest_forceCardTableDecommitFailureCounter = 0;
			continue;
		}
#endif /* J9VM_GC_HEAP_CARD_TABLE */

		if (try_scan(&scan_start, "fvtest_forceSweepChunkArrayCommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceSweepChunkArrayCommitFailure), "fvtest_forceSweepChunkArrayCommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceSweepChunkArrayCommitFailure > 0) {
				extensions->fvtest_forceSweepChunkArrayCommitFailureCounter = extensions->fvtest_forceSweepChunkArrayCommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceSweepChunkArrayCommitFailure")) {
			extensions->fvtest_forceSweepChunkArrayCommitFailure = 1;
			extensions->fvtest_forceSweepChunkArrayCommitFailureCounter = 0;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceMarkMapCommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceMarkMapCommitFailure), "fvtest_forceMarkMapCommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceMarkMapCommitFailure > 0) {
				extensions->fvtest_forceMarkMapCommitFailureCounter = extensions->fvtest_forceMarkMapCommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceMarkMapCommitFailure")) {
				extensions->fvtest_forceMarkMapCommitFailure = 1;
			extensions->fvtest_forceMarkMapCommitFailureCounter = 0;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceMarkMapDecommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceMarkMapDecommitFailure), "fvtest_forceMarkMapDecommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceMarkMapDecommitFailure > 0) {
				extensions->fvtest_forceMarkMapDecommitFailureCounter = extensions->fvtest_forceMarkMapDecommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceMarkMapDecommitFailure")) {
			extensions->fvtest_forceMarkMapDecommitFailure = 1;
			extensions->fvtest_forceMarkMapDecommitFailureCounter = 0;
			continue;
		}

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
		if (try_scan(&scan_start, "fvtest_enableReadBarrierVerification=")) {
			extensions->fvtest_enableReadBarrierVerification = 0;

			char * pattern = scan_to_delim(PORTLIB, &scan_start, ',');

			if (true == ('0' != pattern[4])) {
				extensions->fvtest_enableHeapReadBarrierVerification = 1;
				extensions->fvtest_enableReadBarrierVerification = 1;
			}
			if (true == ('0' !=  pattern[3])) {
				extensions->fvtest_enableClassStaticsReadBarrierVerification = 1;
				extensions->fvtest_enableReadBarrierVerification = 1;
			}
			if (true == ('0' != pattern[2])){
				extensions->fvtest_enableMonitorObjectsReadBarrierVerification = 1;
				extensions->fvtest_enableReadBarrierVerification = 1;
			}
			if (true == ('0' != pattern[1])) {
				extensions->fvtest_enableJNIGlobalWeakReadBarrierVerification = 1;
				extensions->fvtest_enableReadBarrierVerification = 1;
			}
			continue;
		}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

		if (try_scan(&scan_start, "fvtest_forceReferenceChainWalkerMarkMapCommitFailure=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailure), "fvtest_forceReferenceChainWalkerMarkMapCommitFailure=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if (extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailure > 0) {
				extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailureCounter = extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailure - 1;
			}
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceReferenceChainWalkerMarkMapCommitFailure")) {
			extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailure = 1;
			extensions->fvtest_forceReferenceChainWalkerMarkMapCommitFailureCounter = 0;
			continue;
		}

		if (try_scan(&scan_start, "fvtest_forceCopyForwardHybridMarkCompactRatio=")) {
			/* the percentage of the collectionSet regions would like to markCompact instead of copyForward */
			if(!scan_udata_helper(vm, &scan_start, &(extensions->fvtest_forceCopyForwardHybridRatio), "fvtest_forceCopyForwardHybridMarkCompactRatio=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if ((extensions->fvtest_forceCopyForwardHybridRatio < 1) || (100 < extensions->fvtest_forceCopyForwardHybridRatio)) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTIONS_INTEGER_OUT_OF_RANGE, "fvtest_forceCopyForwardHybridMarkCompactRatio=", (UDATA)1, (UDATA)100);
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		/* NOTE: this option must be understood by all collectors since it appears in options.default */
		if (try_scan(&scan_start, "numaCommonThreadClass=")) {
			char * pattern = scan_to_delim(PORTLIB, &scan_start, ',');
			if (NULL == pattern) {
				returnValue = JNI_ENOMEM;
				break;
			} else {
				const char* needle = NULL;
				UDATA needleLength = 0;
				U_32 matchFlag = 0;
				if (0 == parseWildcard(pattern, strlen(pattern), &needle, &needleLength, &matchFlag)) {
					MM_Wildcard *wildcard = MM_Wildcard::newInstance(extensions, matchFlag, needle, needleLength, pattern);
					if (NULL != wildcard) {
						wildcard->_next = extensions->numaCommonThreadClassNamePatterns;
						extensions->numaCommonThreadClassNamePatterns = wildcard;
					} else {
						returnValue = JNI_ENOMEM;
						break;
					}
				} else {
					j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTION_MALFORMED, pattern);
					j9mem_free_memory(pattern);
					returnValue = JNI_EINVAL;
					break;
				}
			}
			continue;
		}
		
		if (try_scan(&scan_start, "stringTableListToTreeThreshold=")) {
			if(!scan_u32_helper(vm, &scan_start, &(extensions->_stringTableListToTreeThreshold), "stringTableListToTreeThreshold=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "objectListFragmentCount=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->objectListFragmentCount), "objectListFragmentCount=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "minimumFreeSizeForSurvivor=")) {
			UDATA size = 0;
			if(!scan_udata_helper(vm, &scan_start, &size, "minimumFreeSizeForSurvivor=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if ((size > MAXIMUM_SURVIVOR_MINIMUM_FREESIZE) || (size < MINIMUM_SURVIVOR_MINIMUM_FREESIZE) || (0 != (size & (MINIMUM_SURVIVOR_MINIMUM_FREESIZE-1)))) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->minimumFreeSizeForSurvivor = size;
			continue;
		}

		if (try_scan(&scan_start, "freeSizeThresholdForSurvivor=")) {
			UDATA size = 0;
			if(!scan_udata_helper(vm, &scan_start, &size, "freeSizeThresholdForSurvivor=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			if ((size > MAXIMUM_SURVIVOR_THRESHOLD) || (size < MINIMUM_SURVIVOR_THRESHOLD) || (0 != (size & (MINIMUM_SURVIVOR_THRESHOLD-1)))) {
				returnValue = JNI_EINVAL;
				break;
			}
			extensions->freeSizeThresholdForSurvivor = size;
			continue;
		}

		if (try_scan(&scan_start, "noRecycleRemainders")) {
			extensions->recycleRemainders = false;
			continue;
		}

		if (try_scan(&scan_start, "recycleRemainders")) {
			extensions->recycleRemainders = true;
			continue;
		}

		if (try_scan(&scan_start, "stringDedupPolicy=")) {
			if (try_scan(&scan_start, "disabled")) {
				extensions->stringDedupPolicy = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_DISABLED;
			} else if (try_scan(&scan_start, "favourLower")) {
				extensions->stringDedupPolicy = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER;
			} else if (try_scan(&scan_start, "favourHigher")) {
				extensions->stringDedupPolicy = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_FAVOUR_HIGHER;
			} else {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}
		if (try_scan(&scan_start, "darkMatterSampleRate=")) {
			if(!scan_udata_helper(vm, &scan_start, &(extensions->darkMatterSampleRate), "darkMatterSampleRate=")) {
				returnValue = JNI_EINVAL;
				break;
			}
			continue;
		}

		if (try_scan(&scan_start, "disableContinuationList")) {
			extensions->continuationListOption = MM_GCExtensions::disable_continuation_list;
			continue;
		}

		if (try_scan(&scan_start, "enableContinuationList")) {
			extensions->continuationListOption = MM_GCExtensions::enable_continuation_list;
			continue;
		}

		if (try_scan(&scan_start, "verifyContinuationList")) {
			extensions->continuationListOption = MM_GCExtensions::verify_continuation_list;
			continue;
		}

		if (try_scan(&scan_start, "AddContinuationInListOnStarted")) {
			extensions->timingAddContinuationInList = MM_GCExtensions::onStarted;
			continue;
		}

		if (try_scan(&scan_start, "AddContinuationInListOnCreated")) {
			extensions->timingAddContinuationInList = MM_GCExtensions::onCreated;
			continue;
		}
		/* Couldn't find a match for arguments */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_OPTION_UNKNOWN, error_scan);
		returnValue = JNI_EINVAL;
		break;

	} /* end loop */

	return returnValue;

}

