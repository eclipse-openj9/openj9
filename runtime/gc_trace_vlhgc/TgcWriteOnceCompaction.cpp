
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
#include "mmhook.h"
#include "gcutils.h"
#include "j9port.h"
#include "modronopt.h"
#include "j9modron.h"

#include <math.h>

#if defined(J9VM_GC_MODRON_COMPACTION)

#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "CompactVLHGCStats.hpp"
#include "TgcExtensions.hpp"
#include "VMThreadListIterator.hpp"

/**
 * Function called by a hook when the compaction is done.
 * 
 * @param env The environment of the background thread
 */
static void
tgcHookCompactEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_CompactEndEvent* event = (MM_CompactEndEvent*)eventData;
	OMR_VMThread *omrVMThread = event->omrVMThread;
	J9VMThread *vmThread = (J9VMThread *)MM_EnvironmentBase::getEnvironment(omrVMThread)->getLanguageVMThread();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);
	J9VMThread *walkThread;
	CompactReason reason = extensions->globalGCStats.compactStats._compactReason;
	UDATA gcCount = extensions->globalGCStats.gcCount;

	PORT_ACCESS_FROM_VMC(vmThread);

	tgcExtensions->printf("Compact(%zu): reason = %zu (%s)\n", gcCount, reason, getCompactionReasonAsString(reason));

	UDATA movedObjectsTotal = 0;
	UDATA movedObjectsMin = UDATA_MAX;
	UDATA movedObjectsMax = 0;

	UDATA movedBytesTotal = 0;
	UDATA movedBytesMin = UDATA_MAX;
	UDATA movedBytesMax = 0;

	UDATA fixupTotal = 0;
	UDATA fixupMin = UDATA_MAX;
	UDATA fixupMax = 0;
	
	UDATA threadCount = 0;
	
	/* walk the threads first to determine some of the stats -- we need these for the standard deviation */
	GC_VMThreadListIterator compactionThreadListIterator(vmThread);
	while ((walkThread = compactionThreadListIterator.nextVMThread()) != NULL) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(walkThread);
		if ((walkThread == vmThread) || (env->getThreadType() == GC_SLAVE_THREAD)) {
			MM_CompactVLHGCStats *stats = &env->_compactVLHGCStats;

			movedObjectsTotal += stats->_movedObjects;
			movedObjectsMin = OMR_MIN(stats->_movedObjects, movedObjectsMin);
			movedObjectsMax = OMR_MAX(stats->_movedObjects, movedObjectsMax);
			
			movedBytesTotal += stats->_movedBytes;
			movedBytesMin = OMR_MIN(stats->_movedBytes, movedBytesMin);
			movedBytesMax = OMR_MAX(stats->_movedBytes, movedBytesMax);

			fixupTotal += stats->_fixupObjects;
			fixupMin = OMR_MIN(stats->_fixupObjects, fixupMin);
			fixupMax = OMR_MAX(stats->_fixupObjects, fixupMax);
			
			threadCount += 1;
		}
	}
	
	double movedObjectsMean = (double)movedObjectsTotal / (double)threadCount;
	double movedBytesMean = (double)movedBytesTotal / (double)threadCount;
	double fixupMean = (double)fixupTotal / (double)threadCount;
	
	double movedObjectsSquareSum = 0.0;
	double movedBytesSquareSum = 0.0;
	double fixupSquareSum = 0.0;
	
	compactionThreadListIterator = GC_VMThreadListIterator(vmThread);
	while ((walkThread = compactionThreadListIterator.nextVMThread()) != NULL) {
		/* TODO: Are we guaranteed to get the threads in the right order? */
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(walkThread);
		if ((walkThread == vmThread) || (env->getThreadType() == GC_SLAVE_THREAD)) {
			MM_CompactVLHGCStats *stats = &env->_compactVLHGCStats;
			UDATA slaveID = env->getSlaveID();

			tgcExtensions->printf("Compact(%zu): Thread %zu, setup stage: %llu ms.\n",
				gcCount, slaveID, j9time_hires_delta(stats->_setupStartTime, stats->_setupEndTime, J9PORT_TIME_DELTA_IN_MILLISECONDS));
			tgcExtensions->printf("Compact(%zu): Thread %zu, move stage: handled %zu objects in %llu ms, bytes moved %zu.\n",
				gcCount, slaveID, stats->_movedObjects,
				j9time_hires_delta(stats->_moveStartTime, stats->_moveEndTime, J9PORT_TIME_DELTA_IN_MILLISECONDS), stats->_movedBytes);
			tgcExtensions->printf("Compact(%zu): Thread %zu, fixup stage: handled %zu objects in %zu ms, root fixup time %zu ms.\n",
				gcCount, slaveID, stats->_fixupObjects,
				j9time_hires_delta(stats->_fixupStartTime, stats->_fixupEndTime, J9PORT_TIME_DELTA_IN_MILLISECONDS),
				j9time_hires_delta(stats->_rootFixupStartTime, stats->_rootFixupEndTime, J9PORT_TIME_DELTA_IN_MILLISECONDS));

			double movedObjectsDelta = (double)stats->_movedObjects - movedObjectsMean;
			movedObjectsSquareSum += movedObjectsDelta * movedObjectsDelta;
			double movedBytesDelta = (double)stats->_movedBytes - movedBytesMean;
			movedBytesSquareSum += movedBytesDelta * movedBytesDelta;
			double fixupDelta = (double)stats->_fixupObjects - fixupMean;
			fixupSquareSum += fixupDelta * fixupDelta;
		}
	}
	
	tgcExtensions->printf("Compact(%zu): Summary, move stage: handled %zu (min=%zu, max=%zu, stddev=%.2f) objects, bytes moved %zu (min=%zu, max=%zu, stddev=%.2f).\n",
		gcCount,
		movedObjectsTotal, movedObjectsMin, movedObjectsMax, sqrt(movedObjectsSquareSum / threadCount),
		movedBytesTotal, movedBytesMin, movedBytesMax, sqrt(movedBytesSquareSum / threadCount));
	tgcExtensions->printf("Compact(%zu): Summary, fixup stage: handled %zu (min=%zu, max=%zu, stddev=%.2f) objects.\n",
		gcCount,
		fixupTotal, fixupMin, fixupMax, sqrt(fixupSquareSum / threadCount));

}


/**
 * Initialize compaction tgc tracing.
 * Initializes the TgcCompactionExtensions object associated with concurrent tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by concurrent tgc tracing.
 */
bool
tgcWriteOnceCompactionInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_COMPACT_END, tgcHookCompactEnd, OMR_GET_CALLSITE(), NULL);

	return result;
}

#endif /* J9VM_GC_MODRON_COMPACTION */
 
