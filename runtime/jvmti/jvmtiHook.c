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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include "j2sever.h"

typedef IDATA (*J9HookRedirectorFunction)(J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData);

#define ENSURE_EVENT_PHASE_PRIMORDIAL_START_OR_LIVE(func, env) \
	do { \
		UDATA phase = J9JVMTI_PHASE(env); \
		if ((phase != JVMTI_PHASE_LIVE) && (phase != JVMTI_PHASE_START) && (phase != JVMTI_PHASE_PRIMORDIAL)) { \
			TRACE_JVMTI_EVENT_RETURN(func); \
		} \
	} while(0)

#define ENSURE_EVENT_PHASE_START_OR_LIVE(func, env) \
	do { \
		UDATA phase = J9JVMTI_PHASE(env); \
		if ((phase != JVMTI_PHASE_LIVE) && (phase != JVMTI_PHASE_START)) { \
			TRACE_JVMTI_EVENT_RETURN(func); \
		} \
	} while(0)

#define ENSURE_EVENT_PHASE_LIVE(func, env) \
	if (J9JVMTI_PHASE(env) != JVMTI_PHASE_LIVE) { \
		TRACE_JVMTI_EVENT_RETURN(func); \
	}

#define TRACE_JVMTI_EVENT_RETURN(func) TRACE2_JVMTI_EVENT_RETURN(func)
#define TRACE2_JVMTI_EVENT_RETURN(func) \
	do { \
		Trc_JVMTI_##func##_Exit(); \
		return; \
	} while(0)


static void jvmtiHookPermanentPC (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookThreadCreated (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookThreadDestroy (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookGCEnd (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookGCCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookClassPrepare (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static IDATA processEvent (J9JVMTIEnv* j9env, jint event, J9HookRedirectorFunction redirectorFunction);
static void jvmtiHookMonitorContendedEntered (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookMonitorWait (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void removeUnloadedFieldWatches (J9JVMTIEnv * j9env, J9Class * unloadedClass);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void jvmtiHookClassUnload (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
static IDATA hookRegister (J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData);
static void jvmtiHookExceptionCatch (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookExceptionThrow (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void jvmtiHookCompilingEnd (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* INTERP_NATIVE_SUPPORT */
static void jvmtiHookClassLoad (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMInitializedFirst (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMInitialized (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMShutdownLast (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMShutdown (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookSingleStep (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookGCStart (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookGCCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) 
static void jvmtiHookCheckForDataBreakpoint (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_JIT_FULL_SPEED_DEBUG */
static void jvmtiHookFramePop (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookFieldModification (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void jvmtiHookDynamicCodeUnload (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* INTERP_NATIVE_SUPPORT */
#if defined(J9VM_INTERP_NATIVE_SUPPORT) 
static void jvmtiHookDynamicCodeLoad (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* INTERP_NATIVE_SUPPORT */
static void jvmtiHookMethodExit (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiFreeClassData (void * userData, void * address);
static void jvmtiHookPopFramesInterrupt (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static IDATA hookReserve (J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData);
static void jvmtiHookUserInterrupt (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookLookupNativeAddress (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static UDATA lookupNativeAddressHelper(J9VMThread *currentThread, J9JVMTIData * jvmtiData, J9Method * nativeMethod, UDATA prefixOffset, UDATA retransformFlag, UDATA functionArgCount, lookupNativeAddressCallback callback);
static void jvmtiHookFindNativeToRegister (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static UDATA removeMethodPrefixesHelper(J9JVMTIData * jvmtiData, U_8 * methodName, UDATA methodPrefixSize, UDATA prefixOffset, UDATA retransformFlag);
static void jvmtiHookMonitorWaited (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookMonitorContendedEnter (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void jvmtiHookCompilingStart (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* INTERP_NATIVE_SUPPORT */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void stopCompileEventThread (J9JVMTIData * jvmtiData);
#endif /* INTERP_NATIVE_SUPPORT */
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void removeUnloadedAgentBreakpoints (J9JVMTIEnv * j9env, J9VMThread * currentThread, J9Class * unloadedClass);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
static IDATA hookUnregister (J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData);
static void jvmtiHookBreakpoint (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookRequiredDebugAttributes (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookJNINativeBind (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static IDATA hookIsDisabled (J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData);
static void jvmtiHookMethodEnter (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static int J9THREAD_PROC compileEventThreadProc (void *entryArg);
#endif /* INTERP_NATIVE_SUPPORT */
#if JAVA_SPEC_VERSION >= 11
static void jvmtiHookSampledObjectAlloc (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* JAVA_SPEC_VERSION >= 11 */
static void jvmtiHookObjectAllocate (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookThreadEnd (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookFindMethodFromPC (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookThreadStarted (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookFieldAccess (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMStartedFirst (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVMStarted (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if JAVA_SPEC_VERSION >= 9
static void jvmtiHookModuleSystemStarted (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* JAVA_SPEC_VERSION >= 9 */
static void jvmtiHookClassFileLoadHook (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookResourceExhausted(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static UDATA findFieldIndexFromOffset(J9VMThread *currentThread, J9Class *clazz, UDATA offset, UDATA isStatic, J9Class **declaringClass);
static jfieldID findWatchedField (J9VMThread *currentThread, J9JVMTIEnv * j9env, UDATA isWrite, UDATA isStatic, UDATA tag, J9Class * fieldClass);
static void jvmtiHookGetEnv (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVmDumpStart (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jvmtiHookVmDumpEnd (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static UDATA methodExists(J9Class * methodClass, U_8 * nameData, UDATA nameLength, J9UTF8 * signature);

static void
jvmtiHookThreadEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadEndEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventThreadEnd callback = j9env->callbacks.ThreadEnd;
	J9VMThread * currentThread = data->currentThread;

	Trc_JVMTI_jvmtiHookThreadEnd_Entry();

	ENSURE_EVENT_PHASE_START_OR_LIVE(jvmtiHookThreadEnd, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_THREAD_END, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef);
			finishedEvent(data->currentThread, JVMTI_EVENT_THREAD_END, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookThreadEnd);
}


static void
jvmtiHookThreadCreated(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadCreatedEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9VMThread * vmThread = data->vmThread;

	Trc_JVMTI_jvmtiHookThreadCreated_Entry();

	if (createThreadData(j9env, vmThread) != JVMTI_ERROR_NONE) {
		/* Struct creation failed, disallow the attaching of this thread */
		data->continueInitialization = FALSE;
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookThreadCreated);
}


static void
jvmtiHookThreadStarted(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadStartedEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventThreadStart callback = j9env->callbacks.ThreadStart;
	J9VMThread * currentThread = data->currentThread;

	Trc_JVMTI_jvmtiHookThreadStarted_Entry();

	ENSURE_EVENT_PHASE_START_OR_LIVE(jvmtiHookThreadStarted, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, data->vmThread, JVMTI_EVENT_THREAD_START, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef);
			finishedEvent(currentThread, JVMTI_EVENT_THREAD_START, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookThreadStarted);
}


static void
jvmtiHookThreadDestroy(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadDestroyEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9JVMTIThreadData * threadData;
	J9VMThread * vmThread = data->vmThread;

	Trc_JVMTI_jvmtiHookThreadDestroy_Entry();

	/* The final thread in the system is destroyed very late, JVMTI could be shut down by then */

	if (vmThread->javaVM->jvmtiData != NULL) {
		/* Deallocate the thread data block for this environment/thread pair, if it exists */

		threadData = THREAD_DATA_FOR_VMTHREAD(j9env, vmThread);
		if (threadData != NULL) {
			omrthread_tls_set(data->vmThread->osThread, j9env->tlsKey, NULL);
			omrthread_monitor_enter(j9env->threadDataPoolMutex);
			pool_removeElement(j9env->threadDataPool, threadData);
			omrthread_monitor_exit(j9env->threadDataPoolMutex);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookThreadDestroy);
}


static void
jvmtiHookVMInitializedFirst(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIData * jvmtiData = userData;

	Trc_JVMTI_jvmtiHookVMInitializedFirst_Entry();

	jvmtiData->phase = JVMTI_PHASE_LIVE;

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMInitializedFirst);
}


static void
jvmtiHookVMInitialized(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInitEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventVMInit callback = j9env->callbacks.VMInit;

	Trc_JVMTI_jvmtiHookVMInitialized_Entry();

	if (callback != NULL) {
		J9VMThread * currentThread = data->vmThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_VM_INIT, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef);
			finishedEvent(currentThread, JVMTI_EVENT_VM_INIT, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMInitialized);
}


static void
jvmtiHookMethodEnter(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMethodEntry callback = j9env->callbacks.MethodEntry;

	Trc_JVMTI_jvmtiHookMethodEnter_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMethodEnter, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread;
		J9Method * method;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (eventNum == J9HOOK_VM_NATIVE_METHOD_ENTER) {
			J9VMNativeMethodEnterEvent * data = eventData;

			currentThread = data->currentThread;
			method = data->method;
		} else {
			J9VMMethodEnterEvent * data = eventData;

			currentThread = data->currentThread;
			method = data->method;
		}

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_METHOD_ENTRY, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jmethodID methodID;

			methodID = getCurrentMethodID(currentThread, method);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (methodID != NULL) {
				if (callback != NULL) {
					callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID);
				}
			}
			finishedEvent(currentThread, JVMTI_EVENT_METHOD_ENTRY, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMethodEnter);
}


static IDATA
hookRegister(J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void *userData)
{
	J9HookInterface** hookInterface = hookInterfaceWithID->hookInterface;

	return (*hookInterface)->J9HookRegisterWithCallSite(hookInterface, J9HOOK_TAG_AGENT_ID | eventNum, function, callsite, userData, hookInterfaceWithID->agentID);
}

/*
 * parameter "callsite" has been added in hookReserve(), hookUnregister() and hookIsDisabled(), but it never been used in those functions
 * it is just for matching hookRegister()(need callsite for hook tracepoint and hook java dump) in order to share code in processEvent().
 */

static IDATA
hookReserve(J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData)
{
	J9HookInterface** hookInterface = hookInterfaceWithID->hookInterface;

	return (*hookInterface)->J9HookReserve(hookInterface, eventNum);
}

static IDATA
hookUnregister(J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData)
{
	J9HookInterface** hookInterface = hookInterfaceWithID->hookInterface;

	(*hookInterface)->J9HookUnregister(hookInterface, J9HOOK_TAG_COUNTED | eventNum, function, userData);
	return 0;
}

static IDATA
processEvent(J9JVMTIEnv* j9env, jint event, J9HookRedirectorFunction redirectorFunction)
{
	J9JVMTIHookInterfaceWithID * vmHook = &j9env->vmHook;
	J9JVMTIHookInterfaceWithID * gcOmrHook = &j9env->gcOmrHook;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	J9JVMTIHookInterfaceWithID * jitHook = &j9env->jitHook;

	if (jitHook->hookInterface == NULL) {
		jitHook = NULL;
	}
#endif

	switch(event) {
		case JVMTI_EVENT_VM_INIT:
			return redirectorFunction(vmHook, J9HOOK_VM_INITIALIZED, jvmtiHookVMInitialized, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_VM_START:
			return redirectorFunction(vmHook, J9HOOK_VM_STARTED, jvmtiHookVMStarted, OMR_GET_CALLSITE(), j9env)
#if JAVA_SPEC_VERSION >= 9
					&& redirectorFunction(vmHook, J9HOOK_JAVA_BASE_LOADED, jvmtiHookModuleSystemStarted, OMR_GET_CALLSITE(), j9env)
# endif /* JAVA_SPEC_VERSION >= 9 */
			;

		case JVMTI_EVENT_VM_DEATH:
			return redirectorFunction(vmHook, J9HOOK_VM_SHUTTING_DOWN, jvmtiHookVMShutdown, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_METHOD_ENTRY:
			return	redirectorFunction(vmHook, J9HOOK_VM_METHOD_ENTER, jvmtiHookMethodEnter, OMR_GET_CALLSITE(), j9env) ||
						redirectorFunction(vmHook, J9HOOK_VM_NATIVE_METHOD_ENTER, jvmtiHookMethodEnter, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_FIELD_ACCESS:
			return	redirectorFunction(vmHook, J9HOOK_VM_GET_FIELD, jvmtiHookFieldAccess, OMR_GET_CALLSITE(), j9env) ||
						redirectorFunction(vmHook, J9HOOK_VM_GET_STATIC_FIELD, jvmtiHookFieldAccess, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_FIELD_MODIFICATION:
			return	redirectorFunction(vmHook, J9HOOK_VM_PUT_FIELD, jvmtiHookFieldModification, OMR_GET_CALLSITE(), j9env) ||
						redirectorFunction(vmHook, J9HOOK_VM_PUT_STATIC_FIELD, jvmtiHookFieldModification, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_THREAD_START:
			return redirectorFunction(vmHook, J9HOOK_VM_THREAD_STARTED, jvmtiHookThreadStarted, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_THREAD_END:
			return redirectorFunction(vmHook, J9HOOK_VM_THREAD_END, jvmtiHookThreadEnd, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_CLASS_FILE_LOAD_HOOK:
			return redirectorFunction(
				vmHook,
				(j9env->flags & J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE) ? J9HOOK_VM_CLASS_LOAD_HOOK2 : J9HOOK_VM_CLASS_LOAD_HOOK,
				jvmtiHookClassFileLoadHook, OMR_GET_CALLSITE(),
				j9env);

		case JVMTI_EVENT_CLASS_LOAD:
			return redirectorFunction(vmHook, J9HOOK_VM_CLASS_LOAD, jvmtiHookClassLoad, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_CLASS_PREPARE:
			return redirectorFunction(vmHook, J9HOOK_VM_CLASS_PREPARE, jvmtiHookClassPrepare, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_SINGLE_STEP:
			return redirectorFunction(vmHook, J9HOOK_VM_SINGLE_STEP, jvmtiHookSingleStep, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_BREAKPOINT:
			return redirectorFunction(vmHook, J9HOOK_VM_BREAKPOINT, jvmtiHookBreakpoint, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_EXCEPTION:
			return redirectorFunction(vmHook, J9HOOK_VM_EXCEPTION_THROW, jvmtiHookExceptionThrow, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_EXCEPTION_CATCH:
			return redirectorFunction(vmHook, J9HOOK_VM_EXCEPTION_CATCH, jvmtiHookExceptionCatch, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_FRAME_POP:
			return redirectorFunction(vmHook, J9HOOK_VM_FRAME_POP, jvmtiHookFramePop, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_VM_OBJECT_ALLOC:
			return redirectorFunction(vmHook, J9HOOK_VM_OBJECT_ALLOCATE, jvmtiHookObjectAllocate, OMR_GET_CALLSITE(), j9env);

#if JAVA_SPEC_VERSION >= 11
		case JVMTI_EVENT_SAMPLED_OBJECT_ALLOC:
		{
			J9JVMTIHookInterfaceWithID * gcHook = &j9env->gcHook;
			return redirectorFunction(gcHook, J9HOOK_MM_OBJECT_ALLOCATION_SAMPLING, jvmtiHookSampledObjectAlloc, OMR_GET_CALLSITE(), j9env);
		}

#endif /* JAVA_SPEC_VERSION >= 11 */
		case JVMTI_EVENT_NATIVE_METHOD_BIND:
			return redirectorFunction(vmHook, J9HOOK_VM_JNI_NATIVE_BIND, jvmtiHookJNINativeBind, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_METHOD_EXIT:
			return	redirectorFunction(vmHook, J9HOOK_VM_METHOD_RETURN, jvmtiHookMethodExit, OMR_GET_CALLSITE(), j9env) ||
						redirectorFunction(vmHook, J9HOOK_VM_NATIVE_METHOD_RETURN, jvmtiHookMethodExit, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_GARBAGE_COLLECTION_START:
			return redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_START, jvmtiHookGCStart, OMR_GET_CALLSITE(), j9env) ||
					redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_START, jvmtiHookGCStart, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_GARBAGE_COLLECTION_FINISH:
		case JVMTI_EVENT_OBJECT_FREE:
			return	redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_END, jvmtiHookGCEnd, OMR_GET_CALLSITE(), j9env) ||
						redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_END, jvmtiHookGCEnd, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_DATA_DUMP_REQUEST:
			return redirectorFunction(vmHook, J9HOOK_VM_USER_INTERRUPT, jvmtiHookUserInterrupt, OMR_GET_CALLSITE(), j9env);

 		case JVMTI_EVENT_MONITOR_CONTENDED_ENTER:
			return redirectorFunction(vmHook, J9HOOK_VM_MONITOR_CONTENDED_ENTER, jvmtiHookMonitorContendedEnter, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_MONITOR_CONTENDED_ENTERED:
			return redirectorFunction(vmHook, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, jvmtiHookMonitorContendedEntered, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_MONITOR_WAIT:
			return redirectorFunction(vmHook, J9HOOK_VM_MONITOR_WAIT, jvmtiHookMonitorWait, OMR_GET_CALLSITE(), j9env);

		case JVMTI_EVENT_MONITOR_WAITED:
			return redirectorFunction(vmHook, J9HOOK_VM_MONITOR_WAITED, jvmtiHookMonitorWaited, OMR_GET_CALLSITE(), j9env);

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
		case JVMTI_EVENT_DYNAMIC_CODE_GENERATED:
		case JVMTI_EVENT_COMPILED_METHOD_LOAD:
			return redirectorFunction(vmHook, J9HOOK_VM_DYNAMIC_CODE_LOAD, jvmtiHookDynamicCodeLoad, OMR_GET_CALLSITE(), J9JVMTI_DATA_FROM_ENV(j9env));

		case JVMTI_EVENT_COMPILED_METHOD_UNLOAD:
			return redirectorFunction(vmHook, J9HOOK_VM_DYNAMIC_CODE_UNLOAD, jvmtiHookDynamicCodeUnload, OMR_GET_CALLSITE(), J9JVMTI_DATA_FROM_ENV(j9env));
#endif

		case JVMTI_EVENT_RESOURCE_EXHAUSTED:
			return redirectorFunction(vmHook, J9HOOK_VM_RESOURCE_EXHAUSTED, jvmtiHookResourceExhausted, OMR_GET_CALLSITE(), j9env);

		/* Extension Events */

		case J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC:
			return redirectorFunction(vmHook, J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE, jvmtiHookObjectAllocate, OMR_GET_CALLSITE(), j9env);

		case J9JVMTI_EVENT_COM_IBM_VM_DUMP_START:
			return redirectorFunction(vmHook, J9HOOK_VM_DUMP_START, jvmtiHookVmDumpStart, OMR_GET_CALLSITE(), j9env);

		case J9JVMTI_EVENT_COM_IBM_VM_DUMP_END:
			return redirectorFunction(vmHook, J9HOOK_VM_DUMP_END, jvmtiHookVmDumpEnd, OMR_GET_CALLSITE(), j9env);

		case J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START:
			return redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_GC_CYCLE_START, jvmtiHookGCCycleStart, OMR_GET_CALLSITE(), j9env);

		case J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH:
			return redirectorFunction(gcOmrHook, J9HOOK_MM_OMR_GC_CYCLE_END, jvmtiHookGCCycleEnd, OMR_GET_CALLSITE(), j9env);

		case J9JVMTI_EVENT_COM_IBM_COMPILING_START:
			if (jitHook != NULL) {
				return redirectorFunction(jitHook, J9HOOK_JIT_COMPILING_START, jvmtiHookCompilingStart, OMR_GET_CALLSITE(), j9env);
			} else {
				return 0;
			}

		case J9JVMTI_EVENT_COM_IBM_COMPILING_END:
			if (jitHook != NULL) {
				return redirectorFunction(jitHook, J9HOOK_JIT_COMPILING_END, jvmtiHookCompilingEnd, OMR_GET_CALLSITE(), j9env);
			} else {
				return 0;
			}
	}

	return 0;
}


IDATA
hookEvent(J9JVMTIEnv * j9env, jint event)
{
	return processEvent(j9env, event, hookRegister);
}


IDATA
unhookEvent(J9JVMTIEnv * j9env, jint event)
{
	return processEvent(j9env, event, hookUnregister);
}


IDATA
reserveEvent(J9JVMTIEnv * j9env, jint event)
{
	return processEvent(j9env, event, hookReserve);
}


static void
jvmtiHookVMStartedFirst(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInitEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;

	Trc_JVMTI_jvmtiHookVMStarted_Entry();

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	/* Allocate the object for the event reporting thread, if one exists */

	if (jvmtiData->compileEventThread != NULL) {
		J9VMThread * currentThread = data->vmThread;
		J9JavaVM * vm = currentThread->javaVM;
		J9VMThread * compileEventVMThread = jvmtiData->compileEventVMThread;

		vm->internalVMFunctions->initializeAttachedThread
			(currentThread, "JVMTI event reporting thread",
			vm->systemThreadGroupRef,
			((compileEventVMThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0),
			compileEventVMThread);

		if ((currentThread->currentException == NULL) && (currentThread->threadObject != NULL)) {
			TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, currentThread, compileEventVMThread);
		} else {
			(*((JNIEnv*) currentThread))->ExceptionClear((JNIEnv*) currentThread);
		}
	}
#endif

	jvmtiData->phase = JVMTI_PHASE_START;

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMStartedFirst);
}

static void
jvmtiHookVMStarted(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInitEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventVMStart callback = j9env->callbacks.VMStart;

	Trc_JVMTI_jvmtiHookVMStarted_Entry();

	if (callback != NULL) {
		J9VMThread *currentThread = data->vmThread;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;
		BOOLEAN reportEvent = TRUE;

#if JAVA_SPEC_VERSION >= 9
		J9JavaVM *vm = currentThread->javaVM;
	 	if (J2SE_VERSION(vm) >= J2SE_V11) {
	 		if (j9env->capabilities.can_generate_early_vmstart == 0) {
	 			reportEvent = FALSE;
	 		}
	 	}
#endif /* JAVA_SPEC_VERSION >= 9 */
	 	if (TRUE == reportEvent) {
	 		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_VM_START, NULL, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
	 			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread);
	 			finishedEvent(currentThread, JVMTI_EVENT_VM_START, hadVMAccess, javaOffloadOldState);
	 		}
	 	}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMStarted);
}

#if JAVA_SPEC_VERSION >= 9
static void
jvmtiHookModuleSystemStarted(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData) {
	J9VMModuleStartEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventVMStart callback = j9env->callbacks.VMStart;
	J9VMThread *currentThread = data->vmThread;
	J9JavaVM *vm = currentThread->javaVM;

	Trc_JVMTI_jvmtiHookModuleSystemStarted_Entry();

	Assert_JVMTI_true(J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED));
	Assert_JVMTI_true(J2SE_VERSION(vm) >= J2SE_V11);

	/*
	 * In Java9 the VMStart event can be triggered from either the J9HOOK_VM_STARTED
	 * hook or J9HOOK_JAVA_BASE_LOADED. If the can_generate_early_vmstart capability is set
	 * we can trigger this event as early as possible in the jvmtiHookVMStarted handler.
	 * Otherwise we have to wait until we have loaded java.base and trigger the VMStart event
	 * here (jvmtiHookModuleSystemStarted handler).
	 */

	if (callback != NULL) {
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (j9env->capabilities.can_generate_early_vmstart == 0) {
			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_VM_START, NULL, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread);
				finishedEvent(currentThread, JVMTI_EVENT_VM_START, hadVMAccess, javaOffloadOldState);
			}
		}
	}
	TRACE_JVMTI_EVENT_RETURN(jvmtiHookModuleSystemStarted);
}
#endif /* JAVA_SPEC_VERSION >= 9 */


static void
jvmtiHookClassLoad(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassLoadEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;

	Trc_JVMTI_jvmtiHookClassLoad_Entry();

	ENSURE_EVENT_PHASE_START_OR_LIVE(jvmtiHookClassLoad, j9env);

	/* Do not report the event for arrays or primitive types */

	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(data->clazz->romClass) == 0) {
		jvmtiEventClassLoad callback = j9env->callbacks.ClassLoad;

		/* Call the event callback */

		if (callback != NULL) {
			J9VMThread * currentThread = data->currentThread;
			jthread threadRef;
			UDATA hadVMAccess;
			UDATA javaOffloadOldState = 0;

			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_CLASS_LOAD, &threadRef, &hadVMAccess, TRUE, 1, &javaOffloadOldState)) {
				j9object_t * classRef = (j9object_t*) currentThread->arg0EA;

				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(data->clazz);
				currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, (jclass) classRef);
				finishedEvent(currentThread, JVMTI_EVENT_CLASS_LOAD, hadVMAccess, javaOffloadOldState);
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookClassLoad);
}


UDATA
isEventHookable(J9JVMTIEnv * j9env, jvmtiEvent event)
{
	return processEvent(j9env, event, hookIsDisabled) == 0;
}


static IDATA
hookIsDisabled(J9JVMTIHookInterfaceWithID* hookInterfaceWithID, UDATA eventNum, J9HookFunction function, const char *callsite, void* userData)
{
	J9HookInterface** hookInterface = hookInterfaceWithID->hookInterface;

	return (*hookInterface)->J9HookIsEnabled(hookInterface, eventNum) == 0;
}

void
unhookAllEvents(J9JVMTIEnv * j9env)
{
	J9JVMTIHookInterfaceWithID * vmHook = &j9env->vmHook;
	J9JVMTIHookInterfaceWithID * gcOmrHook = &j9env->gcOmrHook;
	jvmtiEvent i;

	for (i = J9JVMTI_LOWEST_EVENT; i <= J9JVMTI_HIGHEST_EVENT; ++i) {
		unhookEvent(j9env, i);
	}

	hookUnregister(vmHook, J9HOOK_VM_THREAD_CREATED, jvmtiHookThreadCreated, NULL, j9env);
	hookUnregister(vmHook, J9HOOK_VM_THREAD_DESTROY, jvmtiHookThreadDestroy, NULL, j9env);
	hookUnregister(vmHook, J9HOOK_VM_POP_FRAMES_INTERRUPT, jvmtiHookPopFramesInterrupt, NULL, j9env);

	hookUnregister(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_END, jvmtiHookGCEnd, NULL, j9env);
	hookUnregister(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_END, jvmtiHookGCEnd, NULL, j9env);
}


IDATA
hookRequiredEvents(J9JVMTIEnv * j9env)
{
	J9JVMTIHookInterfaceWithID * vmHook = &j9env->vmHook;

	if (hookRegister(vmHook, J9HOOK_VM_THREAD_CREATED, jvmtiHookThreadCreated, OMR_GET_CALLSITE(), j9env)) {
		return 1;
	}

	if (hookRegister(vmHook, J9HOOK_VM_THREAD_DESTROY, jvmtiHookThreadDestroy, OMR_GET_CALLSITE(), j9env)) {
		return 1;
	}

	return 0;
}


IDATA
hookNonEventCapabilities(J9JVMTIEnv * j9env, jvmtiCapabilities * capabilities)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(j9env);
	J9JVMTIHookInterfaceWithID * vmHook = &j9env->vmHook;
	J9JVMTIHookInterfaceWithID * gcOmrHook = &j9env->gcOmrHook;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);

	if (capabilities->can_generate_breakpoint_events) {
		if (hookRegister(vmHook, J9HOOK_VM_BREAKPOINT, jvmtiHookBreakpoint, OMR_GET_CALLSITE(), j9env)) {
			return 1;
		}
	}

	if (capabilities->can_get_line_numbers) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_LINE_NUMBER_TABLE)) {
			return 1;
		}
	}

	if (capabilities->can_get_source_file_name) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_SOURCE_FILE)) {
			return 1;
		}
	}

	if (capabilities->can_access_local_variables) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_LOCAL_VARIABLE_TABLE | J9VM_DEBUG_ATTRIBUTE_CAN_ACCESS_LOCALS)) {
			return 1;
		}
		installDebugLocalMapper(vm);
	}

#ifdef J9VM_OPT_DEBUG_JSR45_SUPPORT
	if (capabilities->can_get_source_debug_extension) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_SOURCE_DEBUG_EXTENSION)) {
			return 1;
		}
	}
#endif

	if (capabilities->can_redefine_classes) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_CAN_REDEFINE_CLASSES)) {
			return 1;
		}
	}

	if (capabilities->can_pop_frame) {
		if (hookRegister(vmHook, J9HOOK_VM_POP_FRAMES_INTERRUPT, jvmtiHookPopFramesInterrupt, OMR_GET_CALLSITE(), jvmtiData)) {
			return 1;
		}
	}

	if (capabilities->can_force_early_return) {
		if (hookRegister(vmHook, J9HOOK_VM_POP_FRAMES_INTERRUPT, jvmtiHookPopFramesInterrupt, OMR_GET_CALLSITE(), jvmtiData)) {
			return 1;
		}
	}

	if (capabilities->can_tag_objects) {
		if (hookRegister(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_END, jvmtiHookGCEnd, OMR_GET_CALLSITE(), j9env)) {
			return 1;
		}
		if (hookRegister(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_END, jvmtiHookGCEnd, OMR_GET_CALLSITE(), j9env)) {
			return 1;
		}
	}

	if (capabilities->can_retransform_classes) {
		if (enableDebugAttribute(j9env, J9VM_DEBUG_ATTRIBUTE_ALLOW_RETRANSFORM | J9VM_DEBUG_ATTRIBUTE_CAN_REDEFINE_CLASSES)) {
			return 1;
		}
		j9env->flags |= J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE;
	}

	if (capabilities->can_generate_compiled_method_load_events) {
		if (JVMTI_ERROR_NONE != startCompileEventThread(jvmtiData)) {
			return 1;
		}
	}

	return 0;
}


static void
jvmtiHookClassPrepare(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassPrepareEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventClassPrepare callback = j9env->callbacks.ClassPrepare;

	Trc_JVMTI_jvmtiHookClassPrepare_Entry(
		data->clazz, 
		(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(data->clazz->romClass)),
		J9UTF8_DATA(J9ROMCLASS_CLASSNAME(data->clazz->romClass)));

	ENSURE_EVENT_PHASE_START_OR_LIVE(jvmtiHookClassPrepare, j9env);

	/* Do not report the event for arrays or primitive types - currently the VM does not send them, so no code needed here */

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_CLASS_PREPARE, &threadRef, &hadVMAccess, TRUE, 1, &javaOffloadOldState)) {
			j9object_t * classRef = (j9object_t*) currentThread->arg0EA;

			*classRef = J9VM_J9CLASS_TO_HEAPCLASS(data->clazz);
			currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, (jclass) classRef);
			finishedEvent(currentThread, JVMTI_EVENT_CLASS_PREPARE, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookClassPrepare);
}


static void
jvmtiHookSingleStep(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMSingleStepEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventSingleStep callback = j9env->callbacks.SingleStep;

	Trc_JVMTI_jvmtiHookSingleStep_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookSingleStep, j9env);

	/* Call the event callback */
	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_SINGLE_STEP, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jmethodID methodID;

			methodID = getCurrentMethodID(currentThread, data->method);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (methodID != NULL) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jlocation) data->location);
			}
			finishedEvent(currentThread, JVMTI_EVENT_SINGLE_STEP, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookSingleStep);
}


static void
jvmtiHookFieldAccess(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventFieldAccess callback = j9env->callbacks.FieldAccess;

	Trc_JVMTI_jvmtiHookFieldAccess_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookFieldAccess, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		UDATA tag;
		jfieldID fieldID;
		j9object_t object;
		J9Method * method;
		IDATA location;
		J9VMThread * currentThread;
		J9Class * clazz;

		if (eventNum == J9HOOK_VM_GET_FIELD) {
			J9VMGetFieldEvent * data = eventData;

			object = data->object;
			clazz = J9OBJECT_CLAZZ(data->currentThread, object);
			tag = data->offset;
			method = data->method;
			location = data->location;
			currentThread = data->currentThread;
		} else {
			J9VMGetStaticFieldEvent * data = eventData;

			object = NULL;
			clazz = data->declaringClass;
			tag = (UDATA) data->fieldAddress;
			method = data->method;
			location = data->location;
			currentThread = data->currentThread;
		}

		/* Current thread has VM access, and the field watch list is only modified under exclusive */

		fieldID = findWatchedField(currentThread, j9env, FALSE, J9HOOK_VM_GET_FIELD != eventNum, tag, clazz);

		/* If the current field is not being watched, do nothing */

		if (fieldID != NULL) {
			jthread threadRef;
			UDATA hadVMAccess;
			UDATA javaOffloadOldState = 0;

			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_FIELD_ACCESS, &threadRef, &hadVMAccess, TRUE, (object == NULL) ? 1 : 2, &javaOffloadOldState)) {
				J9JavaVM * vm = currentThread->javaVM;
				jmethodID methodID;
				j9object_t * classRef = (j9object_t*) currentThread->arg0EA;
				j9object_t * objectRef;
				J9Class* clazz;

				if (object == NULL) {
					objectRef = NULL;
				} else {
					objectRef = (j9object_t*) (currentThread->arg0EA - 1);
					*objectRef = object;
				}
				clazz = getCurrentClass(((J9JNIFieldID *) fieldID)->declaringClass);
				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				methodID = getCurrentMethodID(currentThread, method);
				vm->internalVMFunctions->internalExitVMToJNI(currentThread);
				if (methodID != NULL) {
					callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jlocation) location, (jclass) classRef, (jobject) objectRef, fieldID);
				}
				vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
				if (NULL != object) {
					J9VMGetFieldEvent* data = eventData;
					data->object = *objectRef;
				}
				finishedEvent(currentThread, JVMTI_EVENT_FIELD_ACCESS, hadVMAccess, javaOffloadOldState);
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookFieldAccess);
}


static void
jvmtiHookFieldModification(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventFieldModification callback = j9env->callbacks.FieldModification;

	Trc_JVMTI_jvmtiHookFieldModification_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookFieldModification, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		UDATA tag;
		jfieldID fieldID;
		j9object_t object;
		J9Method * method;
		IDATA location;
		J9VMThread * currentThread;
		void * valueAddress;
		J9Class * clazz;

		if (eventNum == J9HOOK_VM_PUT_FIELD) {
			J9VMPutFieldEvent * data = eventData;

			object = data->object;
			clazz = J9OBJECT_CLAZZ(data->currentThread, object);
			tag = data->offset;
			valueAddress = &(data->newValue);
			currentThread = data->currentThread;
			method = data->method;
			location = data->location;
		} else {
			J9VMPutStaticFieldEvent * data = eventData;

			object = NULL;
			clazz = data->declaringClass;
			tag = (UDATA) data->fieldAddress;
			valueAddress = &(data->newValue);
			currentThread = data->currentThread;
			method = data->method;
			location = data->location;
		}

		/* Current thread has VM access, and the field watch list is only modified under exclusive */

		fieldID = findWatchedField(currentThread, j9env, TRUE, J9HOOK_VM_PUT_FIELD != eventNum, tag, clazz);

		/* If the current field is not being watched, do nothing */

		if (fieldID != NULL) {
			jthread threadRef;
			UDATA hadVMAccess;
			char signatureType = J9UTF8_DATA(J9ROMFIELDSHAPE_SIGNATURE(((J9JNIFieldID *) fieldID)->field))[0];
			UDATA refCount = 1;
			UDATA javaOffloadOldState = 0;

			if (signatureType == '[') {
				signatureType = 'L';
			}
			if (signatureType == 'L') {
				if (*((void **) valueAddress) != NULL) {
					++refCount;
				}
			}
			if (object != NULL) {
				++refCount;
			}
			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_FIELD_MODIFICATION, &threadRef, &hadVMAccess, TRUE, refCount, &javaOffloadOldState)) {
				J9JavaVM * vm = currentThread->javaVM;
				jmethodID methodID;
				j9object_t * classRef = (j9object_t*) currentThread->arg0EA;
				J9Class* clazz;
				j9object_t * objectRef;
				jvalue newValue;
				j9object_t * jvalueStorage = (j9object_t*) currentThread->arg0EA - 1;

				if (object == NULL) {
					objectRef = NULL;
				} else {
					objectRef = jvalueStorage--;
					*objectRef = object;
				}
				fillInJValue(signatureType, &newValue, valueAddress, jvalueStorage);
				clazz = getCurrentClass(((J9JNIFieldID *) fieldID)->declaringClass);
				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				methodID = getCurrentMethodID(currentThread, method);
				vm->internalVMFunctions->internalExitVMToJNI(currentThread);
				if (methodID != NULL) {
					callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jlocation) location, (jclass) classRef, (jobject) objectRef, fieldID, signatureType, newValue);
				}
				vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
				if (NULL != object) {
					J9VMPutFieldEvent* data = eventData;
					data->object = *objectRef;
				}
				if (signatureType == 'L') {
					if (*((void **) valueAddress) != NULL) {
						*(j9object_t*)valueAddress = *jvalueStorage;
					}
				}
				finishedEvent(currentThread, JVMTI_EVENT_FIELD_MODIFICATION, hadVMAccess, javaOffloadOldState);
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookFieldModification);
}


static void
jvmtiHookRequiredDebugAttributes(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMRequiredDebugAttributesEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;

	Trc_JVMTI_jvmtiHookRequiredDebugAttributes_Entry();

	data->requiredDebugAttributes |= jvmtiData->requiredDebugAttributes;

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookRequiredDebugAttributes);
}

static UDATA
findFieldIndexFromOffset(J9VMThread *currentThread, J9Class *clazz, UDATA offset, UDATA isStatic, J9Class **declaringClass)
{
	UDATA index = 0;
	J9JavaVM * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	U_32 const walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
	U_32 staticBit = 0;
	if (isStatic) {
		staticBit = J9AccStatic;
		offset -= (UDATA)clazz->ramStatics;
	}
	for(;;) {
		J9ROMClass * const romClass = clazz->romClass;
		J9Class * const superclazz = GET_SUPERCLASS(clazz);
		J9ROMFieldOffsetWalkState state;
		J9ROMFieldOffsetWalkResult *result = NULL;

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		result = vmFuncs->fieldOffsetsStartDo(vm, romClass, superclazz, &state, walkFlags, clazz->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		result = vmFuncs->fieldOffsetsStartDo(vm, romClass, superclazz, &state, walkFlags);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

		while (NULL != result->field) {
			if (staticBit == (result->field->modifiers & J9AccStatic)) {
				if (offset == result->offset) {
					if (NULL != declaringClass) {
						*declaringClass = clazz;
					}
					goto done;
				}
			}
			index += 1;
			result = vmFuncs->fieldOffsetsNextDo(&state);
		}
		/* Static fields must be found in the input class */
		Assert_JVMTI_false(isStatic);
		/* Instance fields may come from any superclass */
		clazz = superclazz;
		Assert_JVMTI_notNull(clazz);
		/* Start the index counting again for each superclass */
		index = 0;
	}
done:
	return index;
}

static jfieldID
findWatchedField(J9VMThread *currentThread, J9JVMTIEnv * j9env, UDATA isWrite, UDATA isStatic, UDATA tag, J9Class * fieldClass)
{
	jfieldID result = NULL;
	if (J9_ARE_ANY_BITS_SET(fieldClass->classFlags, J9ClassHasWatchedFields)) {
		J9Class *declaringClass = NULL;
		J9JVMTIWatchedClass *watchedClass = NULL;
		UDATA index = findFieldIndexFromOffset(currentThread, fieldClass, tag, isStatic, &declaringClass);
		watchedClass = hashTableFind(j9env->watchedClasses, &declaringClass);
		if (NULL != watchedClass) {
			UDATA *watchBits = (UDATA*)&watchedClass->watchBits;
			UDATA found = FALSE;
			if (J9JVMTI_CLASS_REQUIRES_ALLOCATED_J9JVMTI_WATCHED_FIELD_ACCESS_BITS(declaringClass)) {
				watchBits = watchedClass->watchBits;
			}
			if (isWrite) {
				found = watchBits[J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(index)] & J9JVMTI_WATCHED_FIELD_MODIFICATION_BIT(index);
			} else {
				found = watchBits[J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(index)] & J9JVMTI_WATCHED_FIELD_ACCESS_BIT(index);			
			}
			if (found) {
				/* In order for a watch to have been placed, the fieldID for the field in question
				 * must already have been created (it's a parameter to the JVMTI calls).
				 */
				void **jniIDs = declaringClass->jniIDs;
				Assert_JVMTI_notNull(jniIDs);
				result = (jfieldID)(jniIDs[index + declaringClass->romClass->romMethodCount]);
				Assert_JVMTI_notNull(result);
			}
		}
	}
	return result;
}

static void
jvmtiHookVMShutdownLast(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIData * jvmtiData = userData;

	Trc_JVMTI_jvmtiHookVMShutdownLast_Entry();

	jvmtiData->phase = JVMTI_PHASE_DEAD;

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	/* Stop the compile event reporting thread if necessary */

	stopCompileEventThread(jvmtiData);
#endif

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMShutdownLast);
}


static void
jvmtiHookVMShutdown(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMShutdownEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventVMDeath callback = j9env->callbacks.VMDeath;

	Trc_JVMTI_jvmtiHookVMShutdown_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookVMShutdown, j9env);

	if (callback != NULL) {
		J9VMThread * currentThread = data->vmThread;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_VM_DEATH, NULL, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread);
			finishedEvent(currentThread, JVMTI_EVENT_VM_DEATH, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVMShutdown);
}


static void
jvmtiHookBreakpoint(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMBreakpointEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventBreakpoint callback = j9env->callbacks.Breakpoint;
	J9Method * method = data->method;
	J9JVMTIBreakpointedMethod * breakpointedMethod;
	IDATA location = data->location;

	Trc_JVMTI_jvmtiHookBreakpoint_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookBreakpoint, j9env);

	/* See if JVMTI has a breakpoint in this method */

	breakpointedMethod = findBreakpointedMethod(J9JVMTI_DATA_FROM_ENV(j9env), method);

	/* Check if breakpointedMethod is null */
	Assert_JVMTI_notNull(breakpointedMethod);

	/* Look up the old bytecode in the breakpointed method */

	data->originalBytecode = J9_BYTECODE_START_FROM_ROM_METHOD(breakpointedMethod->originalROMMethod)[location];

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		J9JVMTIAgentBreakpoint * agentBreakpoint;
		UDATA javaOffloadOldState = 0;

		/* See if this agent has a breakpoint at the current location - assume the current thread has VM access at this point */

		if ((agentBreakpoint = findAgentBreakpoint(currentThread, j9env, method, location)) != NULL) {
			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_BREAKPOINT, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
				jmethodID methodID = agentBreakpoint->method;

				currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jlocation) location);
				finishedEvent(currentThread, JVMTI_EVENT_BREAKPOINT, hadVMAccess, javaOffloadOldState);
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookBreakpoint);
}


static void
jvmtiHookExceptionThrow(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMExceptionThrowEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventException callback = j9env->callbacks.Exception;

	Trc_JVMTI_jvmtiHookExceptionThrow_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookExceptionThrow, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		j9object_t exception = data->exception;
		J9VMThread * currentThread = data->currentThread;
		J9JavaVM * vm = currentThread->javaVM;
		jthread threadRef;
		UDATA hadVMAccess;
		J9StackWalkState walkState;
		J9Method * throwMethod;
		IDATA throwLocation;
		J9Method * catchMethod;
		IDATA catchLocation;
		UDATA javaOffloadOldState = 0;

		/* Find the thrower */

		walkState.walkThread = currentThread;
		walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
		walkState.skipCount = 0;
		walkState.maxFrames = 1;
		vm->walkStackFrames(currentThread, &walkState);
		throwMethod = walkState.method;
		throwLocation = walkState.bytecodePCOffset;

/* JVMTI JDK compatibility */
		if (throwLocation == -1) {
			throwLocation = 0;
		}
/* JVMTI JDK compatibility */

		/* Find the catcher, if any */

		exception = (j9object_t) vm->internalVMFunctions->walkStackForExceptionThrow(currentThread, exception, TRUE);
		switch((UDATA) currentThread->stackWalkState->userData3) {
			case J9_EXCEPT_SEARCH_JIT_HANDLER:
				catchMethod = vm->jitConfig->jitGetExceptionCatcher(currentThread, currentThread->stackWalkState->userData2, currentThread->stackWalkState->jitInfo, &catchLocation);
				break;

			case J9_EXCEPT_SEARCH_JAVA_HANDLER:
				catchMethod =  currentThread->stackWalkState->method;
				catchLocation = (IDATA) currentThread->stackWalkState->userData1;
				break;

			default:
				catchMethod = NULL;
				catchLocation = 0;
				break;
		}

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_EXCEPTION, &threadRef, &hadVMAccess, TRUE, 1, &javaOffloadOldState)) {
			jmethodID throwMethodID;
			jmethodID catchMethodID;
			j9object_t * exceptionRef = (j9object_t*) currentThread->arg0EA;

			*exceptionRef = exception;
			throwMethodID = getCurrentMethodID(currentThread, throwMethod);
			if (catchMethod == NULL) {
				catchMethodID = NULL;
			} else {
				catchMethodID = getCurrentMethodID(currentThread, catchMethod);
				if (catchMethodID == NULL) {
					throwMethodID = NULL;
				}
			}
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (throwMethodID != NULL) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef,
					throwMethodID, (jlocation) throwLocation, (jobject) exceptionRef,
					catchMethodID, (jlocation) catchLocation);
			}
			currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
			exception = J9_JNI_UNWRAP_REDIRECTED_REFERENCE(exceptionRef);
			finishedEvent(currentThread, JVMTI_EVENT_EXCEPTION, hadVMAccess, javaOffloadOldState);
		}

		data->exception = exception;
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookExceptionThrow);
}


static void
jvmtiHookMethodExit(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMethodExit callback = j9env->callbacks.MethodExit;

	Trc_JVMTI_jvmtiHookMethodExit_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMethodExit, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread;
		J9Method * method;
		UDATA poppedByException;
		void * valueAddress;
		jthread threadRef;
		UDATA hadVMAccess;
		char signatureType = '\0';
		UDATA refCount = 0;
		UDATA javaOffloadOldState = 0;

		if (eventNum == J9HOOK_VM_NATIVE_METHOD_RETURN) {
			J9VMNativeMethodReturnEvent * data = eventData;

			currentThread = data->currentThread;
			method = data->method;
			poppedByException = data->poppedByException;
			valueAddress = data->returnValuePtr;
		} else {
			J9VMMethodReturnEvent * data = eventData;

			currentThread = data->currentThread;
			method = data->method;
			poppedByException = data->poppedByException;
			valueAddress = data->returnValuePtr;
		}

		if (!poppedByException) {
			J9UTF8 * signature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass,J9_ROM_METHOD_FROM_RAM_METHOD(method));

			if ((J9UTF8_DATA(signature)[J9UTF8_LENGTH(signature) - 2] == '[') || ((signatureType = J9UTF8_DATA(signature)[J9UTF8_LENGTH(signature) - 1]) == ';')) {
				signatureType = 'L';
				if (*((void **) valueAddress) != NULL) {
					++refCount;
				}
			}
		}

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_METHOD_EXIT, &threadRef, &hadVMAccess, TRUE, refCount, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jmethodID methodID;
			jvalue returnValue;

			if (!poppedByException) {
				fillInJValue(signatureType, &returnValue, valueAddress,  (j9object_t*) currentThread->arg0EA);
			}
			methodID = getCurrentMethodID(currentThread, method);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (methodID != NULL) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jboolean) poppedByException, returnValue);
			}
			finishedEvent(currentThread, JVMTI_EVENT_METHOD_EXIT, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMethodExit);
}


static void
jvmtiHookUserInterrupt(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMUserInterruptEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventDataDumpRequest callback = j9env->callbacks.DataDumpRequest;

	Trc_JVMTI_jvmtiHookUserInterrupt_Entry();

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread;
		J9JavaVM* vm = data->vm;
		UDATA hadVMAccess;
		jint rc;
        J9JavaVMAttachArgs thr_args;
        UDATA javaOffloadOldState = 0;

        thr_args.version = JNI_VERSION_1_2;
        thr_args.name = "JVMTI data dump request";
        thr_args.group = NULL;

		/* Temporarily attach external thread */
		rc = (jint) vm->internalVMFunctions->AttachCurrentThreadAsDaemon((JavaVM *)vm, (void **)&currentThread, (void *)&thr_args);
		if (rc == JNI_OK) {

			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_DATA_DUMP_REQUEST, NULL, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
				callback((jvmtiEnv *) j9env);
				finishedEvent(currentThread, JVMTI_EVENT_DATA_DUMP_REQUEST, hadVMAccess, javaOffloadOldState);
			}

			/* Detach temporary vm thread */
			(*((JavaVM *)vm))->DetachCurrentThread((JavaVM *)vm);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookUserInterrupt);
}


static void
jvmtiHookPermanentPC(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMPermanentPCEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	pool_state poolState;
	J9JVMTIBreakpointedMethod * breakpointedMethod;

	Trc_JVMTI_jvmtiHookPermanentPC_Entry();

	/* Assuming that this event is called with VM access, meaning the breakpoint list will not change */

	breakpointedMethod = pool_startDo(jvmtiData->breakpointedMethods, &poolState);
	while (breakpointedMethod != NULL) {
		J9ROMMethod * copiedROMMethod = breakpointedMethod->copiedROMMethod;

		if ((data->pc >= (U_8 *) copiedROMMethod) && (data->pc < J9_BYTECODE_END_FROM_ROM_METHOD(copiedROMMethod))) {
			data->pc = data->pc - (U_8 *) copiedROMMethod + (U_8 *) breakpointedMethod->originalROMMethod;
			break;
		}
		breakpointedMethod = pool_nextDo(&poolState);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookPermanentPC);
}


static void
jvmtiHookFindMethodFromPC(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMFindMethodFromPCEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	pool_state poolState;
	J9JVMTIBreakpointedMethod * breakpointedMethod;
	U_8* pc = data->pc;

	Trc_JVMTI_jvmtiHookFindMethodFromPC_Entry();

	/* Assuming that this event is called with VM access, meaning the breakpoint list will not change */

	breakpointedMethod = pool_startDo(jvmtiData->breakpointedMethods, &poolState);
	while (breakpointedMethod != NULL) {
		J9ROMMethod * copiedROMMethod = breakpointedMethod->copiedROMMethod;

		if ((pc >= (U_8 *) copiedROMMethod) && (pc < J9_BYTECODE_END_FROM_ROM_METHOD(copiedROMMethod))) {
			data->result = breakpointedMethod->method;
			break;
		}
		breakpointedMethod = pool_nextDo(&poolState);
	}

	if (breakpointedMethod == NULL) {
		/* scan the ROM methods of the class (to handle the case of PCs in stack traces which came from breakpointed methods
		 *	-- the default handler won't have found these, since it walked the RAM methods) 
		 */
		J9ROMClass* romClass = data->clazz->romClass;
		J9ROMMethod* romMethod = J9ROMCLASS_ROMMETHODS(romClass);
		U_32 i;

		for (i = 0; i < romClass->romMethodCount; i++) {
			if ((pc >= (U_8 *) romMethod) && (pc < J9_BYTECODE_END_FROM_ROM_METHOD(romMethod))) {
				data->result = &(data->clazz->ramMethods[i]);
				break;
			}
			romMethod = J9_NEXT_ROM_METHOD(romMethod);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookFindMethodFromPC);
}


static void
jvmtiHookGetEnv(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMGetEnvEvent * data = eventData;

	Trc_JVMTI_jvmtiHookGetEnv_Entry();

	if (data->rc == JNI_EVERSION) {
		jint version = data->version & ~JVMTI_VERSION_MASK_MICRO;

		if ((version == JVMTI_VERSION_1_0) 
			|| (version == JVMTI_VERSION_1_1) 
			|| (version == JVMTI_VERSION_1_2) 
			|| (version == JVMTI_VERSION_9)
			|| (version == JVMTI_VERSION_11)
		) {
			/* Jazz 99339: obtain the pointer to J9JavaVM from J9InvocationJavaVM so as to store J9NativeLibrary in J9JVMTIEnv */
			J9InvocationJavaVM * invocationJavaVM = (J9InvocationJavaVM *)data->jvm;
			J9JVMTIData * jvmtiData = userData;

			if (jvmtiData != NULL) {
				if (jvmtiData->phase != JVMTI_PHASE_DEAD) {
					data->rc = allocateEnvironment(invocationJavaVM, data->version, data->penv);
				}
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookGetEnv);
}


IDATA
hookGlobalEvents(J9JVMTIData * jvmtiData)
{
	J9JavaVM * vm = jvmtiData->vm;
	J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	J9HookInterface ** jitHook = vm->internalVMFunctions->getJITHookInterface(vm);
#endif

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (jitHook != NULL) {
		if ((*jitHook)->J9HookRegisterWithCallSite(jitHook, J9HOOK_JIT_CHECK_FOR_DATA_BREAKPOINT, jvmtiHookCheckForDataBreakpoint, OMR_GET_CALLSITE(), jvmtiData)) {
			return 1;
		}
	}
#endif

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_CLASS_UNLOAD, jvmtiHookClassUnload, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}
#endif

	if ((*vmHook)->J9HookReserve(vmHook, J9HOOK_VM_CLASS_LOAD_HOOK)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_GETENV, jvmtiHookGetEnv, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_PERMANENT_PC, jvmtiHookPermanentPC, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_FIND_METHOD_FROM_PC, jvmtiHookFindMethodFromPC, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_LOOKUP_NATIVE_ADDRESS, jvmtiHookLookupNativeAddress, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_FIND_NATIVE_TO_REGISTER, jvmtiHookFindNativeToRegister, OMR_GET_CALLSITE(), jvmtiData)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_TAG_AGENT_ID | J9HOOK_VM_INITIALIZED, jvmtiHookVMInitializedFirst, OMR_GET_CALLSITE(), jvmtiData, J9HOOK_AGENTID_FIRST)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_TAG_AGENT_ID | J9HOOK_VM_STARTED, jvmtiHookVMStartedFirst, OMR_GET_CALLSITE(), jvmtiData, J9HOOK_AGENTID_FIRST)) {
		return 1;
	}

	if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_TAG_AGENT_ID | J9HOOK_VM_SHUTTING_DOWN, jvmtiHookVMShutdownLast, OMR_GET_CALLSITE(), jvmtiData, J9HOOK_AGENTID_LAST)) {
		return 1;
	}

	return 0;
}


void
unhookGlobalEvents(J9JVMTIData * jvmtiData)
{
	J9JavaVM * vm = jvmtiData->vm;
	J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	J9HookInterface ** jitHook = vm->internalVMFunctions->getJITHookInterface(vm);
#endif

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (jitHook != NULL) {
		(*jitHook)->J9HookUnregister(jitHook, J9HOOK_JIT_CHECK_FOR_DATA_BREAKPOINT, jvmtiHookCheckForDataBreakpoint, NULL);
	}
#endif

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_CLASS_UNLOAD, jvmtiHookClassUnload, NULL);
#endif

	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_GETENV, jvmtiHookGetEnv, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_PERMANENT_PC, jvmtiHookPermanentPC, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_FIND_METHOD_FROM_PC, jvmtiHookFindMethodFromPC, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_LOOKUP_NATIVE_ADDRESS, jvmtiHookLookupNativeAddress, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_FIND_NATIVE_TO_REGISTER, jvmtiHookFindNativeToRegister, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES, jvmtiHookRequiredDebugAttributes, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_INITIALIZED, jvmtiHookVMInitializedFirst, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_STARTED, jvmtiHookVMStartedFirst, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_SHUTTING_DOWN, jvmtiHookVMShutdownLast, NULL);
}


static void
jvmtiHookMonitorContendedEnter(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorContendedEnterEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMonitorContendedEnter callback = j9env->callbacks.MonitorContendedEnter;

	Trc_JVMTI_jvmtiHookMonitorContendedEnter_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMonitorContendedEnter, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		J9ThreadAbstractMonitor * lock = (J9ThreadAbstractMonitor*)data->monitor;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jobject objectRef = 0;

			if ( lock && ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) ) {
				objectRef = (jobject) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)lock->userData);
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, objectRef);
			finishedEvent(currentThread, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMonitorContendedEnter);
}


static void
jvmtiHookMonitorContendedEntered(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorContendedEnteredEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMonitorContendedEntered callback = j9env->callbacks.MonitorContendedEntered;

	Trc_JVMTI_jvmtiHookMonitorContendedEntered_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMonitorContendedEntered, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		J9ThreadAbstractMonitor * lock = (J9ThreadAbstractMonitor*)data->monitor;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jobject objectRef = 0;

			if ( lock && ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) ) {
				objectRef = (jobject) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)lock->userData);
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, objectRef);
			finishedEvent(currentThread, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMonitorContendedEntered);
}


static void
jvmtiHookMonitorWait(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorWaitEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMonitorWait callback = j9env->callbacks.MonitorWait;

	Trc_JVMTI_jvmtiHookMonitorWait_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMonitorWait, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		J9ThreadAbstractMonitor * lock = (J9ThreadAbstractMonitor*)data->monitor;
		jlong timeout = data->millis;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_MONITOR_WAIT, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jobject objectRef = 0;

			if ( lock && ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) ) {
				objectRef = (jobject) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)lock->userData);
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, objectRef, timeout);
			finishedEvent(currentThread, JVMTI_EVENT_MONITOR_WAIT, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMonitorWait);
}


static void
jvmtiHookMonitorWaited(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorWaitedEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventMonitorWaited callback = j9env->callbacks.MonitorWaited;

	Trc_JVMTI_jvmtiHookMonitorWaited_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookMonitorWaited, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		J9ThreadAbstractMonitor * lock = (J9ThreadAbstractMonitor*)data->monitor;
		jboolean timed_out = (data->reason == J9THREAD_TIMED_OUT);
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_MONITOR_WAITED, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jobject objectRef = 0;

			if ( lock && ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) ) {
				objectRef = (jobject) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, (j9object_t)lock->userData);
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, objectRef, timed_out);
			finishedEvent(currentThread, JVMTI_EVENT_MONITOR_WAITED, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookMonitorWaited);
}


static void
jvmtiHookFramePop(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	J9VMFramePopEvent * data = eventData;
	jvmtiEventFramePop callback = j9env->callbacks.FramePop;

	Trc_JVMTI_jvmtiHookFramePop_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookFramePop, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread= data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_FRAME_POP, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			J9Method * method= data->method;
			jmethodID methodID;

			methodID = getCurrentMethodID(currentThread, method);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (methodID != NULL) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, (jboolean) data->poppedByException);
			}
			finishedEvent(currentThread, JVMTI_EVENT_FRAME_POP, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookFramePop);
}


static void
jvmtiHookExceptionCatch(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMExceptionCatchEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventExceptionCatch callback = j9env->callbacks.ExceptionCatch;

	Trc_JVMTI_jvmtiHookExceptionCatch_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookExceptionCatch, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		j9object_t exception = data->exception;
		J9VMThread * currentThread = data->currentThread;
		J9JavaVM * vm = currentThread->javaVM;
		jthread threadRef;
		UDATA hadVMAccess;
		J9StackWalkState walkState;
		J9Method * catchMethod;
		IDATA catchLocation;
		UDATA javaOffloadOldState = 0;

		/* Find the catching frame */

		walkState.walkThread = currentThread;
		walkState.flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
		walkState.skipCount = 0;
		walkState.maxFrames = 1;
		vm->walkStackFrames(currentThread, &walkState);
		catchMethod = walkState.method;
		catchLocation = walkState.bytecodePCOffset;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_EXCEPTION_CATCH, &threadRef, &hadVMAccess, TRUE, (exception == NULL) ? 0 : 1, &javaOffloadOldState)) {
			jmethodID catchMethodID;
			j9object_t * exceptionRef = (j9object_t*) currentThread->arg0EA;

			if (exception != NULL) {
				*exceptionRef = exception;
			}
			catchMethodID = getCurrentMethodID(currentThread, catchMethod);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			if (catchMethodID != NULL) {
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, catchMethodID, (jlocation) catchLocation, (jobject) exceptionRef);
			}
			currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
			if (exception != NULL) {
				exception = J9_JNI_UNWRAP_REDIRECTED_REFERENCE(exceptionRef);
			}
			finishedEvent(currentThread, JVMTI_EVENT_EXCEPTION_CATCH, hadVMAccess, javaOffloadOldState);
		}

		data->exception = exception;
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookExceptionCatch);
}


/* Unused and should be deleted once a new way of informing the JIT about the capability is devised */
static void
jvmtiHookPopFramesInterrupt(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
}


static void
jvmtiHookResourceExhausted(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventResourceExhausted callback = j9env->callbacks.ResourceExhausted;
	jint flags = 0;

	Trc_JVMTI_jvmtiHookResourceExhausted_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookResourceExhausted, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMVmResourceExhaustedEvent * data = (J9VMVmResourceExhaustedEvent *) eventData;
		J9VMThread * currentThread = data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_RESOURCE_EXHAUSTED, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			const char* messageSelect = data->description;

			/* Thread exhaustion? */
			if (data->resourceTypes & J9_EX_OOM_THREAD) {
				flags |= (JVMTI_RESOURCE_EXHAUSTED_THREADS | JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR);
				if (NULL == messageSelect) {
					messageSelect = "OS Threads Exhausted";
				}
			}

			if (data->resourceTypes & J9_EX_OOM_OS_HEAP) {
				flags |= JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR;
				if (NULL == messageSelect) {
					messageSelect = "OS Heap Exhausted";
				}
			}

			if (data->resourceTypes & J9_EX_OOM_JAVA_HEAP) {
				flags |= (JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP);
				if (NULL == messageSelect) {
					messageSelect = "Java Heap Exhausted";
				}
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, flags, NULL, messageSelect);

			finishedEvent(currentThread, JVMTI_EVENT_RESOURCE_EXHAUSTED, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookResourceExhausted);
}

static void
jvmtiHookGCEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	J9JVMTIObjectTag * taggedObject;
	J9HashTableState hashState;
	UDATA phase = J9JVMTI_DATA_FROM_ENV(j9env)->phase;
	J9JVMTIObjectTag * deletedHead = NULL;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	J9VMThread * currentThread = NULL;
	UDATA javaOffloadOldState = 0;

	if (eventNum == J9HOOK_MM_OMR_GLOBAL_GC_END) {
		MM_GlobalGCEndEvent *data = (MM_GlobalGCEndEvent *)eventData;
		currentThread = (J9VMThread*)data->currentThread->_language_vmthread;
	} else {
		MM_LocalGCEndEvent *data = (MM_LocalGCEndEvent *)eventData;
		currentThread = (J9VMThread*)data->currentThread->_language_vmthread;
	}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */

	Trc_JVMTI_jvmtiHookGCEnd_Entry();

	/* Link all NULLed entries */

	taggedObject = hashTableStartDo(j9env->objectTagTable, &hashState);
	while (taggedObject != NULL) {
		if (taggedObject->ref == NULL) {
			taggedObject->ref = (j9object_t) deletedHead;
			deletedHead = (J9JVMTIObjectTag *) taggedObject;
		}
		taggedObject = hashTableNextDo(&hashState);
	}

	/* Rehash the object tag table for this environment */

	hashTableRehash(j9env->objectTagTable);

	/* Remove freed objects from the tag table - report events if need be */

	if (deletedHead != NULL) {
		jvmtiEventObjectFree objectFreeCallback = j9env->callbacks.ObjectFree;
		UDATA reportObjectFreeEvents;

		reportObjectFreeEvents =
			(phase == JVMTI_PHASE_LIVE) &&
			(objectFreeCallback != NULL) &&
			EVENT_IS_ENABLED(JVMTI_EVENT_OBJECT_FREE, &(j9env->globalEventEnable));
		do {
			taggedObject = deletedHead;
			if (reportObjectFreeEvents) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
				/* Jazz 99339: Switch away from the zAAP processor if running there */
				if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
					javaOffloadSwitchOff(j9env, currentThread, JVMTI_EVENT_OBJECT_FREE, &javaOffloadOldState);
				}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
				objectFreeCallback((jvmtiEnv *) j9env, taggedObject->tag);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
				/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
				if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
					javaOffloadSwitchOn(currentThread, JVMTI_EVENT_OBJECT_FREE, javaOffloadOldState);
				}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			}
			deletedHead = (J9JVMTIObjectTag *) taggedObject->ref;
			hashTableRemove(j9env->objectTagTable, taggedObject);
		} while (deletedHead != NULL);
	}

	/* Call the event callback */

	if (phase == JVMTI_PHASE_LIVE) {
		jvmtiEventGarbageCollectionFinish gcFinishCallback = j9env->callbacks.GarbageCollectionFinish;

		if (gcFinishCallback != NULL) {
			/*	Do not use prepareForEvent - we cannot acquire VM access during the GC callbacks.
				Only check for global event enablement, as there is no current thread parameter in this callback.
			*/

			if (EVENT_IS_ENABLED(JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, &(j9env->globalEventEnable))) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
				/* Jazz 99339: Switch away from the zAAP processor if running there */
				if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
					javaOffloadSwitchOff(j9env, currentThread, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, &javaOffloadOldState);
				}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
				gcFinishCallback((jvmtiEnv *) j9env);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
				/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
				if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
					javaOffloadSwitchOn(currentThread, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, javaOffloadOldState);
				}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookGCEnd);
}


static void
jvmtiHookGCStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventGarbageCollectionStart callback = j9env->callbacks.GarbageCollectionStart;

	Trc_JVMTI_jvmtiHookGCStart_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookGCStart, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		/*	Do not use prepareForEvent - we cannot acquire VM access during the GC callbacks.
			Only check for global event enablement, as there is no current thread parameter in this callback.
		*/

		if (EVENT_IS_ENABLED(JVMTI_EVENT_GARBAGE_COLLECTION_START, &(j9env->globalEventEnable))) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			J9VMThread * currentThread = NULL;
			UDATA javaOffloadOldState = 0;

			if (eventNum == J9HOOK_MM_OMR_GLOBAL_GC_START) {
				MM_GlobalGCStartEvent *data = (MM_GlobalGCStartEvent *)eventData;
				currentThread = (J9VMThread*)data->currentThread->_language_vmthread;
			} else {
				MM_LocalGCStartEvent *data = (MM_LocalGCStartEvent *)eventData;
				currentThread = (J9VMThread*)data->currentThread->_language_vmthread;
			}

			/* Jazz 99339: Switch away from the zAAP processor if running there */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOff(j9env, currentThread, JVMTI_EVENT_GARBAGE_COLLECTION_START, &javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			callback((jvmtiEnv *) j9env);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOn(currentThread, JVMTI_EVENT_GARBAGE_COLLECTION_START, javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookGCStart);
}


static void
jvmtiHookGCCycleStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiExtensionEvent callback = J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START);

	Trc_JVMTI_jvmtiHookGCCycleStart_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookGCCycleStart, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		/*	Do not use prepareForEvent - we cannot acquire VM access during the GC callbacks.
			Only check for global event enablement, as there is no current thread parameter in this callback.
		*/

		if (EVENT_IS_ENABLED(J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START, &(j9env->globalEventEnable))) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			UDATA javaOffloadOldState = 0;
			MM_GCCycleStartEvent *data = (MM_GCCycleStartEvent *)eventData;
			J9VMThread * currentThread = (J9VMThread*)data->omrVMThread->_language_vmthread;

			/* Jazz 99339: Switch away from the zAAP processor if running there */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOff(j9env, currentThread, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START, &javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			callback((jvmtiEnv *)j9env);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOn(currentThread, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_START, javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookGCCycleStart);
}


static void
jvmtiHookGCCycleEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiExtensionEvent callback = J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH);

	Trc_JVMTI_jvmtiHookGCCycleFinish_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookGCCycleFinish, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		/*	Do not use prepareForEvent - we cannot acquire VM access during the GC callbacks.
			Only check for global event enablement, as there is no current thread parameter in this callback.
		*/

		if (EVENT_IS_ENABLED(J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH, &(j9env->globalEventEnable))) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			UDATA javaOffloadOldState = 0;
			MM_GCCycleEndEvent *data = (MM_GCCycleEndEvent *)eventData;
			J9VMThread * currentThread = (J9VMThread*)data->omrVMThread->_language_vmthread;

			/* Jazz 99339: Switch away from the zAAP processor if running there */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOff(j9env, currentThread, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH, &javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
			callback((jvmtiEnv *)j9env);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
			/* Jazz 99339: Switch onto the zAAP processor if not running there after native finishes running on GP */
			if (J9_ARE_ALL_BITS_SET(currentThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RESTRICT_IFA)) {
				javaOffloadSwitchOn(currentThread, J9JVMTI_EVENT_COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH, javaOffloadOldState);
			}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookGCCycleFinish);
}

/**
 * Determines if the class described by <tt>data</tt> can 
 * be instrumented.
 * @param data The class file load hook event data
 * @return TRUE if the class can be instrumented. 
 */
static BOOLEAN
canClassBeInstrumented(J9VMClassLoadHookEvent* data)
{
	BOOLEAN modifiable = TRUE;
	/* Only classes on the bootpath may be marked unmodifiable */
	if (data->classLoader == data->currentThread->javaVM->systemClassLoader) {
		U_8 *classData = data->classData;
		UDATA const classDataLength = data->classDataLength;
		UDATA const annotationLength = sizeof(J9_UNMODIFIABLE_CLASS_ANNOTATION_DATA);
		/* Before the constant pool in the class file, there's at least:
		 *
		 *	u4 magic
		 *	u2 minor_version
		 *	u2 major_version
		 *	u2 constant_pool_count
		 *
 		 * Totalling 10 bytes.
		 */
		UDATA const minimumSize = 10;
		if (classDataLength > (annotationLength + minimumSize)) {
			/* Make sure this at least looks like a class file */
			if ((0xCA == classData[0]) && (0xFE == classData[1]) && (0xBA == classData[2]) && (0xBE == classData[3])) {
				U_8 *dataEnd = classData + classDataLength - annotationLength;
				classData += minimumSize;
				/* Search for the bytes representing the UTF8 annotation name */
				while (classData < dataEnd) {
					static J9_UNMODIFIABLE_CLASS_ANNOTATION_DATA annotBytes = {
						(char)1, /* tag byte for UTF8 in the constant pool */
						(char)(LITERAL_STRLEN(J9_UNMODIFIABLE_CLASS_ANNOTATION) >> 8), /* high byte of size (big endian) */
						(char)(LITERAL_STRLEN(J9_UNMODIFIABLE_CLASS_ANNOTATION) & 0xFF), /* low byte of size (big endian) */
						J9_UNMODIFIABLE_CLASS_ANNOTATION /* UTF8 data */
					};
					UDATA scanSize = annotationLength;
					U_8 *scanBytes = (U_8*)&annotBytes;
					do {
						if (*scanBytes++ != *classData++) {
							goto notFound;
						}
						scanSize -= 1;
					} while (0 != scanSize);
					modifiable = FALSE;
					break;
notFound: ;
				}
			}
		}
	}
	return modifiable;
}


static void
jvmtiHookClassFileLoadHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassLoadHookEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventClassFileLoadHook callback = j9env->callbacks.ClassFileLoadHook;
	J9JavaVM * vm = j9env->vm;

	Trc_JVMTI_jvmtiHookClassFileLoadHook_Entry();

#if JAVA_SPEC_VERSION >= 9
	if ((J2SE_VERSION(vm) >= J2SE_V11)
		&& (j9env->capabilities.can_generate_early_class_hook_events == 0)
	) {
		ENSURE_EVENT_PHASE_START_OR_LIVE(jvmtiHookClassFileLoadHook, j9env);
	} else {
		ENSURE_EVENT_PHASE_PRIMORDIAL_START_OR_LIVE(jvmtiHookClassFileLoadHook, j9env);
	}

#else /* JAVA_SPEC_VERSION >= 9 */
	ENSURE_EVENT_PHASE_PRIMORDIAL_START_OR_LIVE(jvmtiHookClassFileLoadHook, j9env);
#endif /* JAVA_SPEC_VERSION >= 9 */
	if (!canClassBeInstrumented(data)) {
		TRACE_JVMTI_EVENT_RETURN(jvmtiHookClassFileLoadHook);
	}

	/* Call the event callback */
	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		UDATA hadVMAccess;
		j9object_t classLoaderObject = (data->classLoader == vm->systemClassLoader) ? NULL : J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, data->classLoader);
		j9object_t protectionDomain = data->protectionDomain;
		J9Class * oldClass = data->oldClass;
		UDATA extraRefs = 0;
		UDATA javaOffloadOldState = 0;

		if (classLoaderObject != NULL) {
			++extraRefs;
		}
		if (protectionDomain != NULL) {
			++extraRefs;
		}
		if (oldClass != NULL) {
			++extraRefs;
		}

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL, &hadVMAccess, TRUE, extraRefs, &javaOffloadOldState)) {
			j9object_t * classRef = NULL;
			j9object_t * classLoaderRef = NULL;
			j9object_t * pdRef = NULL;
			unsigned char * newData = NULL;
			jint newLength = 0;
			j9object_t * refs = (j9object_t*) currentThread->arg0EA;

			if (classLoaderObject != NULL) {
				classLoaderRef = refs--;
				*classLoaderRef = classLoaderObject;
			}
			if (protectionDomain != NULL) {
				pdRef = refs--;
				*pdRef = protectionDomain;
			}
			if (oldClass != NULL) {
				classRef = refs--;
				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(oldClass);
			}

			vm->internalVMFunctions->internalExitVMToJNI(currentThread);

			callback(
				(jvmtiEnv *) j9env,
				(JNIEnv *) currentThread,
				(jclass) classRef,
				(jobject) classLoaderRef,
				(const char *) data->className,
				(jobject) pdRef,
				(jint) data->classDataLength,
				(const unsigned char*) data->classData,
				&newLength,
				&newData);

			currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
			if (pdRef != NULL) {
				data->protectionDomain = J9_JNI_UNWRAP_REDIRECTED_REFERENCE(pdRef);
			}
			finishedEvent(currentThread, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, hadVMAccess, javaOffloadOldState);

			if (newData != NULL) {
				if (data->freeFunction != NULL) {
					data->freeFunction(data->freeUserData, data->classData);
				}
				data->classData = (U_8 *) newData;
				data->classDataLength = (UDATA) newLength;
				data->freeUserData = j9env;
				data->freeFunction = jvmtiFreeClassData;
				data->classDataReplaced = TRUE;
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookClassFileLoadHook);
}


static void
jvmtiFreeClassData(void * userData, void * address)
{
	jvmtiEnv * env = (jvmtiEnv *) userData;

	(*env)->Deallocate(env, address);
}



#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) 
static void
jvmtiHookCheckForDataBreakpoint(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9CheckForDataBreakpointEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	J9JVMTIEnv * j9env;
	pool_state envPoolState;
	J9ROMConstantPoolItem * romConstantPool = data->constantPool->romConstantPool;
	J9ROMFieldRef * romFieldRef = (J9ROMFieldRef *) (romConstantPool + data->fieldIndex);
	J9ROMClassRef * romClassRef = (J9ROMClassRef *) (romConstantPool + romFieldRef->classRefCPIndex);
	J9UTF8 * resolveClassName = J9ROMCLASSREF_NAME(romClassRef);
	J9ROMNameAndSignature * resolveNAS = J9ROMFIELDREF_NAMEANDSIGNATURE(romFieldRef);
	J9UTF8 * resolveName = J9ROMNAMEANDSIGNATURE_NAME(resolveNAS);
	J9UTF8 * resolveSig = J9ROMNAMEANDSIGNATURE_SIGNATURE(resolveNAS);

	Trc_JVMTI_jvmtiHookCheckForDataBreakpoint_Entry();

	/* Look for all field watches in all environments */

	omrthread_monitor_enter(jvmtiData->mutex);

	j9env = pool_startDo(jvmtiData->environments, &envPoolState);
	while (j9env != NULL) {
		/* CMVC 196966: only inspect live (not disposed) environments */
		if (0 == (j9env->flags & J9JVMTIENV_FLAG_DISPOSED)) {
			J9HashTableState walkState;
			J9JVMTIWatchedClass *watchedClass = hashTableStartDo(j9env->watchedClasses, &walkState);
			while (NULL != watchedClass) {
				J9Class *clazz = watchedClass->clazz;
				J9ROMClass *romClass = clazz->romClass;
				UDATA fieldCount = romClass->romFieldCount;
				UDATA *watchBits = (UDATA*)&watchedClass->watchBits;
				UDATA index = 0;
				UDATA descriptionsRemaining = 0;
				UDATA descriptionBits = 0;
				if (fieldCount > J9JVMTI_WATCHED_FIELDS_PER_UDATA) {
					watchBits = watchedClass->watchBits;				
				}
				while (index < fieldCount) {
					if (0 == descriptionsRemaining) {
						descriptionsRemaining = J9JVMTI_WATCHED_FIELDS_PER_UDATA;
						descriptionBits = *watchBits++;
					}
					if (descriptionBits & (data->isStore ? 2 : 1)) {
						/* In order for a watch to have been placed, the fieldID for the field in question
						 * must already have been created (it's a parameter to the JVMTI calls).
						 */
						void **jniIDs = clazz->jniIDs;
						J9JNIFieldID *fieldID = NULL;
						J9ROMFieldShape * romField = NULL;
						Assert_JVMTI_notNull(jniIDs);
						fieldID = (J9JNIFieldID*)(jniIDs[index + clazz->romClass->romMethodCount]);
						Assert_JVMTI_notNull(fieldID);
						romField = fieldID->field;
						if ((romField->modifiers & J9AccStatic) == (data->isStatic ? J9AccStatic : 0)) {
							if (data->resolvedField == NULL) {
								J9UTF8 * romFieldClassName = J9ROMCLASS_CLASSNAME(fieldID->declaringClass->romClass);

								if (J9UTF8_EQUALS(resolveClassName, romFieldClassName)) {
									J9UTF8 * romFieldName = J9ROMFIELDSHAPE_NAME(romField);

									if (J9UTF8_EQUALS(resolveName, romFieldName)) {
										J9UTF8 * romFieldSig = J9ROMFIELDSHAPE_SIGNATURE(romField);

										if (J9UTF8_EQUALS(resolveSig, romFieldSig)) {
											data->result = J9_JIT_RESOLVE_FAIL_COMPILE;
											goto done;
										}
									}
								}
							} else {
								if (data->resolvedField == romField) {
									data->result = J9_JIT_RESOLVE_FAIL_COMPILE;
									goto done;
								}
							}
						}
					}
					index += 1;
					descriptionsRemaining -= 1;
					descriptionBits >>= J9JVMTI_WATCHED_FIELD_BITS_PER_FIELD;
				}
				watchedClass = hashTableNextDo(&walkState);
			}
		}
		j9env = pool_nextDo(&envPoolState);
	}
done:
	omrthread_monitor_exit(jvmtiData->mutex);

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookCheckForDataBreakpoint);
}

#endif /* J9VM_JIT_FULL_SPEED_DEBUG (autogen) */


static void
jvmtiHookLookupNativeAddress(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMLookupNativeAddressEvent * data = eventData;
	J9Method * nativeMethod = data->nativeMethod;

	Trc_JVMTI_jvmtiHookLookupNativeAddress_Entry();

	/* Do nothing if the method is already bound */

	if (!J9_NATIVE_METHOD_IS_BOUND(nativeMethod)) {
		char * longJNI = data->longJNI;
		char * shortJNI = data->shortJNI;
		UDATA functionArgCount = data->functionArgCount;
		lookupNativeAddressCallback callback = data->callback;
		J9VMThread *currentThread = data->currentThread;
		J9JVMTIData * jvmtiData = userData;
		J9NativeLibrary * nativeLibrary;
		UDATA prefixOffset;

		/* Look up the given name in the agent libraries */

		nativeLibrary = jvmtiData->agentLibrariesHead;
		while (nativeLibrary != NULL) {
			callback(currentThread, nativeMethod, nativeLibrary, longJNI, shortJNI, functionArgCount, TRUE);
			if (J9_NATIVE_METHOD_IS_BOUND(nativeMethod)) {
				goto done;
			}
			nativeLibrary = nativeLibrary->next;
		}

		/* Remove prefixes for retransform-capable agents */

		prefixOffset = lookupNativeAddressHelper(currentThread, jvmtiData, nativeMethod, 0, J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE, functionArgCount, callback);
		if (!J9_NATIVE_METHOD_IS_BOUND(nativeMethod)) {
			/* Remove prefixes for retransform-incapable agents */

			lookupNativeAddressHelper(currentThread, jvmtiData, nativeMethod, prefixOffset, 0, functionArgCount, callback);
		}
	}

done:
	TRACE_JVMTI_EVENT_RETURN(jvmtiHookLookupNativeAddress);
}


static UDATA
lookupNativeAddressHelper(J9VMThread * currentThread, J9JVMTIData * jvmtiData, J9Method * nativeMethod, UDATA prefixOffset, UDATA retransformFlag, UDATA functionArgCount, lookupNativeAddressCallback callback)
{
	J9JVMTIEnv * j9env;
	J9JavaVM * vm = jvmtiData->vm;
	J9Class * methodClass = J9_CLASS_FROM_METHOD(nativeMethod);
	J9ClassLoader * classLoader = methodClass->classLoader;
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(nativeMethod);
	J9UTF8 * methodName = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(nativeMethod)->ramClass->romClass, romMethod);
	J9UTF8 * methodSignature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(nativeMethod)->ramClass->romClass, romMethod);
	U_8 * nameData = J9UTF8_DATA(methodName);
	UDATA nameLength = J9UTF8_LENGTH(methodName);

	JVMTI_ENVIRONMENTS_REVERSE_DO(jvmtiData, j9env) {
		if ((j9env->flags & J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE) == retransformFlag) {
			jint prefixCount;
			char * prefix;
			PORT_ACCESS_FROM_JAVAVM(vm);

			/* Remove the prefixes */

			prefixCount = j9env->prefixCount;
			prefix = j9env->prefixes;
			while (prefixCount != 0) {
				size_t prefixLength = strlen(prefix);

				if ((prefixOffset + prefixLength) < nameLength) {
					if (memcmp(prefix, nameData + prefixOffset, prefixLength) == 0) {
						prefixOffset += prefixLength;

						/* Verify that a method of this name exists */

						if (methodExists(methodClass, nameData + prefixOffset, nameLength - prefixOffset, methodSignature)) {
							U_8 * nativeNames = vm->internalVMFunctions->buildNativeFunctionNames(vm, nativeMethod, methodClass, prefixOffset);

							if (nativeNames != NULL) {
								U_8 * longJNI = nativeNames;
								U_8 * shortJNI = longJNI + strlen((char *) longJNI) + 1;
								J9NativeLibrary * nativeLibrary;

								/* First look in JNI libraries in the native method's classLoader */

								nativeLibrary = classLoader->librariesHead;
								while (nativeLibrary != NULL) {
									callback(currentThread, nativeMethod, nativeLibrary, (char*)longJNI, (char*)shortJNI, functionArgCount, TRUE);
									if (J9_NATIVE_METHOD_IS_BOUND(nativeMethod)) {
										j9mem_free_memory(nativeNames);
										goto done;
									}
									nativeLibrary = nativeLibrary->next;
								}

								/* Next look through the agent libraries */

								nativeLibrary = jvmtiData->agentLibrariesHead;
								while (nativeLibrary != NULL) {
									callback(currentThread, nativeMethod, nativeLibrary, (char*)longJNI, (char*)shortJNI, functionArgCount, TRUE);
									if (J9_NATIVE_METHOD_IS_BOUND(nativeMethod)) {
										j9mem_free_memory(nativeNames);
										goto done;
									}
									nativeLibrary = nativeLibrary->next;
								}
								j9mem_free_memory(nativeNames);
							}
						}
					}
				}
				prefix += (prefixLength + 1);
				--prefixCount;
			}
		}

		JVMTI_ENVIRONMENTS_REVERSE_NEXT_DO(jvmtiData, j9env);

	}
done:
	return prefixOffset;
}


static UDATA
methodExists(J9Class * methodClass, U_8 * nameData, UDATA nameLength, J9UTF8 * signature)
{
	U_32 methodCount = methodClass->romClass->romMethodCount;
	J9Method * method = methodClass->ramMethods;
	U_8 * signatureData = J9UTF8_DATA(signature);
	U_16 signatureLength = J9UTF8_LENGTH(signature);

	do {
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * searchName = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * searchSignature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

		if ((nameLength == J9UTF8_LENGTH(searchName)) && (memcmp(nameData, J9UTF8_DATA(searchName), nameLength) == 0)) {
			if ((signatureLength == J9UTF8_LENGTH(searchSignature)) && (memcmp(signatureData, J9UTF8_DATA(searchSignature), signatureLength) == 0)) {
				if ((romMethod->modifiers & J9AccNative) == 0) {
					return TRUE;
				}
			}
		}

		++method;
	} while (--methodCount != 0);

	return FALSE;
}


static void
jvmtiHookFindNativeToRegister(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIData * jvmtiData = userData;
	J9VMFindNativeToRegisterEvent * data = eventData;
	J9Method * nonNativeMethod = data->method;
	J9ROMMethod * nonNativeRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(nonNativeMethod);

	Trc_JVMTI_jvmtiHookFindNativeToRegister_Entry();

	/* If a previous listener has located a native, do nothing */

	if ((nonNativeRomMethod->modifiers & J9AccNative) == 0) {
		J9UTF8 * nonNativeMethodName = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(nonNativeMethod)->ramClass->romClass, nonNativeRomMethod);
		J9UTF8 * nonNativeMethodSignature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(nonNativeMethod)->ramClass->romClass, nonNativeRomMethod);
		U_8 * nonNativeMethodNameData = J9UTF8_DATA(nonNativeMethodName);
		UDATA nonNativeMethodNameLength = J9UTF8_LENGTH(nonNativeMethodName);
		U_8 * nonNativeMethodSignatureData = J9UTF8_DATA(nonNativeMethodSignature);
		UDATA nonNativeMethodSignatureLength = J9UTF8_LENGTH(nonNativeMethodSignature);
		J9Class * methodClass = J9_CLASS_FROM_METHOD(nonNativeMethod);
		J9Method * method = methodClass->ramMethods;
		UDATA count = methodClass->romClass->romMethodCount;

		/* Walk all native methods in the class looking for ones which end in the name */

		while (count != 0) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

			if (romMethod->modifiers & J9AccNative) {
				J9UTF8 * methodSignature = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

				if ((J9UTF8_LENGTH(methodSignature) == nonNativeMethodSignatureLength) && (memcmp(J9UTF8_DATA(methodSignature), nonNativeMethodSignatureData, nonNativeMethodSignatureLength) == 0)) {
					J9UTF8 * methodName = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
					UDATA methodNameLength = J9UTF8_LENGTH(methodName);
					U_8 * methodNameData = J9UTF8_DATA(methodName);

					if (methodNameLength > nonNativeMethodNameLength) {
						UDATA prefixSize = methodNameLength - nonNativeMethodNameLength;

						if (memcmp(methodNameData + prefixSize, nonNativeMethodNameData, nonNativeMethodNameLength) == 0) {
							UDATA prefixOffset;

							/* Remove prefixes for retransform-capable agents */

							prefixOffset = removeMethodPrefixesHelper(jvmtiData, methodNameData, prefixSize, 0, J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE);
							if (prefixOffset != prefixSize) {
								/* Remove prefixes for retransform-incapable agents */

								prefixOffset = removeMethodPrefixesHelper(jvmtiData, methodNameData, prefixSize, prefixOffset, 0);
							}
							if (prefixOffset == prefixSize) {
								data->method = method;
								break;
							}
						}
					}
				}
			}
			--count;
			++method;
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookFindNativeToRegister);
}


static UDATA
removeMethodPrefixesHelper(J9JVMTIData * jvmtiData, U_8 * methodName, UDATA methodPrefixSize, UDATA prefixOffset, UDATA retransformFlag)
{
	J9JVMTIEnv * j9env;

	JVMTI_ENVIRONMENTS_REVERSE_DO(jvmtiData, j9env) {
		if ((j9env->flags & J9JVMTIENV_FLAG_RETRANSFORM_CAPABLE) == retransformFlag) {
			UDATA prefixCount;
			char * prefix;

			/* Remove the prefixes */

			prefixCount = j9env->prefixCount;
			prefix = j9env->prefixes;
			while (prefixCount != 0) {
				size_t prefixLength = strlen(prefix);

				if ((prefixOffset + prefixLength) <= methodPrefixSize) {
					if (memcmp(prefix, methodName + prefixOffset, prefixLength) == 0) {
						prefixOffset += prefixLength;
					}
				}
				--prefixCount;
				prefix += (prefixLength + 1);
			}
		}

		JVMTI_ENVIRONMENTS_REVERSE_NEXT_DO(jvmtiData, j9env);
	}

	return prefixOffset;
}


#if defined(J9VM_INTERP_NATIVE_SUPPORT)

static void
jvmtiHookCompilingStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9CompilingStartEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9VMThread* currentThread = data->currentThread;
	J9Method* method = data->method;
	jvmtiExtensionEvent callback = J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_COMPILING_START);
	UDATA hadVMAccess;
	UDATA javaOffloadOldState = 0;

	Trc_JVMTI_jvmtiHookCompilingStart_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookCompilingStart, j9env);

	/* Call the event callback */
	if (prepareForEvent(j9env, currentThread, currentThread, J9JVMTI_EVENT_COM_IBM_COMPILING_START, NULL, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
		J9JavaVM * vm = currentThread->javaVM;
		jmethodID methodID;

		methodID = getCurrentMethodID(currentThread, method);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		if (methodID != NULL) {
			if (callback != NULL) {
				callback((jvmtiEnv *) j9env, methodID);
			}
		}
		finishedEvent(currentThread, J9JVMTI_EVENT_COM_IBM_COMPILING_START, hadVMAccess, javaOffloadOldState);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookCompilingStart);
}

#endif /* INTERP_NATIVE_SUPPORT */


#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void
jvmtiHookCompilingEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9CompilingEndEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9VMThread* currentThread = data->currentThread;
	J9Method* method = data->method;
	jvmtiExtensionEvent callback = *J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_COMPILING_END);
	UDATA hadVMAccess;
	UDATA javaOffloadOldState = 0;

	Trc_JVMTI_jvmtiHookCompilingStart_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookCompilingEnd, j9env);

	/* Call the event callback */
	if (prepareForEvent(j9env, currentThread, currentThread, J9JVMTI_EVENT_COM_IBM_COMPILING_END, NULL, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
		J9JavaVM * vm = currentThread->javaVM;
		jmethodID methodID;

		methodID = getCurrentMethodID(currentThread, method);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		if (methodID != NULL) {
			if (callback != NULL) {
				callback((jvmtiEnv *) j9env, methodID);
			}
		}
		finishedEvent(currentThread, J9JVMTI_EVENT_COM_IBM_COMPILING_END, hadVMAccess, javaOffloadOldState);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookCompilingEnd);
}

#endif /* INTERP_NATIVE_SUPPORT */

#if JAVA_SPEC_VERSION >= 11
static void 
jvmtiHookSampledObjectAlloc(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;
	jvmtiEventSampledObjectAlloc callback = j9env->callbacks.SampledObjectAlloc;

	Trc_JVMTI_jvmtiHookSampledObjectAlloc_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookSampledObjectAlloc, j9env);

	if (NULL != callback) {
		MM_ObjectAllocationSamplingEvent *data = eventData;
		J9VMThread *currentThread = data->currentThread;
		jthread threadRef = NULL;
		UDATA hadVMAccess = 0;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, &threadRef, &hadVMAccess, TRUE, 2, &javaOffloadOldState)) {
			j9object_t *objectRef = (j9object_t*) currentThread->arg0EA;
			j9object_t *classRef = (j9object_t*) (currentThread->arg0EA - 1);
			J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;

			*objectRef = data->object;
			*classRef = J9VM_J9CLASS_TO_HEAPCLASS(data->clazz);
			vmFuncs->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, (jobject) objectRef, (jclass) classRef, (jlong) data->objectSize);
			vmFuncs->internalEnterVMFromJNI(currentThread);
			data->object = *objectRef;
			finishedEvent(currentThread, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookSampledObjectAlloc);
}
#endif /* JAVA_SPEC_VERSION >= 11 */

static void
jvmtiHookObjectAllocate(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JVMTIEnv * j9env = userData;

	Trc_JVMTI_jvmtiHookObjectAllocate_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookObjectAllocate, j9env);

	/* Call one or the other event callback */

	if (eventNum == J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE) {
		jvmtiExtensionEvent callback = J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC);

		if (callback != NULL) {
			J9VMObjectAllocateInstrumentableEvent * data = eventData;
			J9VMThread * currentThread = data->currentThread;
			jthread threadRef;
			UDATA hadVMAccess;
			UDATA javaOffloadOldState = 0;

			if (prepareForEvent(j9env, currentThread, currentThread, J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC, &threadRef, &hadVMAccess, TRUE, 2, &javaOffloadOldState)) {
				j9object_t * objectRef = (j9object_t*) currentThread->arg0EA;
				j9object_t * classRef = objectRef - 1;
				J9Class* clazz;
				J9JavaVM * vm = currentThread->javaVM;

				*objectRef = data->object;
				clazz = J9OBJECT_CLAZZ(currentThread, data->object);
				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				vm->internalVMFunctions->internalExitVMToJNI(currentThread);
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, (jobject) objectRef, (jclass) classRef, (jlong) data->size);
				currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
				data->object = J9_JNI_UNWRAP_REDIRECTED_REFERENCE(objectRef);
				finishedEvent(currentThread, J9JVMTI_EVENT_COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC, hadVMAccess, javaOffloadOldState);
			}
		}
	} else {
		jvmtiEventVMObjectAlloc callback = j9env->callbacks.VMObjectAlloc;

		if (callback != NULL) {
			J9VMObjectAllocateEvent * data = eventData;
			J9VMThread * currentThread = data->currentThread;
			jthread threadRef;
			UDATA hadVMAccess;
			UDATA javaOffloadOldState = 0;

			if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_VM_OBJECT_ALLOC, &threadRef, &hadVMAccess, TRUE, 2, &javaOffloadOldState)) {
				j9object_t * objectRef = (j9object_t*) currentThread->arg0EA;
				j9object_t * classRef = (j9object_t*) (objectRef - 1);
				J9Class* clazz;
				J9JavaVM * vm = currentThread->javaVM;

				*objectRef = data->object;
				clazz = J9OBJECT_CLAZZ(currentThread, data->object);
				*classRef = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				vm->internalVMFunctions->internalExitVMToJNI(currentThread);
				callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, (jobject) objectRef, (jclass) classRef, (jlong) data->size);
				currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
				data->object = J9_JNI_UNWRAP_REDIRECTED_REFERENCE(objectRef);
				finishedEvent(currentThread, JVMTI_EVENT_VM_OBJECT_ALLOC, hadVMAccess, javaOffloadOldState);
			}
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookObjectAllocate);
}




static void
jvmtiHookJNINativeBind(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMJNINativeBindEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	jvmtiEventNativeMethodBind callback = j9env->callbacks.NativeMethodBind;

	Trc_JVMTI_jvmtiHookJNINativeBind_Entry();

	ENSURE_EVENT_PHASE_PRIMORDIAL_START_OR_LIVE(jvmtiHookJNINativeBind, j9env);

	/* Call the event callback */

	if (callback != NULL) {
		J9VMThread * currentThread = data->currentThread;
		jthread threadRef;
		UDATA hadVMAccess;
		UDATA javaOffloadOldState = 0;

		if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_NATIVE_METHOD_BIND, &threadRef, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
			J9JavaVM * vm = currentThread->javaVM;
			jmethodID methodID;

			methodID = getCurrentMethodID(currentThread, data->nativeMethod);
			vm->internalVMFunctions->internalExitVMToJNI(currentThread);
			callback((jvmtiEnv *) j9env, (JNIEnv *) currentThread, threadRef, methodID, data->nativeMethodAddress, &(data->nativeMethodAddress));
			finishedEvent(currentThread, JVMTI_EVENT_NATIVE_METHOD_BIND, hadVMAccess, javaOffloadOldState);
		}
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookJNINativeBind);
}



#if (defined(J9VM_JIT_MICRO_JIT)  || defined(J9VM_INTERP_NATIVE_SUPPORT))
jvmtiError
startCompileEventThread(J9JVMTIData * jvmtiData)
{
	J9JavaVM * vm = jvmtiData->vm;

	omrthread_monitor_enter(jvmtiData->compileEventMutex);
	if (NULL == jvmtiData->compileEventThread) {
		jvmtiData->compileEvents = pool_new(sizeof(J9JVMTICompileEvent), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_JVMTI, POOL_FOR_PORT(vm->portLibrary));
		if (NULL == jvmtiData->compileEvents) {
			omrthread_monitor_exit(jvmtiData->compileEventMutex);
			return JVMTI_ERROR_OUT_OF_MEMORY;
		}

		jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_NEW;
		if (0 != omrthread_create(&(jvmtiData->compileEventThread), vm->defaultOSStackSize, J9THREAD_PRIORITY_NORMAL, FALSE, compileEventThreadProc, jvmtiData)) {
			jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD;
			omrthread_monitor_notify_all(jvmtiData->compileEventMutex);
			omrthread_monitor_exit(jvmtiData->compileEventMutex);
			return JVMTI_ERROR_OUT_OF_MEMORY;
		}
	}
	omrthread_monitor_exit(jvmtiData->compileEventMutex);

	return JVMTI_ERROR_NONE;
}

#endif /* J9VM_JIT_MICRO_JIT || INTERP_NATIVE_SUPPORT */


#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void
stopCompileEventThread(J9JVMTIData * jvmtiData)
{
	J9JavaVM * vm = jvmtiData->vm;
	J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);

	/* Unregister code load/unload hooks that use compileEventMutex which we're going to destroy. */
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_DYNAMIC_CODE_LOAD, jvmtiHookDynamicCodeLoad, NULL);
	(*vmHook)->J9HookUnregister(vmHook, J9HOOK_VM_DYNAMIC_CODE_UNLOAD, jvmtiHookDynamicCodeUnload, NULL);

	if (jvmtiData->compileEventThread != NULL) {
		omrthread_monitor_enter(jvmtiData->compileEventMutex);
		jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_DYING;
		omrthread_monitor_notify_all(jvmtiData->compileEventMutex);
		while (jvmtiData->compileEventThreadState != J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD) {
			omrthread_monitor_wait(jvmtiData->compileEventMutex);
		}
		omrthread_monitor_exit(jvmtiData->compileEventMutex);
	}

	if (jvmtiData->compileEvents != NULL) {
		pool_kill(jvmtiData->compileEvents);
		jvmtiData->compileEvents = NULL;
	}
}

#endif /* INTERP_NATIVE_SUPPORT */


#if (defined(J9VM_JIT_MICRO_JIT)  || defined(J9VM_INTERP_NATIVE_SUPPORT))
static int J9THREAD_PROC
compileEventThreadProc(void *entryArg)
{
	J9JVMTIData * jvmtiData = (J9JVMTIData *) entryArg;
	J9JavaVM * vm = jvmtiData->vm;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA threadPrivateFlag = J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD;

	if (JVMTI_PHASE_LIVE != jvmtiData->phase) {
		/*
		 * This method is tied to JVMTI capability can_generate_compiled_method_load_events.
		 * This capability can only be requested during load and live phase.
		 * If we're not in live phase (must be load), attach as a system daemon, with no object for now.
		 * The thread will be initialized by jvmtiHookVMStartedFirst() via J9HOOK_VM_START
		 */
		threadPrivateFlag |= J9_PRIVATE_FLAGS_NO_OBJECT;
	}

   if (JNI_OK == vm->internalVMFunctions->internalAttachCurrentThread
	   (vm, &currentThread, NULL, threadPrivateFlag, omrthread_self())
	  ){
		/* Indicate that this thread is alive */

		omrthread_monitor_enter(jvmtiData->compileEventMutex);
		jvmtiData->compileEventVMThread = currentThread;
		jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_ALIVE;
		omrthread_monitor_notify_all(jvmtiData->compileEventMutex);

		/* Wait for events to be available to report, or for a termination request to be posted */

		for (;;) {
			J9JVMTICompileEvent * compEvent;
			J9JVMTIEnv * j9env;

			while (J9_LINKED_LIST_IS_EMPTY(jvmtiData->compileEventQueueHead) && (jvmtiData->compileEventThreadState == J9JVMTI_COMPILE_EVENT_THREAD_STATE_ALIVE)) {
				omrthread_monitor_notify_all(jvmtiData->compileEventMutex);
				omrthread_monitor_wait(jvmtiData->compileEventMutex);
			}
			if (jvmtiData->compileEventThreadState != J9JVMTI_COMPILE_EVENT_THREAD_STATE_ALIVE) {
				break;
			}

			/* Dequeue event */

			J9_LINKED_LIST_REMOVE_FIRST(jvmtiData->compileEventQueueHead, compEvent);
			if (compEvent->methodID == NULL) {
				if ((jvmtiData->phase == JVMTI_PHASE_PRIMORDIAL) || (jvmtiData->phase == JVMTI_PHASE_START) || (jvmtiData->phase == JVMTI_PHASE_LIVE)) {
					omrthread_monitor_exit(jvmtiData->compileEventMutex);

					/* Dispatch event to all agents who are listening */

					JVMTI_ENVIRONMENTS_DO(jvmtiData, j9env) {
						jthread threadRef;
						UDATA hadVMAccess;
						jvmtiEventDynamicCodeGenerated callback = j9env->callbacks.DynamicCodeGenerated;

						/* Currently only reporting load events */

						if (callback != NULL) {
							UDATA javaOffloadOldState = 0;

							if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
								callback((jvmtiEnv *) j9env, compEvent->compile_info, compEvent->code_addr, (jint) compEvent->code_size);
								finishedEvent(currentThread, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, hadVMAccess, javaOffloadOldState);
							}
						}

						JVMTI_ENVIRONMENTS_NEXT_DO(jvmtiData, j9env);
					}

					omrthread_monitor_enter(jvmtiData->compileEventMutex);
				}

				j9mem_free_memory((void *) compEvent->compile_info);
			} else {
				if ((jvmtiData->phase == JVMTI_PHASE_LIVE) || (jvmtiData->phase == JVMTI_PHASE_START)) {
					omrthread_monitor_exit(jvmtiData->compileEventMutex);

					/* Dispatch event to all agents who are listening */

					JVMTI_ENVIRONMENTS_DO(jvmtiData, j9env) {
						jthread threadRef;
						UDATA hadVMAccess;

						if (compEvent->isLoad) {
							jvmtiEventCompiledMethodLoad callback = j9env->callbacks.CompiledMethodLoad;

							if (callback != NULL) {
								UDATA javaOffloadOldState = 0;

								if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_COMPILED_METHOD_LOAD, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
									callback((jvmtiEnv *) j9env, compEvent->methodID, (jint) compEvent->code_size, compEvent->code_addr, (jint) 0, (const jvmtiAddrLocationMap *) NULL, compEvent->compile_info);
									finishedEvent(currentThread, JVMTI_EVENT_COMPILED_METHOD_LOAD, hadVMAccess, javaOffloadOldState);
								}
							}
						} else {
							jvmtiEventCompiledMethodUnload callback = j9env->callbacks.CompiledMethodUnload;

							if (callback != NULL) {
								UDATA javaOffloadOldState = 0;

								if (prepareForEvent(j9env, currentThread, currentThread, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, &threadRef, &hadVMAccess, FALSE, 0, &javaOffloadOldState)) {
									callback((jvmtiEnv *) j9env, compEvent->methodID, compEvent->code_addr);
									finishedEvent(currentThread, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, hadVMAccess, javaOffloadOldState);
								}
							}
						}

						JVMTI_ENVIRONMENTS_NEXT_DO(jvmtiData, j9env);
					}

					omrthread_monitor_enter(jvmtiData->compileEventMutex);
				}
			}

			pool_removeElement(jvmtiData->compileEvents, compEvent);
		}

		vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
	}

	/* Indicate this thread is dead and exit it */

	jvmtiData->compileEventThreadState = J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD;
	jvmtiData->compileEventQueueHead = NULL;
	jvmtiData->compileEventThread = NULL;
	omrthread_monitor_notify_all(jvmtiData->compileEventMutex);
	omrthread_exit(jvmtiData->compileEventMutex);
	return 0;
}

#endif /* J9VM_JIT_MICRO_JIT || INTERP_NATIVE_SUPPORT */


IDATA
enableDebugAttribute(J9JVMTIEnv * j9env, UDATA attributes)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(j9env);
	J9HookInterface ** vmHook = vm->internalVMFunctions->getVMHookInterface(vm);
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);

	if ((vm->requiredDebugAttributes & attributes) != attributes) {
		if ((*vmHook)->J9HookRegisterWithCallSite(vmHook, J9HOOK_VM_REQUIRED_DEBUG_ATTRIBUTES, jvmtiHookRequiredDebugAttributes, OMR_GET_CALLSITE(), jvmtiData)) {
			return 1;
		}
		jvmtiData->requiredDebugAttributes |= attributes;
	}

	return 0;
}



#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void
jvmtiHookClassUnload(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassUnloadEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	J9VMThread * currentThread = data->currentThread;
	J9Class * currentClass = data->clazz;
	J9JVMTIEnv * j9env;

	Trc_JVMTI_jvmtiHookClassUnload_Entry(currentClass);

	/* Process all environments */

	JVMTI_ENVIRONMENTS_DO(jvmtiData, j9env) {
		removeUnloadedAgentBreakpoints(j9env, currentThread, currentClass);
		removeUnloadedFieldWatches(j9env, currentClass);

		JVMTI_ENVIRONMENTS_NEXT_DO(jvmtiData, j9env);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookClassUnload);
}

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */




#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) /* priv. proto (autogen) */

static void
removeUnloadedAgentBreakpoints(J9JVMTIEnv * j9env, J9VMThread * currentThread, J9Class * unloadedClass)
{
	J9JVMTIAgentBreakpoint * agentBreakpoint;
	pool_state poolState;

	/* Remove all breakpoints for methods within the unloaded class */

	agentBreakpoint = pool_startDo(j9env->breakpoints, &poolState);
	while (agentBreakpoint != NULL) {
		J9JNIMethodID * methodID = (J9JNIMethodID *) agentBreakpoint->method;
		J9Class * methodClass = J9_CLASS_FROM_METHOD(methodID->method);

		if (methodClass == unloadedClass) {
			deleteAgentBreakpoint(currentThread, j9env, agentBreakpoint);
		}

		agentBreakpoint = pool_nextDo(&poolState);
	}
}

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


static void
removeUnloadedFieldWatches(J9JVMTIEnv * j9env, J9Class * unloadedClass)
{
	J9HashTableState walkState;
	J9JVMTIWatchedClass *watchedClass = hashTableStartDo(j9env->watchedClasses, &walkState);
	while (NULL != watchedClass) {
		if (unloadedClass == watchedClass->clazz) {
			hashTableDoRemove(&walkState);
		}
		watchedClass = hashTableNextDo(&walkState);
	}
}


#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void
jvmtiHookDynamicCodeLoad(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9DynamicCodeLoadEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	J9VMThread* currentThread = data->currentThread;
	J9Method * method = data->method;
	UDATA queueEvent = FALSE;
	jmethodID methodID = NULL;

	Trc_JVMTI_jvmtiHookDynamicCodeLoad_Entry();

	if (method == NULL) {
		if ((jvmtiData->phase == JVMTI_PHASE_PRIMORDIAL) || (jvmtiData->phase == JVMTI_PHASE_START) || (jvmtiData->phase == JVMTI_PHASE_LIVE)) {
			queueEvent = TRUE;
		}
	} else {
		if (jvmtiData->phase == JVMTI_PHASE_LIVE) {
			methodID = getCurrentMethodID(currentThread, method);
			if (methodID != NULL) {
				queueEvent = TRUE;
			}
		}
	}

	if (queueEvent) {
		void * compile_info;

		omrthread_monitor_enter(jvmtiData->compileEventMutex);
		if (method == NULL) {
			PORT_ACCESS_FROM_VMC(currentThread);

			compile_info = j9mem_allocate_memory(strlen(data->name) + 1, J9MEM_CATEGORY_JVMTI);
			if (compile_info == NULL) {
				goto fail;
			}
			strcpy((char *) compile_info, data->name);
		} else {
			compile_info = data->metaData;
		}
		queueCompileEvent(jvmtiData, methodID, data->startPC, data->length, compile_info, TRUE);
fail:
		omrthread_monitor_exit(jvmtiData->compileEventMutex);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookDynamicCodeLoad);
}

#endif /* INTERP_NATIVE_SUPPORT */


#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static void
jvmtiHookDynamicCodeUnload(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9DynamicCodeUnloadEvent * data = eventData;
	J9JVMTIData * jvmtiData = userData;
	J9VMThread* currentThread = data->currentThread;
	J9Method* method = data->method;
	J9JavaVM * vm = currentThread->javaVM;
	jmethodID methodID = NULL;
	J9JVMTICompileEvent * compEvent;

	Trc_JVMTI_jvmtiHookDynamicCodeUnload_Entry();

	if (method != NULL) {
		methodID = getCurrentMethodID(currentThread, method);
		if (methodID == NULL) {
			goto NO_METHOD_ID;
		}
	}

	if (J9JVMTI_COMPILE_EVENT_THREAD_STATE_DEAD == jvmtiData->compileEventThreadState)	{
		/* CMVC 183352 : This hook may be triggered by freeClassLoader long after CompileEvent Thread died */
		goto NO_COMPILATION_THREAD;
	} else {
		/* Scan the compile event queue for a matching load event */
		omrthread_monitor_enter(jvmtiData->compileEventMutex);
		compEvent = J9_LINKED_LIST_START_DO(jvmtiData->compileEventQueueHead);
		while (compEvent != NULL) {
			if ((compEvent->methodID == methodID) && (compEvent->code_addr == (const void *) data->startPC)) {
				if (methodID == NULL) {
					PORT_ACCESS_FROM_JAVAVM(vm);
					j9mem_free_memory((void *) compEvent->compile_info);
				}
				J9_LINKED_LIST_REMOVE(jvmtiData->compileEventQueueHead, compEvent);
				pool_removeElement(jvmtiData->compileEvents, compEvent);
				goto DONE;
			}
			compEvent = J9_LINKED_LIST_NEXT_DO(jvmtiData->compileEventQueueHead, compEvent);
		}

		/* No load event was removed, so queue the unload event - currently, only queue compiled method unload, since dynamic code unload is not supported */
		if ((methodID != NULL) && (jvmtiData->phase == JVMTI_PHASE_LIVE)) {
			queueCompileEvent(jvmtiData, methodID, data->startPC, 0, NULL, FALSE);
		}
	}

DONE:
	omrthread_monitor_exit(jvmtiData->compileEventMutex);

NO_METHOD_ID:
NO_COMPILATION_THREAD:
	TRACE_JVMTI_EVENT_RETURN(jvmtiHookDynamicCodeUnload);
}

#endif /* INTERP_NATIVE_SUPPORT */

static void
jvmtiHookVmDumpStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMVmDumpStartEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9VMThread* currentThread = data->currentThread;

	jvmtiExtensionEvent callback = *J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_VM_DUMP_START);
	UDATA hadVMAccess;
	UDATA javaOffloadOldState = 0;

	Trc_JVMTI_jvmtiHookVmDumpStart_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookVmDumpStart, j9env);

	/* Call the event callback */
	if (prepareForEvent(j9env, currentThread, currentThread, J9JVMTI_EVENT_COM_IBM_VM_DUMP_START, NULL, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
		J9JavaVM * vm = currentThread->javaVM;

		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		if (callback != NULL) {
			callback((jvmtiEnv *) j9env, data->label, COM_IBM_VM_DUMP_START, data->detail);
		}
		finishedEvent(currentThread, J9JVMTI_EVENT_COM_IBM_VM_DUMP_START, hadVMAccess, javaOffloadOldState);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVmDumpStart);
}


static void
jvmtiHookVmDumpEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMVmDumpEndEvent * data = eventData;
	J9JVMTIEnv * j9env = userData;
	J9VMThread* currentThread = data->currentThread;

	jvmtiExtensionEvent callback = *J9JVMTI_EXTENSION_CALLBACK(j9env, J9JVMTI_EVENT_COM_IBM_VM_DUMP_END);
	UDATA hadVMAccess;
	UDATA javaOffloadOldState = 0;

	Trc_JVMTI_jvmtiHookVmDumpEnd_Entry();

	ENSURE_EVENT_PHASE_LIVE(jvmtiHookVmDumpEnd, j9env);

	/* Call the event callback */
	if (prepareForEvent(j9env, currentThread, currentThread, J9JVMTI_EVENT_COM_IBM_VM_DUMP_END, NULL, &hadVMAccess, TRUE, 0, &javaOffloadOldState)) {
		J9JavaVM * vm = currentThread->javaVM;

		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
		if (callback != NULL) {
			callback((jvmtiEnv *) j9env, data->label, COM_IBM_VM_DUMP_END, data->detail);
		}
		finishedEvent(currentThread, J9JVMTI_EVENT_COM_IBM_VM_DUMP_END, hadVMAccess, javaOffloadOldState);
	}

	TRACE_JVMTI_EVENT_RETURN(jvmtiHookVmDumpEnd);
}
