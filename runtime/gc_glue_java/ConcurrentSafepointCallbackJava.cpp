/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include "ConcurrentSafepointCallbackJava.hpp"
#include "ModronAssertions.h"

MM_ConcurrentSafepointCallbackJava *
MM_ConcurrentSafepointCallbackJava::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentSafepointCallbackJava *callback;

	callback = (MM_ConcurrentSafepointCallbackJava *)env->getForge()->allocate(sizeof(MM_ConcurrentSafepointCallbackJava), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != callback) {
		new(callback) MM_ConcurrentSafepointCallbackJava(env);
		if (!callback->initialize(env)) {
			callback->kill(env);
			return NULL;
		}
	}
	return callback;
}

void
MM_ConcurrentSafepointCallbackJava::kill(MM_EnvironmentBase *env)
{
	if (-1 != _asyncEventKey) {
		J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();
		javaVM->internalVMFunctions->J9CancelAsyncEvent(javaVM, NULL, this->_asyncEventKey);
		javaVM->internalVMFunctions->J9UnregisterAsyncEvent(javaVM, this->_asyncEventKey);
	}

#if defined(AIXPPC) || defined(LINUXPPC)
	if (_cancelAfterGC) {
		J9HookInterface** mmHooks = J9_HOOK_INTERFACE(env->getExtensions()->omrHookInterface);
		(*mmHooks)->J9HookUnregister(mmHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, reportGlobalGCComplete, this);
	}
#endif /* defined(AIXPPC) || defined(LINUXPPC) */

	env->getForge()->free(this);
}

bool
MM_ConcurrentSafepointCallbackJava::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** vmHooks = J9_HOOK_INTERFACE(((J9JavaVM*)env->getLanguageVM())->hookInterface);

	if (NULL == env->getOmrVMThread()) {
		/* Register on hook for VM initialized. We can't register our async callback hooks here
		 * as we have no vmThread at this time so register on VM init hook so we can do so later
		 */
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZED, vmInitialized, OMR_GET_CALLSITE(), this);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, vmTerminating, OMR_GET_CALLSITE(), this);
	} else {
		registerAsyncEventHandler(env, this);
	}

	return true;
}

/**
 * Called when a VM initialization is complete.
 *
 * @param eventNum id for event
 * @param eventData reference to J9VMInitEvent
 * @param userData reference to MM_ConcurrentSafepointCallbackJava
 *
 */
void
MM_ConcurrentSafepointCallbackJava::vmInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	J9VMInitEvent* event = (J9VMInitEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->vmThread->omrVMThread);
	registerAsyncEventHandler(env, (MM_ConcurrentSafepointCallbackJava*) userData);
}

void
MM_ConcurrentSafepointCallbackJava::registerAsyncEventHandler(MM_EnvironmentBase *env, MM_ConcurrentSafepointCallbackJava *callback)
{
	J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();

	/* VM initialization complete so we will now have a vmThread under which to register our Async event handler */
	callback->_asyncEventKey = javaVM->internalVMFunctions->J9RegisterAsyncEvent(javaVM, asyncEventHandler, callback);
}

void
MM_ConcurrentSafepointCallbackJava::asyncEventHandler(J9VMThread *vmThread, intptr_t handlerKey, void *userData)
{
	MM_ConcurrentSafepointCallbackJava *callback  = (MM_ConcurrentSafepointCallbackJava*) userData;
	callback->_handler(vmThread->omrVMThread, callback->_userData);
}

/**
 * Called when a VM is about to terminate.
 *
 * @param eventNum id for event
 * @param eventData reference to JJ9VMShutdownEvent
 * @param userData reference to MM_ConcurrentSafepointCallbackJava
 *
 */
void
MM_ConcurrentSafepointCallbackJava::vmTerminating(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	J9VMShutdownEvent* event = (J9VMShutdownEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->vmThread->omrVMThread);
	J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();

	MM_ConcurrentSafepointCallbackJava *callback = (MM_ConcurrentSafepointCallbackJava*) userData;
	javaVM->internalVMFunctions->J9UnregisterAsyncEvent(javaVM, callback->_asyncEventKey);
}

/**
 * Called when a global collect has completed
 *
 * @param eventNum id for event
 * @param eventData reference to MM_GlobalGCEndEvent
 * @param userData reference to MM_ConcurrentSafepointCallbackJava
 */
void
MM_ConcurrentSafepointCallbackJava::reportGlobalGCComplete(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent *event = (MM_GlobalGCEndEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();

	MM_ConcurrentSafepointCallbackJava *callback = (MM_ConcurrentSafepointCallbackJava*) userData;
	javaVM->internalVMFunctions->J9CancelAsyncEvent(javaVM, NULL, callback->_asyncEventKey);
}

void
#if defined(AIXPPC) || defined(LINUXPPC)
MM_ConcurrentSafepointCallbackJava::registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC)
#else
MM_ConcurrentSafepointCallbackJava::registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData)
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
{
	Assert_MM_true(NULL == _handler);
	Assert_MM_true(NULL == _userData);

	_handler = handler;
	_userData = userData;

#if defined(AIXPPC) || defined(LINUXPPC)
	_cancelAfterGC = cancelAfterGC;
	if (_cancelAfterGC) {
		/* Register hook for global GC end. */
		J9HookInterface** mmHooks = J9_HOOK_INTERFACE(env->getExtensions()->omrHookInterface);
		(*mmHooks)->J9HookRegisterWithCallSite(mmHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, reportGlobalGCComplete, OMR_GET_CALLSITE(), this);
	}
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
}

void
MM_ConcurrentSafepointCallbackJava::requestCallback(MM_EnvironmentBase *env)
{
	Assert_MM_false(NULL == _handler);
	Assert_MM_false(NULL == _userData);

	J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();
	javaVM->internalVMFunctions->J9SignalAsyncEvent(javaVM, (J9VMThread *)env->getLanguageVMThread(), _asyncEventKey);
}

void
MM_ConcurrentSafepointCallbackJava::cancelCallback(MM_EnvironmentBase *env)
{
	J9JavaVM *javaVM = (J9JavaVM*) env->getLanguageVM();
	javaVM->internalVMFunctions->J9CancelAsyncEvent(javaVM, NULL, _asyncEventKey);
}
