/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
#include "j9accessbarrier.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9user.h"
#include "j9consts.h"
#include "omrthread.h"
#include "jni.h"
#include "ModronAssertions.h"

#include "FinalizerSupport.hpp"

#include "AtomicOperations.hpp"
#include "ClassLoaderIterator.hpp"
#include "EnvironmentBase.hpp"
#include "FinalizeListManager.hpp"
#include "FinalizableObjectBuffer.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "ModronTypes.hpp"
#include "ObjectAccessBarrier.hpp"
#include "OMRVMInterface.hpp"
#include "SublistFragment.hpp"
#include "SublistIterator.hpp"
#include "SublistSlotIterator.hpp"
#include "SublistPuddle.hpp"
#include "UnfinalizedObjectBuffer.hpp"
#include "UnfinalizedObjectList.hpp"

extern "C" {
	
#if defined(J9VM_GC_FINALIZATION)

/**
 * finds and removes a class loader with thread notification from finalize list
 */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
void *
finalizeForcedClassLoaderUnload(J9VMThread *vmThread)
{
	void *returnValue = NULL;
	J9ClassLoader *classLoader = NULL;
	J9JavaVM *javaVM = vmThread->javaVM;
	GC_FinalizeListManager *finalizeListManager = MM_GCExtensions::getExtensions(javaVM)->finalizeListManager;

#if defined(J9VM_THR_PREEMPTIVE)
	finalizeListManager->lock();
	omrthread_monitor_enter(javaVM->classLoaderBlocksMutex);
#endif /* J9VM_THR_PREEMPTIVE */

	returnValue = (void *)finalizeListManager->popRequiredClassLoaderForForcedClassLoaderUnload();

	if (NULL == returnValue) {
		/* we did not find anything on the finalizeList now try the regular classloader list */
		GC_ClassLoaderIterator classLoaderIterator(javaVM->classLoaderBlocks);
	
		while (NULL != (classLoader = classLoaderIterator.nextSlot())) {
			if (!(classLoader->gcFlags & J9_GC_CLASS_LOADER_UNLOADING)) {
				if (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) {
					if (NULL != classLoader->gcThreadNotification) {
						// Class loader with pending threads found - process this class loader
						returnValue = classLoader;
						break;
					}
				}
			}
		} /* classLoaderIterator */
	}

#if defined(J9VM_THR_PREEMPTIVE)
	omrthread_monitor_exit(javaVM->classLoaderBlocksMutex);
	finalizeListManager->unlock();
#endif /* J9VM_THR_PREEMPTIVE */
		
	return returnValue;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


/**
 * Force objects from the unfinalized list to the finalizable lists.
 * 
 * @note System class loader types are skipped until all other types of objects are processed (jck issue).
 * @note Assumes the calling thread has VM access.
 * @note This routine is a temporary hack while the finalizer is rewritten.
 */
void
finalizeForcedUnfinalizedToFinalizable(J9VMThread *vmThread)
{
#if defined(J9VM_GC_FINALIZATION)
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	GC_FinalizeListManager *finalizeListManager = extensions->finalizeListManager;

	/* Drop the lock and go for exclusive */
	finalizeListManager->unlock();
	env->acquireExclusiveVMAccess();
	finalizeListManager->lock();

	/* ensure that all thread-local buffers of unfinalized objects are flushed */
	GC_OMRVMInterface::flushNonAllocationCaches(env);

	GC_FinalizableObjectBuffer buffer(extensions);
	/* process the lists */
	MM_UnfinalizedObjectList *unfinalizedObjectList = extensions->unfinalizedObjectLists;
	while(NULL != unfinalizedObjectList) {
		/* Iterate directly the current list. Not calling startUnfinalizedProcessing() to create and iterate priorList.
		 * If forced finalize occurs in a middle of a CS cycle, startUnfinalizedProcessing have been already done.
		 * Doing it again would overwrite existing prior list, plus all of objects would then be processed
		 * at the end of CS as being in unfinalized list, while ther are actually in finalizable list.
		 */
		J9Object *objectPtr = unfinalizedObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			J9Object* next = extensions->accessBarrier->getFinalizeLink(objectPtr);
			/* CMVC 181817: need to remember all objects forced onto the finalizable list */
			extensions->accessBarrier->forcedToFinalizableObject(vmThread, objectPtr);
			buffer.add(env, objectPtr);
			objectPtr = next;
		}
		/* Now that we moved out the content, make the list look empty (what typically startUnfinalizedProcessing does).
		 */
		unfinalizedObjectList->resetHeadOfList();

		/* Flush the local buffer of finalizable objects to the global list.
		 * This needs to be done once per unfinalized list to ensure that all
		 * the objects contained in the buffer are always from the same tenant
		 * in multi-tenant mode
		 */
		buffer.flush(env);

		unfinalizedObjectList = unfinalizedObjectList->getNextList();
	}

	env->releaseExclusiveVMAccess();
#endif /* J9VM_GC_FINALIZATION */
}

#define FINALIZE_WORKER_STAY_ALIVE 0
#define FINALIZE_WORKER_SHOULD_DIE 1
#define FINALIZE_WORKER_ABANDONED 2
#define FINALIZE_WORKER_SHOULD_ABANDON 3

#define FINALIZE_WORKER_MODE_NORMAL 0
#define FINALIZE_WORKER_MODE_FORCED 1
#define FINALIZE_WORKER_MODE_CL_UNLOAD 2

struct finalizeWorkerData {
	omrthread_monitor_t monitor;
	J9JavaVM *vm;
	J9VMThread *vmThread;
	IDATA finished;
	IDATA die;
	IDATA noWorkDone;
	IDATA mode;
	IDATA wakeUp;
};

static int J9THREAD_PROC FinalizeWorkerThread(void *arg);
IDATA FinalizeMainRunFinalization(J9JavaVM * vm, omrthread_t * indirectWorkerThreadHandle, struct finalizeWorkerData **indirectWorkerData, IDATA finalizeCycleLimit, IDATA mode);
static int J9THREAD_PROC FinalizeMainThread(void *javaVM);
static int  J9THREAD_PROC gpProtectedFinalizeWorkerThread(void *entryArg);

static int J9THREAD_PROC FinalizeMainThread(void *javaVM)
{
	J9JavaVM *vm = (J9JavaVM *)javaVM;
	omrthread_t workerThreadHandle;
	int doneRunFinalizersOnExit, noCycleWait;
	struct finalizeWorkerData *workerData = NULL;
	IDATA finalizeCycleInterval, finalizeCycleLimit, currentWaitTime, finalizableListUsed;
	IDATA cycleIntervalWaitResult;
	UDATA workerMode, savedFinalizeMainFlags;
	GC_FinalizeListManager *finalizeListManager;
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(vm->omrVM);
	MM_Forge *forge = extensions->getForge();

	/* explicitly set the name for main finalizer thread as it is not attached to VM */
	omrthread_set_name(omrthread_self(), "Finalizer main");

	vm->finalizeMainThread = omrthread_self();
	workerThreadHandle = NULL;
	noCycleWait = 0;

	finalizeListManager = extensions->finalizeListManager;
	
	/* Initialize the defaults */
	finalizeCycleInterval = extensions->finalizeCycleInterval;
	finalizeCycleLimit = extensions->finalizeCycleLimit;

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if(NULL != vm->javaOffloadSwitchOnNoEnvWithReasonFunc) {
		(*vm->javaOffloadSwitchOnNoEnvWithReasonFunc)(vm, vm->finalizeMainThread, J9_JNI_OFFLOAD_SWITCH_GC_FINALIZE_MAIN_THREAD);
	}
#endif

	currentWaitTime = 0;
	omrthread_monitor_enter(vm->finalizeMainMonitor);
	vm->finalizeMainFlags |= J9_FINALIZE_FLAGS_ACTIVE;
	omrthread_monitor_notify_all(vm->finalizeMainMonitor);

	do {
		if(currentWaitTime != -1 && !noCycleWait) {
			if(!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_MAIN_WORK_REQUEST)) {
				if(currentWaitTime == -2) {
					omrthread_yield();
				} else {
					do {
						cycleIntervalWaitResult = omrthread_monitor_wait_timed(vm->finalizeMainMonitor, currentWaitTime, 0);
					} while(!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_MAIN_WORK_REQUEST) && cycleIntervalWaitResult != J9THREAD_TIMED_OUT);
				}
			}
		}

		/* Check for a shutdown request, which overrides all other requests */
		if(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_SHUTDOWN)
			break;

		/* Check for a wake up request, which means the garbage collector has placed objects on the finalizable queue for processing */
		if(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_MAIN_WAKE_UP) {
			vm->finalizeMainFlags &= ~J9_FINALIZE_FLAGS_MAIN_WAKE_UP;
			currentWaitTime = finalizeCycleInterval;
		}

		/* Adjust the wait time based on how full the finalizable queue is (magic for now) */
		finalizableListUsed = finalizeListManager->getJobCount();
		if(0 != finalizableListUsed) {
			noCycleWait = 1;
		} else {
			noCycleWait = 0;
		}

		/* If RUN_FINALIZATION is set, make the interval time is set to -1 -> This will override any "wake up" request made by the garbage collector */
		if(vm->finalizeMainFlags & (J9_FINALIZE_FLAGS_RUN_FINALIZATION
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			 | J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		))
			currentWaitTime = -1;

		/* There is work to be done - run one finalization cycle */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			if(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD) {
				workerMode = FINALIZE_WORKER_MODE_CL_UNLOAD;
			} else {
				workerMode = FINALIZE_WORKER_MODE_NORMAL;
			}
#else /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
				workerMode = FINALIZE_WORKER_MODE_NORMAL;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		savedFinalizeMainFlags = vm->finalizeMainFlags;

		IDATA result = FinalizeMainRunFinalization(vm, &workerThreadHandle, &workerData, finalizeCycleLimit, workerMode);
		if(result < 0) {
			/* give up this run and hope next time will be better */
			currentWaitTime = 0;
			noCycleWait = 0;
			continue;
		}

		/* Determine whether the worker actually did finish it's work */
		omrthread_monitor_enter(workerData->monitor);
		if(workerData->finished) {
			if(workerData->noWorkDone) {
				workerData->noWorkDone = 0;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
				if(!(savedFinalizeMainFlags & J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD)) {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
					currentWaitTime = 0;
					if(savedFinalizeMainFlags & J9_FINALIZE_FLAGS_RUN_FINALIZATION) {
						vm->finalizeMainFlags &= ~J9_FINALIZE_FLAGS_RUN_FINALIZATION;
						omrthread_monitor_enter(vm->finalizeRunFinalizationMutex);
						omrthread_monitor_notify_all(vm->finalizeRunFinalizationMutex);
						omrthread_monitor_exit(vm->finalizeRunFinalizationMutex);
					}
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
				}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			}
		} else {
			/* The worker never finished during the allocated time - abandon it */
			workerData->die = FINALIZE_WORKER_ABANDONED;
			workerThreadHandle = NULL;
		}
		omrthread_monitor_exit(workerData->monitor);
	} while(!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_SHUTDOWN));

	/* Check if finalizers should be run on exit */
	if(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_RUN_FINALIZERS_ON_EXIT) {
		doneRunFinalizersOnExit = 0;
		while(!doneRunFinalizersOnExit) {
			IDATA result = 0;
			do {
				/* Keep trying, even if a worker requests that it be abandoned */
				result = FinalizeMainRunFinalization(vm, &workerThreadHandle, &workerData, finalizeCycleLimit, FINALIZE_WORKER_MODE_FORCED);
			} while(result == -2);

			if(result == -1) {
				/* There was a bad error - just move to the actual quit phase */
				break;
			}
	
			omrthread_monitor_enter(workerData->monitor);
			if(workerData->finished && workerData->noWorkDone) {
				/* No more work to be done */
				doneRunFinalizersOnExit = 1;
			}
			if(!workerData->finished) {
				/* The worker seems to be hung - just quit */
				doneRunFinalizersOnExit = 1;
				workerData->die = FINALIZE_WORKER_ABANDONED;
				workerThreadHandle = NULL;
			}
			omrthread_monitor_exit(workerData->monitor);
		}
	}

	/* We've been told to die */
	if(NULL != workerThreadHandle) {
		omrthread_monitor_exit((omrthread_monitor_t)vm->finalizeMainMonitor);
		omrthread_monitor_enter(workerData->monitor);
		workerData->die = FINALIZE_WORKER_SHOULD_DIE;
		omrthread_monitor_notify_all(workerData->monitor);
		omrthread_monitor_wait(workerData->monitor);
		omrthread_monitor_exit(workerData->monitor);
		omrthread_monitor_destroy(workerData->monitor);
		forge->free(workerData);
		omrthread_monitor_enter((omrthread_monitor_t)vm->finalizeMainMonitor);
	}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if(NULL != vm->javaOffloadSwitchOffNoEnvWithReasonFunc) {
		(*vm->javaOffloadSwitchOffNoEnvWithReasonFunc)(vm, vm->finalizeMainThread, J9_JNI_OFFLOAD_SWITCH_GC_FINALIZE_MAIN_THREAD);
	}
#endif

	/* Notify the main thread that we have shut down */
	vm->finalizeMainFlags |= J9_FINALIZE_FLAGS_SHUTDOWN_COMPLETE;
	vm->finalizeMainFlags &= ~J9_FINALIZE_FLAGS_ACTIVE;
	omrthread_monitor_notify_all((omrthread_monitor_t)vm->finalizeMainMonitor);

	/* If anyone is left waiting for forced finalization to complete, wake them up */
	if(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_RUN_FINALIZATION) {
		vm->finalizeMainFlags &= ~J9_FINALIZE_FLAGS_RUN_FINALIZATION;
		omrthread_monitor_enter(vm->finalizeRunFinalizationMutex);
		omrthread_monitor_notify_all(vm->finalizeRunFinalizationMutex);
		omrthread_monitor_exit(vm->finalizeRunFinalizationMutex);
	}

	omrthread_exit((omrthread_monitor_t)vm->finalizeMainMonitor);	/* exit the monitor and terminate the thread */

	/* NO GUARANTEED EXECUTION BEYOND THIS POINT */

	return 0;
}

static void
process_finalizable(J9VMThread *vmThread, j9object_t object, jclass j9VMInternalsClass, jmethodID runFinalizeMID)
{
	J9InternalVMFunctions* fns;
	J9JavaVM *vm;

	vm = vmThread->javaVM;
	fns = vm->internalVMFunctions;

	jobject localRef = fns->j9jni_createLocalRef((JNIEnv *)vmThread, object);

	fns->internalReleaseVMAccess(vmThread);

	if((NULL != j9VMInternalsClass) && (NULL != runFinalizeMID)) {
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
		/* Tell the interpreter to not register a user condition handler for this callin.
		 * Note: the interpreter clears J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION after reading it,
		 * 		 	so subsequent callins are not affected */
		vmThread->privateFlags |= J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION;
#endif
		((JNIEnv *)vmThread)->CallStaticVoidMethod(j9VMInternalsClass, runFinalizeMID, localRef);
		((JNIEnv *)vmThread)->ExceptionClear();
	}

	((JNIEnv *)vmThread)->DeleteLocalRef(localRef);

	fns->internalEnterVMFromJNI(vmThread);
}

static void
process_reference(J9VMThread *vmThread, j9object_t reference, jmethodID refMID)
{
	J9InternalVMFunctions* fns;
	J9JavaVM *vm;

	vm = vmThread->javaVM;
	fns = vm->internalVMFunctions;

	jobject localRef = fns->j9jni_createLocalRef((JNIEnv *)vmThread, reference);

	fns->internalReleaseVMAccess(vmThread);

	if (refMID) {
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
		/* Tell the interpreter to not register a user condition handler for this callin.
		 * Note: the interpreter clears J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION after reading it,
		 * 		 	so subsequent callins are not affected */
		vmThread->privateFlags |= J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION;
#endif
		((JNIEnv *)vmThread)->CallBooleanMethod(localRef, refMID);
		((JNIEnv *)vmThread)->ExceptionClear();
	}

	((JNIEnv *)vmThread)->DeleteLocalRef(localRef);

	fns->internalEnterVMFromJNI(vmThread);
}

static void
process_classloader(J9VMThread *vmThread, J9ClassLoader *classLoader)
{
	J9InternalVMFunctions* fns;
	J9JavaVM *vm;

	vm = vmThread->javaVM;
	fns = vm->internalVMFunctions;

	fns->internalReleaseVMAccess(vmThread);

	fns->internalEnterVMFromJNI(vmThread);
	Assert_MM_true(NULL == classLoader->classSegments);
	fns->freeClassLoader(classLoader, vm, vmThread, JNI_FALSE);
	fns->internalReleaseVMAccess(vmThread);

	fns->internalEnterVMFromJNI(vmThread);
}

static void
process(J9VMThread *vmThread, const GC_FinalizeJob *finalizeJob, jclass j9VMInternalsClass, jmethodID runFinalizeMID, jmethodID referenceEnqueueImplMID)
{
	if (FINALIZE_JOB_TYPE_OBJECT == (finalizeJob->type & FINALIZE_JOB_TYPE_OBJECT)) {
		process_finalizable(vmThread, finalizeJob->object, j9VMInternalsClass, runFinalizeMID);
	} else if (FINALIZE_JOB_TYPE_REFERENCE == (finalizeJob->type & FINALIZE_JOB_TYPE_REFERENCE)) {
		process_reference(vmThread, finalizeJob->reference, referenceEnqueueImplMID);
	} else if (FINALIZE_JOB_TYPE_CLASSLOADER == (finalizeJob->type & FINALIZE_JOB_TYPE_CLASSLOADER)) {
		process_classloader(vmThread, finalizeJob->classLoader);
	} else {
		Assert_MM_unreachable();
	}
}

/**
 * Worker thread consumes jobs from Finalize List Manager and process them
 */
static int J9THREAD_PROC FinalizeWorkerThread(void *arg)
{
	struct finalizeWorkerData *workerData = (struct finalizeWorkerData *)arg;
	J9VMThread *env;
	const GC_FinalizeJob *finalizeJob;
	GC_FinalizeJob localJob;
	jclass referenceClazz, j9VMInternalsClass = NULL;
	jmethodID referenceEnqueueImplMID = NULL, runFinalizeMID = NULL;
	J9InternalVMFunctions* fns;
	omrthread_monitor_t monitor;
	GC_FinalizeListManager *finalizeListManager;
	J9JavaVM *vm = (J9JavaVM *)(workerData->vm);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vm);
	MM_Forge *forge = extensions->getForge();

	fns = vm->internalVMFunctions;
	monitor = workerData->monitor;

	finalizeListManager = extensions->finalizeListManager;

	if (JNI_OK != vm->internalVMFunctions->attachSystemDaemonThread(vm, &env, "Finalizer thread")) {
		/* Failed to attach the thread - very bad, most likely out of memory */
		workerData->vmThread = (J9VMThread *)NULL;
		omrthread_monitor_enter(monitor);
		omrthread_monitor_notify_all(monitor);
		omrthread_monitor_exit(monitor);
		return 0;
	}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if( vm->javaOffloadSwitchOnWithReasonFunc != NULL ) {
		(*vm->javaOffloadSwitchOnWithReasonFunc)((J9VMThread *)env, J9_JNI_OFFLOAD_SWITCH_FINALIZE_WORKER_THREAD);
		((J9VMThread *)env)->javaOffloadState = 1;
	}
#endif

	fns->internalEnterVMFromJNI(env);
	env->privateFlags |= (J9_PRIVATE_FLAGS_FINALIZE_WORKER | J9_PRIVATE_FLAGS_USE_BOOTSTRAP_LOADER);
	fns->internalReleaseVMAccess(env);

	/* Remember that the thread was gpProtected -- important for the JIT */
	env->gpProtected = 1;

	if(vm->jclFlags & J9_JCL_FLAG_FINALIZATION) {
		/* Only look up finalization methods if the class library supports them */
		j9VMInternalsClass = ((JNIEnv *)env)->FindClass("java/lang/J9VMInternals");
		if (j9VMInternalsClass) {
			j9VMInternalsClass = (jclass)((JNIEnv *)env)->NewGlobalRef(j9VMInternalsClass);
			if (j9VMInternalsClass) {
				runFinalizeMID = ((JNIEnv *)env)->GetStaticMethodID(j9VMInternalsClass, "runFinalize", "(Ljava/lang/Object;)V");
			}
		}
		if (!runFinalizeMID) {
			((JNIEnv *)env)->ExceptionClear();
		}
	
		referenceClazz = ((JNIEnv *)env)->FindClass("java/lang/ref/Reference");
		if (referenceClazz) {
			referenceEnqueueImplMID  = ((JNIEnv *)env)->GetMethodID(referenceClazz, "enqueueImpl", "()Z");
		}
		if (!referenceEnqueueImplMID) {
			((JNIEnv *)env)->ExceptionClear();
		}
	}		
	workerData->vmThread = env;

	/* Notify that the worker has come on line (We should check the result from above) */
	omrthread_monitor_enter(monitor);
	omrthread_monitor_notify_all(monitor);

	do {
		if(!workerData->wakeUp) {
			omrthread_monitor_wait(monitor);
		}
		workerData->wakeUp = 0;

		if(workerData->die != FINALIZE_WORKER_STAY_ALIVE) {
			continue;
		}

		omrthread_monitor_exit(monitor);

		fns->internalEnterVMFromJNI(env);
		
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if(workerData->mode != FINALIZE_WORKER_MODE_CL_UNLOAD)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		{
			if ((NULL != vm->processReferenceMonitor) && (0 != finalizeListManager->getReferenceCount())) {
				omrthread_monitor_enter(vm->processReferenceMonitor);
				vm->processReferenceActive = 1;
				omrthread_monitor_exit(vm->processReferenceMonitor);
			}
		}

		do {

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			if(workerData->mode == FINALIZE_WORKER_MODE_CL_UNLOAD) {
				
				if (NULL == (localJob.classLoader = (J9ClassLoader *)finalizeForcedClassLoaderUnload((J9VMThread *)env))) {
					break;
				} else {
					localJob.type = FINALIZE_JOB_TYPE_CLASSLOADER;
					finalizeJob = &localJob;
				}

			} else {
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


				finalizeListManager->lock();
				
				finalizeJob = finalizeListManager->consumeJob(env, &localJob);
				if(finalizeJob == NULL) {
					if(workerData->mode == FINALIZE_WORKER_MODE_FORCED) {
						finalizeForcedUnfinalizedToFinalizable(env);
						finalizeJob = finalizeListManager->consumeJob(env, &localJob);
					}
				}

				finalizeListManager->unlock();
				
				if(NULL != finalizeJob) {
					workerData->noWorkDone = 0;
				} else {
					workerData->noWorkDone = 1;
					break;				
				}
				
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
			}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

			/* processing will release/acquire VM access */
			process(env, finalizeJob, j9VMInternalsClass, runFinalizeMID, referenceEnqueueImplMID);

			if ((NULL != vm->processReferenceMonitor) && (0 != vm->processReferenceActive)) {
				omrthread_monitor_enter(vm->processReferenceMonitor);
				if (0 == finalizeListManager->getReferenceCount()) {
					/* There is no more pending reference. */
					vm->processReferenceActive = 0;
				}
				/*
				 * Notify any waiters that progress has been made.
				 * This improves latency for Reference.waitForReferenceProcessing() and try to
				 * avoid the performance issue if there are many of pending references in the queue.
				 */
				omrthread_monitor_notify_all(vm->processReferenceMonitor);
				omrthread_monitor_exit(vm->processReferenceMonitor);
			}

			fns->jniResetStackReferences((JNIEnv *)env);

			if(FINALIZE_WORKER_SHOULD_ABANDON == workerData->die) {
				/* We've been abandoned, finish up */
				break;
			}
		} while (true);

		fns->internalReleaseVMAccess(env);

		workerData->finished = 1;

		/* Notify the main that the work is complete */
		omrthread_monitor_enter(monitor);
		omrthread_monitor_notify_all(monitor);
	} while(workerData->die == FINALIZE_WORKER_STAY_ALIVE);
	
	if (j9VMInternalsClass) {
		((JNIEnv *)env)->DeleteGlobalRef(j9VMInternalsClass);
	}

	((JavaVM *)vm)->DetachCurrentThread();

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if( vm->javaOffloadSwitchOffNoEnvWithReasonFunc != NULL ) {
		(*vm->javaOffloadSwitchOffNoEnvWithReasonFunc)(vm, omrthread_self(), J9_JNI_OFFLOAD_SWITCH_FINALIZE_WORKER_THREAD);
	}
#endif

	switch(workerData->die) {
		case FINALIZE_WORKER_SHOULD_ABANDON:
			/* Poke the main in case it missed the notify */
			omrthread_monitor_notify_all(workerData->monitor);

			/* Now wait for the main to give us the OK to die */
			while(FINALIZE_WORKER_SHOULD_ABANDON == workerData->die) {
				omrthread_monitor_wait(workerData->monitor);
			}
			Assert_MM_true(FINALIZE_WORKER_ABANDONED == workerData->die);
			/* Fallthrough to the abandoned case */

		case FINALIZE_WORKER_ABANDONED:
			/* Clean up communication data structures data structures */
			omrthread_monitor_exit(workerData->monitor);
			omrthread_monitor_destroy(workerData->monitor);
			forge->free(workerData);
			break;
		case FINALIZE_WORKER_SHOULD_DIE:
			omrthread_monitor_notify_all(workerData->monitor);
			omrthread_exit(workerData->monitor);		/* exit the monitor, and terminate the thread */
			/* NO EXECUTION GUARANTEE BEYOND THIS POINT */
	}

	/* NO EXECUTION GUARANTEE BEYOND THIS POINT */

	return 0;
}

/*
 * Preconditions:
 * 	holds finalizeMainMonitor
 * 	does not hold workerData->monitor
 * Postconditions:
 * 	holds finalizeMainMonitor
 * 	does not hold workerData->monitor
 */
IDATA FinalizeMainRunFinalization(J9JavaVM * vm, omrthread_t * indirectWorkerThreadHandle,
										   struct finalizeWorkerData **indirectWorkerData, IDATA finalizeCycleLimit,
										   IDATA mode)
{
	omrthread_t workerThreadHandle;
	struct finalizeWorkerData *workerData;
	IDATA workerWaitResult;
	UDATA publicFlags;
	MM_Forge *forge = MM_GCExtensionsBase::getExtensions(vm->omrVM)->getForge();
	
	workerThreadHandle = *indirectWorkerThreadHandle;
	workerData = *indirectWorkerData;

	/* There is work to be done - if no worker thread exists, create one */
	if (NULL == workerThreadHandle) {
		/* Initialize a workerData structure */
		workerData = (struct finalizeWorkerData *) forge->allocate(sizeof(struct finalizeWorkerData), MM_AllocationCategory::FINALIZE, J9_GET_CALLSITE());
		if (NULL == workerData) {
			/* What should be done here! */
			return -1;
		}
		workerData->vm = vm;
		workerData->die = FINALIZE_WORKER_STAY_ALIVE;
		workerData->noWorkDone = 0;
		workerData->mode = FINALIZE_WORKER_MODE_NORMAL;
		workerData->wakeUp = 0;

		if (0 != omrthread_monitor_init(&(workerData->monitor), 0)) {
			forge->free(workerData);

			/* What should be done here! */
			return -1;
		}
		omrthread_monitor_exit(vm->finalizeMainMonitor);
		omrthread_monitor_enter(workerData->monitor);

		/* Fork the worker thread */
		IDATA result = vm->internalVMFunctions->createThreadWithCategory(
							&workerThreadHandle,
							vm->defaultOSStackSize,
							MM_GCExtensions::getExtensions(vm)->finalizeWorkerPriority,
							0,
							&gpProtectedFinalizeWorkerThread,
							workerData,
							J9THREAD_CATEGORY_APPLICATION_THREAD);

		if (result != 0) {
			omrthread_monitor_exit(workerData->monitor);
			omrthread_monitor_destroy(workerData->monitor);			
			forge->free(workerData);
			omrthread_monitor_enter(vm->finalizeMainMonitor);
			return -1;
		}
		omrthread_monitor_wait(workerData->monitor);
		if (!workerData->vmThread) {
			/* The worker thread failed to initialize/attach - this is really bad */
			omrthread_monitor_exit(workerData->monitor);
			omrthread_monitor_destroy(workerData->monitor);
			forge->free(workerData);
			omrthread_monitor_enter(vm->finalizeMainMonitor);
			return -1;
		}
		omrthread_monitor_exit(workerData->monitor);
		omrthread_monitor_enter(vm->finalizeMainMonitor);

		*indirectWorkerData = workerData;
		*indirectWorkerThreadHandle = workerThreadHandle;

		/* Connect the worker */
		vm->finalizeWorkerData = workerData;
	}

	/* A worker exists - set it to work */
	omrthread_monitor_exit(vm->finalizeMainMonitor);

	omrthread_monitor_enter(workerData->monitor);
	workerData->wakeUp = 1;
	workerData->mode = mode;
	workerData->finished = 0;
	omrthread_monitor_notify_all(workerData->monitor);	/* Wake worker up */
	do {
		workerWaitResult = omrthread_monitor_wait_timed(workerData->monitor, finalizeCycleLimit, 0);

		omrthread_monitor_enter(workerData->vmThread->publicFlagsMutex);
		publicFlags = workerData->vmThread->publicFlags;
		omrthread_monitor_exit(workerData->vmThread->publicFlagsMutex);
	}
	while (
		   (workerWaitResult == J9THREAD_TIMED_OUT && publicFlags & J9_PUBLIC_FLAGS_HALT_VM_DUTIES &&
			!workerData->finished) || (workerWaitResult != J9THREAD_TIMED_OUT && !workerData->finished));
	omrthread_monitor_exit(workerData->monitor);

	omrthread_monitor_enter(vm->finalizeMainMonitor);

	if(FINALIZE_WORKER_SHOULD_ABANDON == workerData->die) {
		/* The worker thread has requested that we abandon it */

		/* Disconnect the worker */
		vm->finalizeWorkerData = NULL;
		*indirectWorkerThreadHandle = NULL;
		*indirectWorkerData = NULL;

		/* Let the abandoned worker know that it can clean up */
		omrthread_monitor_enter(workerData->monitor);
		workerData->die = FINALIZE_WORKER_ABANDONED;
		omrthread_monitor_notify_all(workerData->monitor);
		omrthread_monitor_exit(workerData->monitor);

		return -2;
	}

	return workerWaitResult;
}

static UDATA
FinalizeWorkerThreadGlue(J9PortLibrary* portLib, void* userData)
{
	return FinalizeWorkerThread(userData);	
}

static int J9THREAD_PROC 
gpProtectedFinalizeWorkerThread(void *entryArg)
{
	struct finalizeWorkerData *workerData = (struct finalizeWorkerData *) entryArg;
	PORT_ACCESS_FROM_PORT(workerData->vm->portLibrary);
	UDATA rc;

	j9sig_protect(FinalizeWorkerThreadGlue, workerData, 
		workerData->vm->internalVMFunctions->structuredSignalHandlerVM, workerData->vm,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&rc);

	return 0;
}

void
j9gc_finalizer_completeFinalizersOnExit(J9VMThread* vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;

	/* If finalization has already been shut down, do nothing */
	if (!J9_ARE_ALL_BITS_SET(vm->finalizeMainFlags, J9_FINALIZE_FLAGS_ACTIVE)) {
		return;
	}

	/* Set the run finalizers on exit flag and initiate finalizer shutdown. */
	omrthread_monitor_enter(vm->finalizeMainMonitor);
	vm->finalizeMainFlags |= J9_FINALIZE_FLAGS_RUN_FINALIZERS_ON_EXIT;
	if (!J9_ARE_ALL_BITS_SET(vm->finalizeMainFlags, J9_FINALIZE_FLAGS_SHUTDOWN)) {
		vm->finalizeMainFlags |= J9_FINALIZE_FLAGS_SHUTDOWN;
		omrthread_monitor_notify_all(vm->finalizeMainMonitor);
	}
	/* Is there an active worker thread? */
	if (NULL != vm->finalizeWorkerData) {
		struct finalizeWorkerData *workerData = (struct finalizeWorkerData*)vm->finalizeWorkerData;
		if ((NULL != workerData) && (0 == workerData->finished)) {
			/* An active worker thread exists (possibly the current thread).
			 * Abandon it so that a new worker can be created.
			 */
			omrthread_monitor_enter(workerData->monitor);
			if (0 == workerData->finished) {
				workerData->finished = 1;
				workerData->die = FINALIZE_WORKER_SHOULD_ABANDON;
				omrthread_monitor_notify_all(workerData->monitor);
			}
			omrthread_monitor_exit(workerData->monitor);
		}
	}

	/* Now block until finalizer shutdown is complete */
	omrthread_monitor_notify_all(vm->finalizeMainMonitor);
	while (!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_SHUTDOWN_COMPLETE)) {
		omrthread_monitor_wait(vm->finalizeMainMonitor);
	}
	omrthread_monitor_exit(vm->finalizeMainMonitor);
}

int j9gc_finalizer_startup(J9JavaVM * vm)
{
	IDATA result;

	omrthread_monitor_enter(vm->finalizeMainMonitor);

	result = vm->internalVMFunctions->createThreadWithCategory(
				NULL,
				vm->defaultOSStackSize,
				MM_GCExtensions::getExtensions(vm)->finalizeMainPriority,
				0,
				&FinalizeMainThread,
				vm,
				J9THREAD_CATEGORY_SYSTEM_GC_THREAD);

	if (0 != result) {
		omrthread_monitor_exit(vm->finalizeMainMonitor);
		return -1;
	}

	while (!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_ACTIVE)) {
		omrthread_monitor_wait(vm->finalizeMainMonitor);
	}
	omrthread_monitor_exit(vm->finalizeMainMonitor);

	return 0;
}

/**
 * Check if processing reference is active
 *
 * @param vm  Pointer to the Java VM
 * @return 1 if processing reference is active, otherwise return 0.
 */
UDATA j9gc_wait_for_reference_processing(J9JavaVM *vm)
{
	UDATA ret = 0;
	if (NULL != vm->processReferenceMonitor) {
		omrthread_monitor_enter(vm->processReferenceMonitor);
		if (0 != vm->processReferenceActive) {
			omrthread_monitor_wait(vm->processReferenceMonitor);
			ret = 1;
		}
		omrthread_monitor_exit(vm->processReferenceMonitor);
	}
	return ret;
}

/**
 * Precondition: JVMTI must be in JVMTI_PHASE_DEAD
 */
void j9gc_finalizer_shutdown(J9JavaVM * vm)
{
	J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);

	omrthread_monitor_enter(vm->finalizeMainMonitor);
	if(!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_SHUTDOWN)) {
		if ( (vm->finalizeMainFlags & J9_FINALIZE_FLAGS_ACTIVE)
				&& ( (vmThread && !(vmThread->privateFlags & J9_PRIVATE_FLAGS_FINALIZE_WORKER)) || !vmThread) ) {
			bool waitForFinalizer = true;
			struct finalizeWorkerData *workerData = (struct finalizeWorkerData*)vm->finalizeWorkerData;

			vm->finalizeMainFlags |= J9_FINALIZE_FLAGS_SHUTDOWN;
			omrthread_monitor_notify_all(vm->finalizeMainMonitor);
			if ((NULL != workerData) && (NULL != workerData->vmThread)
					&& J9_ARE_ANY_BITS_SET(workerData->vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)) {
				/*
				 * PR 87639 - don't wait for the finalizer if it has been suspended.
				 * This will cause jniinv:terminateRemainingThreads() to fail.
				 */
				waitForFinalizer = false;
			}
			if (waitForFinalizer) {
				while (!(vm->finalizeMainFlags & J9_FINALIZE_FLAGS_SHUTDOWN_COMPLETE)) {
					omrthread_monitor_wait(vm->finalizeMainMonitor);
				}
			}
		}
	}
	omrthread_monitor_exit(vm->finalizeMainMonitor);
}

/**
 * Called whenever a finalizeable object is created. Places the object on the unfinalized List.
 * @param vmThread
 * @param object The object to be finalized.
 * @returns 0 if the object was successfully placed on the unfinalized list, UDATA_MAX otherwise.
 */
UDATA 
finalizeObjectCreated(J9VMThread *vmThread, j9object_t object) 
{
	Trc_FinalizeSupport_finalizeObjectCreated_Entry(vmThread, object);
	
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	env->getGCEnvironment()->_unfinalizedObjectBuffer->add(env, object);

	Trc_FinalizeSupport_finalizeObjectCreated_Exit(vmThread, 0);
	return 0;
}

/**
 * Must not hold VM access when calling runFinalization.
 */
void 
runFinalization(J9VMThread *vmThread) 
{
	Trc_FinalizeSupport_runFinalization_Entry(vmThread);
	J9JavaVM *jvm = vmThread->javaVM;

	Assert_MM_mustNotHaveVMAccess(vmThread);
	
	/* Bump the run finalizers count and signal the main finalize thread if necessary */
	omrthread_monitor_enter(jvm->finalizeMainMonitor);
	if ( 0 == jvm->finalizeRunFinalizationCount ) {
		omrthread_monitor_notify_all(jvm->finalizeMainMonitor);
	}
	jvm->finalizeMainFlags |= J9_FINALIZE_FLAGS_RUN_FINALIZATION;
	jvm->finalizeRunFinalizationCount += 1;
	omrthread_monitor_exit(jvm->finalizeMainMonitor);
	
	/* The main flags are checked without mutex protection, but no writes, so it's safe */
	omrthread_monitor_enter(jvm->finalizeRunFinalizationMutex);
	if ( 0 != (jvm->finalizeMainFlags & J9_FINALIZE_FLAGS_RUN_FINALIZATION) ) {
		/* TODO: The 1000ms wait time arbitrary. This number should probably come from somewhere else. */
		omrthread_monitor_wait_timed(jvm->finalizeRunFinalizationMutex,1000,0);
	}
	omrthread_monitor_exit(jvm->finalizeRunFinalizationMutex);
	
	/* stop the run finalizers request and signal the main monitor if necessary */
	omrthread_monitor_enter(jvm->finalizeMainMonitor);
	jvm->finalizeRunFinalizationCount -= 1;
	if ( 0 == jvm->finalizeRunFinalizationCount ) {
		jvm->finalizeMainFlags &= ~((UDATA)J9_FINALIZE_FLAGS_RUN_FINALIZATION);
		omrthread_monitor_notify_all(jvm->finalizeMainMonitor);
	}
	omrthread_monitor_exit(jvm->finalizeMainMonitor);
	
	Trc_FinalizeSupport_runFinalization_Exit(vmThread);
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/**
 * Forcibly unloads the given class loader provided its gcFlags has the J9_GC_CLASS_LOADER_DEAD
 * bit set. Otherwise, calls runFinalization() and invoke the GC twice.
 * Assumptions: The current thread has the classLoaderBlocksMutex.  Must not hold VM Access.
 * 
 * @param vmThread
 * @param classLoader the class loader to forcibly unload
 * @return 0 on success if the class loader is unloaded, 1 if a failure occurred.
 */
UDATA 
forceClassLoaderUnload(J9VMThread *vmThread, J9ClassLoader *classLoader) 
{
	Trc_FinalizeSupport_forceClassLoaderUnload_Entry(vmThread,classLoader);
	UDATA result = 0;
	bool setForceFlag = false;
	J9JavaVM *jvm = vmThread->javaVM;

	Assert_MM_mustNotHaveVMAccess(vmThread);

	if ( 0 != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
		if ( 0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_UNLOADING) ) {
			/* Class loader is not being unloaded but is eligible. Post the request and wait. */
			Trc_FinalizeSupport_forceClassLoaderUnload_classLoaderNotBeingUnloaded(vmThread,classLoader);
			setForceFlag = true;
		}
		/* The class loader is in the process of being unloaded.
		 * Enqueue the thread for a response and wait 
		 */
		if ( NULL == vmThread->gcClassUnloadingMutex ) {
			/* There's no signaling mutex for this thread yet, so allocate one */
			if ( 0 != omrthread_monitor_init(&vmThread->gcClassUnloadingMutex,0) ) {
				/* Failed to initialize the gcClassUnloadingMutex */
				Trc_FinalizeSupport_forceClassLoaderUnload_failedToInitializeClassUnloadingMutex(vmThread);
				result = 1;
			} 
		}
		
		if ( NULL != vmThread->gcClassUnloadingMutex ) {
			/* Link the thread into the notification list of the class loader */
			J9VMThread *tempNextThread = classLoader->gcThreadNotification;
			classLoader->gcThreadNotification = vmThread;
			vmThread->gcClassUnloadingThreadNext = tempNextThread;
			if ( NULL != tempNextThread ) {
				tempNextThread->gcClassUnloadingThreadPrevious = vmThread;
			}
			
			/* If we need to signal the finalzer to force unloading a class loader, do so */
			if ( setForceFlag ) {
				omrthread_monitor_enter(jvm->finalizeMainMonitor);
				jvm->finalizeMainFlags |= J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD;
				jvm->finalizeForceClassLoaderUnloadCount += 1;
				omrthread_monitor_notify_all(jvm->finalizeMainMonitor);
				omrthread_monitor_exit(jvm->finalizeMainMonitor);
			}
			
			/* Wait for notification that the class loader has finished unloading */
			omrthread_monitor_exit(jvm->classLoaderBlocksMutex);
			omrthread_monitor_enter(vmThread->gcClassUnloadingMutex);
			
			IDATA waitResult = omrthread_monitor_wait_timed(vmThread->gcClassUnloadingMutex,5000,0);
			omrthread_monitor_exit(vmThread->gcClassUnloadingMutex);
			omrthread_monitor_enter(jvm->classLoaderBlocksMutex);
			
			/* If force finalize flag was set, remove 1 from the count and clear the flag if necessary */
			if ( setForceFlag ) {
				omrthread_monitor_enter(jvm->finalizeMainMonitor);
				jvm->finalizeForceClassLoaderUnloadCount -= 1;
				if ( 0 == jvm->finalizeForceClassLoaderUnloadCount ) {
					jvm->finalizeMainFlags |= J9_FINALIZE_FLAGS_FORCE_CLASS_LOADER_UNLOAD;
				}
				omrthread_monitor_notify_all(jvm->finalizeMainMonitor);
				omrthread_monitor_exit(jvm->finalizeMainMonitor);
			}
			
			if ( J9THREAD_TIMED_OUT == waitResult ) {
				/* Lock down the signalling mechanism. If we appear to be still linked up, it is safe to unlink
				 *  (the class loader mutex is still in our possession)
				 */
				Trc_FinalizeSupport_forceClassLoaderUnload_timedOut(vmThread,classLoader);
				omrthread_monitor_enter(vmThread->gcClassUnloadingMutex);
				if ( (NULL != vmThread->gcClassUnloadingThreadPrevious) || (NULL != vmThread->gcClassUnloadingThreadNext) ) {
					if (NULL == vmThread->gcClassUnloadingThreadPrevious) {
						classLoader->gcThreadNotification = vmThread->gcClassUnloadingThreadNext;
					} else {
						vmThread->gcClassUnloadingThreadPrevious->gcClassUnloadingThreadNext = vmThread->gcClassUnloadingThreadNext;
					}
					if (NULL != vmThread->gcClassUnloadingThreadNext) {
						vmThread->gcClassUnloadingThreadNext->gcClassUnloadingThreadPrevious = vmThread->gcClassUnloadingThreadPrevious;
					}
					vmThread->gcClassUnloadingThreadNext = NULL;
					vmThread->gcClassUnloadingThreadPrevious = NULL;
				}
				omrthread_monitor_exit(vmThread->gcClassUnloadingMutex);
				result = 1;
			}
		}
	} else {
		/** If the classLoader is not dead, run the GC aggressively to see if that causes the classLoader
		 * to be marked as dead.
		 */
		Trc_FinalizeSupport_forceClassLoaderUnload_classLoaderNotDead(vmThread,classLoader);
		omrthread_monitor_exit(jvm->classLoaderBlocksMutex);
		runFinalization(vmThread);
		jvm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
		j9gc_modron_global_collect(vmThread);
		jvm->internalVMFunctions->internalReleaseVMAccess(vmThread);
		runFinalization(vmThread);
		jvm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
		j9gc_modron_global_collect(vmThread);
		jvm->internalVMFunctions->internalReleaseVMAccess(vmThread);
		omrthread_monitor_enter(jvm->classLoaderBlocksMutex);
	}
	
	Trc_FinalizeSupport_forceClassLoaderUnload_Exit(vmThread,result);
	return result;
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

/**
 * Set a global flag which determines if finalizers are run on exit.
 * This is used to implement java.lang.Runtime.runFinalizersOnExit().
 * 
 * @param vmThread[in] the current thread
 * @param run TRUE if finalizers should be run, false otherwise 
 */
void
j9gc_runFinalizersOnExit(J9VMThread* vmThread, UDATA run)
{
	J9JavaVM* jvm = vmThread->javaVM;
	
	omrthread_monitor_enter(jvm->finalizeMainMonitor);
	if (FALSE == run) {
		jvm->finalizeMainFlags &= ~(UDATA)J9_FINALIZE_FLAGS_RUN_FINALIZERS_ON_EXIT;
	} else {
		jvm->finalizeMainFlags |= (UDATA)J9_FINALIZE_FLAGS_RUN_FINALIZERS_ON_EXIT;
	}
	omrthread_monitor_exit(jvm->finalizeMainMonitor);
}

#endif /* J9VM_GC_FINALIZATION */

} /* extern "C" */

