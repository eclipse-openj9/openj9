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

#include "j9modron.h"
#include "modronbase.h"
#include "omrgcconsts.h"
#include "mmhook.h"
#include "gcutils.h"

#include "CollectionStatisticsStandard.hpp"
#include "ConcurrentGCStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ScanClassesMode.hpp"
#include "VerboseHandlerOutputStandardJava.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseHandlerJava.hpp"

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void verboseHandlerClassUnloadingEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
static void verboseHandlerSlowExclusive(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

MM_VerboseHandlerOutput *
MM_VerboseHandlerOutputStandardJava::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

	MM_VerboseHandlerOutputStandardJava *verboseHandlerOutput = (MM_VerboseHandlerOutputStandardJava *)extensions->getForge()->allocate(sizeof(MM_VerboseHandlerOutputStandardJava), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != verboseHandlerOutput) {
		new(verboseHandlerOutput) MM_VerboseHandlerOutputStandardJava(extensions);
		if(!verboseHandlerOutput->initialize(env, manager)) {
			verboseHandlerOutput->kill(env);
			verboseHandlerOutput = NULL;
		}
	}
	return verboseHandlerOutput;
}

bool
MM_VerboseHandlerOutputStandardJava::initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	bool initSuccess = MM_VerboseHandlerOutputStandard::initialize(env, manager);
	_mmHooks = J9_HOOK_INTERFACE(MM_GCExtensions::getExtensions(_extensions)->hookInterface);
	J9JavaVM *javaVM = (J9JavaVM *)env->getOmrVM()->_language_vm;
	_vmHooks = J9_HOOK_INTERFACE(javaVM->hookInterface);

	return initSuccess;
}

void
MM_VerboseHandlerOutputStandardJava::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseHandlerOutputStandard::tearDown(env);
}

void
MM_VerboseHandlerOutputStandardJava::outputMemoryInfoInnerStanzaInternal(MM_EnvironmentBase *env, UDATA indent, MM_CollectionStatistics *statsBase)
{
	MM_VerboseHandlerJava::outputFinalizableInfo(_manager, env, indent);
}

void
MM_VerboseHandlerOutputStandardJava::enableVerbose()
{
	MM_VerboseHandlerOutputStandard::enableVerbose();

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	(*_mmHooks)->J9HookRegisterWithCallSite(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, verboseHandlerClassUnloadingEnd, OMR_GET_CALLSITE(), (void *)this);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	(*_vmHooks)->J9HookRegisterWithCallSite(_vmHooks, J9HOOK_VM_SLOW_EXCLUSIVE, verboseHandlerSlowExclusive, OMR_GET_CALLSITE(), (void *)this);

}

void
MM_VerboseHandlerOutputStandardJava::disableVerbose()
{
	MM_VerboseHandlerOutputStandard::disableVerbose();

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	(*_mmHooks)->J9HookUnregister(_mmHooks, J9HOOK_MM_CLASS_UNLOADING_END, verboseHandlerClassUnloadingEnd, NULL);
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	(*_vmHooks)->J9HookUnregister(_vmHooks, J9HOOK_VM_SLOW_EXCLUSIVE, verboseHandlerSlowExclusive, NULL);

}

bool
MM_VerboseHandlerOutputStandardJava::getThreadName(char *buf, UDATA bufLen, OMR_VMThread *vmThread)
{
	return MM_VerboseHandlerJava::getThreadName(buf,bufLen,vmThread);
}

void
MM_VerboseHandlerOutputStandardJava::writeVmArgs(MM_EnvironmentBase* env)
{
	MM_VerboseHandlerJava::writeVmArgs(_manager, env, static_cast<J9JavaVM*>(_omrVM->_language_vm));
}

void
MM_VerboseHandlerOutputStandardJava::outputUnfinalizedInfo(MM_EnvironmentBase *env, UDATA indent, UDATA unfinalizedCandidates, UDATA unfinalizedEnqueuedCount)
{
	if(0 != unfinalizedCandidates) {
		_manager->getWriterChain()->formatAndOutput(env, indent, "<finalization candidates=\"%zu\" enqueued=\"%zu\" />", unfinalizedCandidates, unfinalizedEnqueuedCount);
	}
}

void
MM_VerboseHandlerOutputStandardJava::outputOwnableSynchronizerInfo(MM_EnvironmentBase *env, UDATA indent, UDATA ownableSynchronizerCandidates, UDATA ownableSynchronizerCleared)
{
	if (0 != ownableSynchronizerCandidates) {
		_manager->getWriterChain()->formatAndOutput(env, indent, "<ownableSynchronizers candidates=\"%zu\" cleared=\"%zu\" />", ownableSynchronizerCandidates, ownableSynchronizerCleared);
	}
}

void
MM_VerboseHandlerOutputStandardJava::outputReferenceInfo(MM_EnvironmentBase *env, UDATA indent, const char *referenceType, MM_ReferenceStats *referenceStats, UDATA dynamicThreshold, UDATA maxThreshold)
{
	if(0 != referenceStats->_candidates) {
		if (0 != maxThreshold) {
			_manager->getWriterChain()->formatAndOutput(env, indent, "<references type=\"%s\" candidates=\"%zu\" cleared=\"%zu\" enqueued=\"%zu\" dynamicThreshold=\"%zu\" maxThreshold=\"%zu\" />",
					referenceType, referenceStats->_candidates, referenceStats->_cleared, referenceStats->_enqueued, dynamicThreshold, maxThreshold);
		} else {
			_manager->getWriterChain()->formatAndOutput(env, indent, "<references type=\"%s\" candidates=\"%zu\" cleared=\"%zu\" enqueued=\"%zu\" />",
					referenceType, referenceStats->_candidates, referenceStats->_cleared, referenceStats->_enqueued);
		}
	}
}

void
MM_VerboseHandlerOutputStandardJava::handleMarkEndInternal(MM_EnvironmentBase* env, void *eventData)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	MM_MarkJavaStats *markJavaStats = &extensions->markJavaStats;
	MM_WorkPacketStats *workPacketStats = &_extensions->globalGCStats.workPacketStats;

	outputUnfinalizedInfo(env, 1, markJavaStats->_unfinalizedCandidates, markJavaStats->_unfinalizedEnqueued);

	outputOwnableSynchronizerInfo(env, 1, markJavaStats->_ownableSynchronizerCandidates, markJavaStats->_ownableSynchronizerCleared);

	outputReferenceInfo(env, 1, "soft", &markJavaStats->_softReferenceStats, extensions->getDynamicMaxSoftReferenceAge(), extensions->getMaxSoftReferenceAge());
	outputReferenceInfo(env, 1, "weak", &markJavaStats->_weakReferenceStats, 0, 0);
	outputReferenceInfo(env, 1, "phantom", &markJavaStats->_phantomReferenceStats, 0, 0);

	outputStringConstantInfo(env, 1, markJavaStats->_stringConstantsCandidates, markJavaStats->_stringConstantsCleared);

	if (workPacketStats->getSTWWorkStackOverflowOccured()) {
		_manager->getWriterChain()->formatAndOutput(env, 1, "<warning details=\"work packet overflow\" count=\"%zu\" packetcount=\"%zu\" />",
				workPacketStats->getSTWWorkStackOverflowCount(), workPacketStats->getSTWWorkpacketCountAtOverflow());
	}
}

void
MM_VerboseHandlerOutputStandardJava::handleSlowExclusive(J9HookInterface **hook, UDATA eventNum, void *eventData)
{
	J9VMSlowExclusiveEvent *event = (J9VMSlowExclusiveEvent *) eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread->omrVMThread);
	MM_VerboseManager *manager = getManager();
	MM_VerboseWriterChain *writer = manager->getWriterChain();

	char threadName[64];
	getThreadName(threadName,sizeof(threadName),event->currentThread->omrVMThread);

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0,"<warning details=\"slow exclusive request due to %s\" threadname=\"%s\" timems=\"%zu\" />", (event->reason == 1)?"JNICritical":"Exclusive Access", threadName, event->timeTaken);
	writer->flush(env);
	exitAtomicReportingBlock();

}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_VerboseHandlerOutputStandardJava::handleClassUnloadEnd(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_ClassUnloadingEndEvent* event = (MM_ClassUnloadingEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread->omrVMThread);
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	MM_ClassUnloadStats *classUnloadStats = &extensions->globalGCStats.classUnloadStats;
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 duration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&duration, classUnloadStats->_startTime, classUnloadStats->_endTime);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "classunload", env->_cycleState->_verboseContextID, duration, deltaTimeSuccess);

	U_64 setupTime   = j9time_hires_delta(classUnloadStats->_startSetupTime, classUnloadStats->_endSetupTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	U_64 scanTime    = j9time_hires_delta(classUnloadStats->_startScanTime, classUnloadStats->_endScanTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	U_64 postTime    = j9time_hires_delta(classUnloadStats->_startPostTime, classUnloadStats->_endPostTime, J9PORT_TIME_DELTA_IN_MICROSECONDS);
	/* !!!Note: classUnloadStats->_classUnloadMutexQuiesceTime is in us already, do not convert it again!!!*/

	writer->formatAndOutput(
			env, 1,
			"<classunload-info classloadercandidates=\"%zu\" classloadersunloaded=\"%zu\" classesunloaded=\"%zu\" anonymousclassesunloaded=\"%zu\""
			" quiescems=\"%llu.%03.3llu\" setupms=\"%llu.%03.3llu\" scanms=\"%llu.%03.3llu\" postms=\"%llu.%03.3llu\" />",
			classUnloadStats->_classLoaderCandidates, classUnloadStats->_classLoaderUnloadedCount, classUnloadStats->_classesUnloadedCount, classUnloadStats->_anonymousClassesUnloadedCount,
			classUnloadStats->_classUnloadMutexQuiesceTime / 1000, classUnloadStats->_classUnloadMutexQuiesceTime % 1000,
			setupTime / 1000, setupTime % 1000,
			scanTime / 1000, scanTime % 1000,
			postTime / 1000, postTime % 1000);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

#if defined(J9VM_GC_MODRON_SCAVENGER)
void
MM_VerboseHandlerOutputStandardJava::handleScavengeEndInternal(MM_EnvironmentBase* env, void *eventData)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	MM_ScavengeEndEvent* event = (MM_ScavengeEndEvent*)eventData;

	if (event->cycleEnd) {
		MM_ScavengerJavaStats *scavengerJavaStats = &extensions->scavengerJavaStats;

		outputUnfinalizedInfo(env, 1, scavengerJavaStats->_unfinalizedCandidates, scavengerJavaStats->_unfinalizedEnqueued);

		outputOwnableSynchronizerInfo(env, 1, scavengerJavaStats->_ownableSynchronizerCandidates, (scavengerJavaStats->_ownableSynchronizerCandidates - scavengerJavaStats->_ownableSynchronizerTotalSurvived));

		outputReferenceInfo(env, 1, "soft", &scavengerJavaStats->_softReferenceStats, extensions->getDynamicMaxSoftReferenceAge(), extensions->getMaxSoftReferenceAge());
		outputReferenceInfo(env, 1, "weak", &scavengerJavaStats->_weakReferenceStats, 0, 0);
		outputReferenceInfo(env, 1, "phantom", &scavengerJavaStats->_phantomReferenceStats, 0, 0);
	}
}
#endif /*defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

const char*
MM_VerboseHandlerOutputStandardJava::getConcurrentKickoffReason(void *eventData)
{
	MM_ConcurrentKickoffEvent* event = (MM_ConcurrentKickoffEvent*)eventData;
	const char* reasonString;

	if ((ConcurrentKickoffReason) event->reason == LANGUAGE_DEFINED_REASON) {

		switch (event->languageReason) {
		case FORCED_UNLOADING_CLASSES:
			reasonString = "unloading classes requested";
			break;
		case NO_LANGUAGE_KICKOFF_REASON:
			/* Should never be the case */
			reasonString = "none";
			break;
		default:
			reasonString = "unknown";
			break;
		}

	} else {
		/* Defer to generic reason handler.*/
		return MM_VerboseHandlerOutputStandard::getConcurrentKickoffReason(event);
	}

	return reasonString;
}


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
MM_VerboseHandlerOutputStandardJava::handleConcurrentHaltedInternal(MM_EnvironmentBase *env, void *eventData)
{
	MM_ConcurrentHaltedEvent* event = (MM_ConcurrentHaltedEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
#define CONCURRENT_STATUS_BUFFER_LENGTH 32
	char statusBuffer[CONCURRENT_STATUS_BUFFER_LENGTH];
	const char* statusString = MM_ConcurrentGCStats::getConcurrentStatusString(env, event->executionMode, statusBuffer, CONCURRENT_STATUS_BUFFER_LENGTH);
#undef CONCURRENT_STATUS_BUFFER_LENGTH
	const char* stateString = "Complete";
	if (0 == event->isTracingExhausted) {
		stateString = "Tracing incomplete";
	}

	switch (event->scanClassesMode) {
		case MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED:
		case MM_ScanClassesMode::SCAN_CLASSES_CURRENTLY_ACTIVE:
			stateString = "Class scanning incomplete";
			break;
		case MM_ScanClassesMode::SCAN_CLASSES_COMPLETE:
		case MM_ScanClassesMode::SCAN_CLASSES_DISABLED:
			break;
		default:
			stateString = "Class scanning bad state";
			break;
	}

	writer->formatAndOutput(env, 1, "<halted state=\"%s\" status=\"%s\" />", stateString, statusString);
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void
verboseHandlerClassUnloadingEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandardJava *)userData)->handleClassUnloadEnd(hook, eventNum, eventData);
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

void
verboseHandlerSlowExclusive(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	((MM_VerboseHandlerOutputStandardJava *)userData)->handleSlowExclusive(hook, eventNum, eventData);
}
