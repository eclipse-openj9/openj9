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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(CONCURRENTMARKINGDELEGATE_HPP_)
#define CONCURRENTMARKINGDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "j9class.h"
#include "j9consts.h"
#include "j9cp.h"
#include "j9modron.h"
#include "j9nongenerated.h"
#include "j9nonbuilder.h"
#include "modron.h"
#include "objectdescription.h"
#include "omrgcconsts.h"

#include "ConcurrentSafepointCallbackJava.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkingScheme.hpp"
#include "ReferenceObjectBuffer.hpp"
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
#include "ScanClassesMode.hpp"
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

class GC_VMThreadIterator;
class MM_ConcurrentGC;
class MM_MarkingScheme;

/**
 * Provides language-specific support for marking.
 */
class MM_ConcurrentMarkingDelegate
{
	/*
	 * Data members
	 */
private:

protected:
	J9JavaVM *_javaVM;
	GC_ObjectModel *_objectModel;
	MM_ConcurrentGC *_collector;
	MM_MarkingScheme *_markingScheme;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_ScanClassesMode _scanClassesMode; /** Support for dynamic class unloading in concurrent mark */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

public:
	/* This enum extends ConcurrentStatus with values in the exclusive range (CONCURRENT_ROOT_TRACING,
	 * CONCURRENT_TRACE_ONLY). ConcurrentStatus extensions allow the client language to define discrete
	 * units of work that can be executed in parallel by concurrent threads. ConcurrentGC will call
	 * MM_ConcurrentMarkingDelegate::collectRoots(..., concurrentStatus, ...) only once with each
	 * client-defined status value. The thread that receives the call can check the concurrentStatus
	 * value to select and execute the appropriate unit of work.
	 *
	 * @see ConcurrentStatus (omrgcconsts.h)
	 */
	enum {
		CONCURRENT_ROOT_TRACING1 = CONCURRENT_ROOT_TRACING + 1
		, CONCURRENT_ROOT_TRACING2 = CONCURRENT_ROOT_TRACING + 2
		, CONCURRENT_ROOT_TRACING3 = CONCURRENT_ROOT_TRACING + 3
		, CONCURRENT_ROOT_TRACING4 = CONCURRENT_ROOT_TRACING + 4
	};

	typedef struct markSchemeStackIteratorData {
		MM_MarkingScheme *markingScheme;
		MM_EnvironmentBase *env;
	} markSchemeStackIteratorData;

	/*
	 * Function members
	 */
private:
	void collectJNIRoots(MM_EnvironmentBase *env, bool *completedJNIRoots);
	void collectClassRoots(MM_EnvironmentBase *env, bool *completedClassRoots, bool *classesMarkedAsRoots);
	void collectFinalizableRoots(MM_EnvironmentBase *env, bool *completedFinalizableRoots);
	void collectStringRoots(MM_EnvironmentBase *env, bool *completedStringRoots, bool *collectedStringConstants);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	uintptr_t concurrentClassMark(MM_EnvironmentBase *env, bool *completedClassMark);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

protected:

public:
	/**
	 * Initialize the delegate.
	 *
	 * @param env environment for calling thread
	 * @return true if delegate initialized successfully
	 */
	bool initialize(MM_EnvironmentBase *env, MM_ConcurrentGC *collector);

	/**
	 * In the case of Weak Consistency platforms we require this method to bring mutator threads to a safe point. A safe
	 * point is a point at which a GC may occur.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return An instance of a MM_ConcurrentSafepointCallback instance that can signal all mutator threads and cause them
	 * to synchronize at a safe point
	 * @see MM_ConcurrentSafepointCallback
	 */
	MMINLINE MM_ConcurrentSafepointCallback*
	createSafepointCallback(MM_EnvironmentBase *env)
	{
		return MM_ConcurrentSafepointCallbackJava::newInstance(env);
	}

	/**
	 * Concurrent marking component maintains a background helper thread to assist with concurrent marking
	 * tasks. The delegate may provide a specialized signal handler (and associated argument) to process
	 * signals raised from this thread.
	 *
	 * @param[out] signalHandlerArg receives (nullable) pointer to argument to be passed to signal handler when invoked
	 * @return a pointer to the signal handler function (or NULL if no signal handler)
	 */
	MMINLINE omrsig_handler_fn
	getProtectedSignalHandler(void **signalHandlerArg)
	{
		*signalHandlerArg = (void *)_javaVM;
		return (omrsig_handler_fn)_javaVM->internalVMFunctions->structuredSignalHandlerVM;
	}

	/**
	 * Test whether a GC can be started. Under some circumstances it may be desirable to circumvent continued
	 * concurrent marking and allow a GC to kick off immediately. In that case this method should return true
	 * and set the kick off reason.
	 *
	 * If this method returns false, the GC cycle may be started immediately. Otherwise, concurrent marking
	 * will kick off and the GC cycle will be deferred until concurrent marking completes.
	 *
	 * @param env the calling thread environment
	 * @param gcCode the GC code identifying the cause of the GC request
	 * @param languageKickoffReason set this to the value to be reported as kickoff reson in verbose GC log
	 * @see J9MMCONSTANT_* (j9nonbuilder.h) for gcCode values
	 * @return true if Kickoff can be forced
	 */
	MMINLINE bool
	canForceConcurrentKickoff(MM_EnvironmentBase *env, uintptr_t gcCode, uintptr_t *languageKickoffReason)
	{
		if (J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES == gcCode) {
			*languageKickoffReason = FORCED_UNLOADING_CLASSES;
			return true;
		}
		return false;
	}

	MMINLINE uintptr_t
	getNextTracingMode(uintptr_t executionMode)
	{
		uintptr_t nextExecutionMode = CONCURRENT_TRACE_ONLY;

		switch (executionMode) {
		case CONCURRENT_ROOT_TRACING:
			nextExecutionMode = CONCURRENT_ROOT_TRACING1;
			break;
		case CONCURRENT_ROOT_TRACING1:
			nextExecutionMode = CONCURRENT_ROOT_TRACING2;
			break;
		case CONCURRENT_ROOT_TRACING2:
			nextExecutionMode = CONCURRENT_ROOT_TRACING3;
			break;
		case CONCURRENT_ROOT_TRACING3:
			nextExecutionMode = CONCURRENT_ROOT_TRACING4;
			break;
		case CONCURRENT_ROOT_TRACING4:
			nextExecutionMode = CONCURRENT_TRACE_ONLY;
			break;
		default:
			Assert_MM_unreachable();
		}

		return nextExecutionMode;
	}

	MMINLINE uintptr_t
	collectRoots(MM_EnvironmentBase *env, uintptr_t concurrentStatus, bool *collectedRoots, bool *paidTax)
	{
		uintptr_t bytesScanned = 0;
		*collectedRoots = true;
		*paidTax = true;

		switch (concurrentStatus) {
		case CONCURRENT_ROOT_TRACING1:
			collectJNIRoots(env, collectedRoots);
			break;
		case CONCURRENT_ROOT_TRACING2:
			collectClassRoots(env, paidTax, collectedRoots);
			break;
		case CONCURRENT_ROOT_TRACING3:
			collectFinalizableRoots(env, collectedRoots);
			break;
		case CONCURRENT_ROOT_TRACING4:
			collectStringRoots(env, paidTax, collectedRoots);
			break;
		default:
			Assert_MM_unreachable();
		}

		return bytesScanned;
	}

	MMINLINE void
	concurrentInitializationComplete(MM_EnvironmentBase *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		_scanClassesMode.setScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_DISABLED);	/* disable concurrent classes scan - default state */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	MMINLINE void cardCleaningStarted(MM_EnvironmentBase *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		/* If scanning of classes is enabled and was completed in CONCURRENT_TRACE_ONLY state - start it again*/
		_scanClassesMode.switchScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_COMPLETE, MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	/**
	 * Signal all threads for call back. Iterate over all threads and signal each to call scanThread()
	 * so we can scan their stacks for roots.
	 */
	void signalThreadsToTraceStacks(MM_EnvironmentBase *env);

	void signalThreadsToDirtyCards(MM_EnvironmentBase *env);

	void signalThreadsToStopDirtyingCards(MM_EnvironmentBase *env);

	/**
	 * This method is called during card cleaning for each object associated with an uncleaned, dirty card in the card
	 * table. No client actions are necessary but this method may be overridden if desired to hook into card cleaning.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to an object associated with an uncleaned, dirty card.
	 */
	MMINLINE void
	processItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		if (GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT == _objectModel->getScanType(objectPtr)) {
			/* since we popped this object from the work packet, it is our responsibility to record it in the list of reference objects */
			/* we know that the object must be in the collection set because it was on a work packet */
			/* we don't need to process cleared or enqueued references */
			if (GC_ObjectModel::REF_STATE_INITIAL == J9GC_J9VMJAVALANGREFERENCE_STATE(env, objectPtr)) {
				env->getGCEnvironment()->_referenceObjectBuffer->add(env, objectPtr);
			}
		}
	}

	/**
	 * Scan a thread structure and stack frames for roots. Implementation must call
	 * MM_MarkingScheme::markObject(MM_EnvironmentBase *, omrobjectptr_t) for
	 * each heap object reference found on the thread's stack or in thread structure.
	 *
	 * @param env the thread environment for the thread to be scanned
	 * @return true if the thread was scanned successfully
	 */
	bool scanThreadRoots(MM_EnvironmentBase *env);

	/**
	 * Flush any roots held in thread local buffers.
	 *
	 * @param env the thread environment for the thread to be flushed
	 * @return true if any data were flushed, false otherwise
	 */
	MMINLINE bool
	flushThreadRoots(MM_EnvironmentBase *env)
	{
		bool wasEmpty = env->getGCEnvironment()->_referenceObjectBuffer->isEmpty();
		env->getGCEnvironment()->_referenceObjectBuffer->flush(env);
		return !wasEmpty;
	}

	void abortCollection(MM_EnvironmentBase *env);

	/**
	 *
	 */
	MMINLINE bool
	startConcurrentScanning(MM_EnvironmentBase *env, uintptr_t *bytesTraced, bool *collectedRoots)
	{
		*bytesTraced = 0;
		*collectedRoots = false;
		bool started = false;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (_scanClassesMode.switchScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED, MM_ScanClassesMode::SCAN_CLASSES_CURRENTLY_ACTIVE)) {	/* currently not running */
			*bytesTraced = concurrentClassMark(env, collectedRoots);
			started = true;
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		return started;
	}

	MMINLINE void
	concurrentScanningStarted(MM_EnvironmentBase *env, uintptr_t bytesTraced)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (0 != bytesTraced) {
			_scanClassesMode.setScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_NEED_TO_BE_EXECUTED); /* need more iterations */
		} else {
			_scanClassesMode.setScanClassesMode(MM_ScanClassesMode::SCAN_CLASSES_COMPLETE); /* complete for now */
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	MMINLINE bool
	isConcurrentScanningComplete(MM_EnvironmentBase *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		return !_scanClassesMode.isPendingOrActiveMode();
#else
		return true;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	MMINLINE uintptr_t
	reportConcurrentScanningMode(MM_EnvironmentBase *env)
	{
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		return _scanClassesMode.getScanClassesMode();
#else
		return 0;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	/**
	 * Constructor.
	 */
	MMINLINE MM_ConcurrentMarkingDelegate()
		: _javaVM(NULL)
		, _objectModel(NULL)
		, _collector(NULL)
		, _markingScheme(NULL)
	{ }
};

#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
#endif /* CONCURRENTMARKINGDELEGATE_HPP_ */
