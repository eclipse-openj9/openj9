/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "VMRuntimeStateAgent.hpp"

/* used to store reference to the agent which is then used by C callbacks registered
 * with JVMTI
 */
static VMRuntimeStateAgent *runtimeStateAgent;

/*********************************************************
 * Methods exported for agent
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * callback invoked by JVMTI when the agent is unloaded
 * @param vm [in] jvm that can be used by the agent
 */
void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	VMRuntimeStateAgent *agent = VMRuntimeStateAgent::getAgent(vm, &runtimeStateAgent);
	agent->shutdown();
	agent->kill();
}

/**
 * callback invoked by JVMTI when the agent is loaded
 * @param vm       [in] jvm that can be used by the agent
 * @param options  [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	VMRuntimeStateAgent *agent = VMRuntimeStateAgent::getAgent(vm, &runtimeStateAgent);
	return agent->setup(options);
}

void JNICALL
vmInitWrapper(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread)
{
	if (NULL != runtimeStateAgent) {
		runtimeStateAgent->cbVMInit(jvmti_env, jni_env, thread);
	}
}

void JNICALL
classPrepareWrapper(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jclass klass)
{
	if (NULL != runtimeStateAgent) {
		runtimeStateAgent->cbClassPrepare(jvmti_env, jni_env, thread, klass);
	}
}

void JNICALL
vmDeathWrapper(jvmtiEnv *jvmti_env, JNIEnv* jni_env)
{
	if (NULL != runtimeStateAgent) {
		runtimeStateAgent->verifyResult();
	}
}

void
vmRuntimeStateChangeWrapper(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	VMRuntimeStateAgent *agent = (VMRuntimeStateAgent *)userData;
	agent->cbVMRuntimeStateChange(eventData);
}

#ifdef __cplusplus
}
#endif

/*
 * Start of class methods
 *
 */
jint
VMRuntimeStateAgent::setup(char * options)
{
	jvmtiEnv * jvmti_env = NULL;

	/* parse any options */
	jint rc = parseArguments(options);
	if (AGENT_ERROR_NONE != rc ){
		return rc;
	}

	/* start by getting an env we can use */
	rc = _vm->GetEnv((void **) &jvmti_env, JVMTI_VERSION_1_1);
	if (JNI_OK != rc) {
		if ((JNI_EVERSION != rc) || JNI_OK != ((rc = _vm->GetEnv((void **) &jvmti_env, JVMTI_VERSION_1_0)))) {
			error("Failed to GetEnv\n");
			return AGENT_ERROR_FAILED_TO_GET_JNI_ENV;
		}
	}

	/* set and enable the callbacks */
	memset(&_callbacks, 0, sizeof(jvmtiEventCallbacks));
	_callbacks.VMInit = vmInitWrapper;
	_callbacks.ClassPrepare = classPrepareWrapper;
	_callbacks.VMDeath = vmDeathWrapper;

	rc = jvmti_env->SetEventCallbacks(&_callbacks, sizeof(jvmtiEventCallbacks));

	if (JVMTI_ERROR_NONE == rc){
		rc = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
	}
	if (JVMTI_ERROR_NONE == rc){
		rc = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
	}
	if (JVMTI_ERROR_NONE == rc){
		rc = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
	}
	if (JVMTI_ERROR_NONE != rc){
		return AGENT_ERROR_COULD_NOT_SET_REQ_EVENTS;
	}

	return AGENT_ERROR_NONE;
}

/**
 * This method is used to initialize an instance
 * @returns true if initialization was successful
 */
bool
VMRuntimeStateAgent::init(void)
{
	jbyte counter = 0;
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsAgentBase::init()) {
		return false;
	}

	_getNameMID = NULL;
	_appClass = NULL;
	_triggerHook = TRUE;
	_hookTriggered = 0;
	_idleToActiveCount = 0;
	_activeToIdleCount = 0;

	return true;
}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void
VMRuntimeStateAgent::kill(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	if (NULL != _appClass) {
		j9mem_free_memory(_appClass);
	}
	j9mem_free_memory(this);
}

/**
 * This method is used to create an instance
 *
 * @param vm [in] java vm that can be used by this manager
 * @returns an instance of the class
 */
VMRuntimeStateAgent*
VMRuntimeStateAgent::newInstance(JavaVM * vm)
{
	J9JavaVM *javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	VMRuntimeStateAgent *obj = (VMRuntimeStateAgent*) j9mem_allocate_memory(sizeof(VMRuntimeStateAgent), OMRMEM_CATEGORY_VM);

	if (NULL != obj) {
		new (obj) VMRuntimeStateAgent(vm);
		if (!obj->init()) {
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * This is called once for each argument, the agent should parse the arguments
 * it supports and pass any that it does not understand to parent classes
 *
 * @param options string containing the argument
 *
 * @returns one of the AGENT_ERROR_ values
 */
jint
VMRuntimeStateAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	if (strlen(option) == 0) {
		return AGENT_ERROR_INVALID_ARGUMENT;
	}
	if (try_scan(&option, "appClass:")) {
		_appClass = (char *)j9mem_allocate_memory(strlen(option) + 1, OMRMEM_CATEGORY_VM);
		if (NULL == _appClass) {
			errorFormat("failed to allocate memory\n");
			return AGENT_ERROR_FAILED_TO_ALLOCATE_MEMORY;
		}
		strcpy(_appClass, option);
	} else if (try_scan(&option, "triggerHook:")) {
		if (!strcmp(option, "no")) {
			_triggerHook = FALSE;
		} else if (!strcmp(option, "yes")) {
			_triggerHook = TRUE;
		} else {
			errorFormat("invalid value of argument triggerHook passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else {
		rc = RuntimeToolsAgentBase::parseArgument(option);
	}

	return rc;
}

void JNICALL
VMRuntimeStateAgent::cbVMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread)
{
	J9JavaVM *javaVM = getJavaVM();

	registerHook();

	_currentState = javaVM->internalVMFunctions->getVMRuntimeState(javaVM);
}

void JNICALL
VMRuntimeStateAgent::cbClassPrepare(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jclass klass)
{
	jfieldID idleToActiveFieldID;
	jfieldID activeToIdleFieldID;
	jstring className = NULL;
	const char *classNameUTF = NULL;
	IDATA rc = 0;
	jvmtiPhase phase = JVMTI_PHASE_ONLOAD;

        jvmti_env->GetPhase(&phase);
        if (JVMTI_PHASE_LIVE != phase) {
                return;
        }

        if (NULL == _getNameMID) {
                jclass classLocalRef = jni_env->FindClass("java/lang/Class");
                if (NULL == classLocalRef) {
                        errorFormat("%s:%d ERROR: Failed to find java/lang/Class\n", __FILE__, __LINE__);
                        return;
                }
                _getNameMID = jni_env->GetMethodID(classLocalRef, "getName", "()Ljava/lang/String;");
                if (NULL == _getNameMID) {
                        errorFormat("%s:%d ERROR: Failed to get methodID for Class.getName()\n", __FILE__, __LINE__);
                        return;
                }
        }

	className = (jstring) jni_env->CallObjectMethod(klass, _getNameMID);
	if (JNI_TRUE == jni_env->ExceptionCheck()) {
		errorFormat("%s:%d ERROR: Failed to get name of the class\n", __FILE__, __LINE__);
		return;
	}
	classNameUTF = jni_env->GetStringUTFChars(className, NULL);
	if (NULL == classNameUTF) {
		errorFormat("%s:%d ERROR: Failed to get UTF-8 characters from j.l.String object\n", __FILE__, __LINE__);
		return;
	}

	rc = strcmp(classNameUTF, _appClass);
	jni_env->ReleaseStringUTFChars(className, classNameUTF);

	if (0 != rc) {
		return;
	}

	idleToActiveFieldID = jni_env->GetStaticFieldID(klass, "idleToActiveCount", "I");
	if (NULL == idleToActiveFieldID) {
		errorFormat("%s:%d ERROR: Failed to find field idleToActiveCount in class %s\n", __FILE__, __LINE__, _appClass);
		return;
	}

	activeToIdleFieldID = jni_env->GetStaticFieldID(klass, "activeToIdleCount", "I");
	if (NULL == activeToIdleFieldID) {
		errorFormat("%s:%d ERROR: Failed to find field activeToIdleFieldID in class %s\n", __FILE__, __LINE__, _appClass);
		return;
	}

	_expectedIdleToActiveCount = jni_env->GetStaticIntField(klass, idleToActiveFieldID);
	_expectedActiveToIdleCount = jni_env->GetStaticIntField(klass, activeToIdleFieldID);
}

void
VMRuntimeStateAgent::cbVMRuntimeStateChange(void* eventData)
{
	J9JavaVM *vm = getJavaVM();
        J9VMRuntimeStateChanged *j9vmState = (J9VMRuntimeStateChanged *) eventData;
	U_32 newState = vm->internalVMFunctions->getVMRuntimeState(vm);

	_hookTriggered += 1;
        if (J9VM_RUNTIME_STATE_IDLE == _currentState) {
		if (J9VM_RUNTIME_STATE_ACTIVE != newState) {
			errorFormat("%s:%d ERROR : Expected VM runtime state %s, found %s\n", J9VM_RUNTIME_STATE_IDLE, newState);
		} else {
			_idleToActiveCount += 1;
		}
        } else {
		// currentState must be J9VM_RUNTIME_STATE_ACTIVE
		if (J9VM_RUNTIME_STATE_IDLE != newState) {
			errorFormat("%s:%d ERROR : Expected VM runtime state %s, found %s\n",J9VM_RUNTIME_STATE_ACTIVE, newState);
		} else {
			_activeToIdleCount += 1;
		}
	}
	_currentState = newState;
}

void
VMRuntimeStateAgent::registerHook(void)
{
	J9JavaVM * vm = getJavaVM();
	J9HookInterface **vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);

	if (NULL != vmHooks) {
		IDATA rc = (*vmHooks)->J9HookRegister(vmHooks, J9HOOK_VM_RUNTIME_STATE_CHANGED, vmRuntimeStateChangeWrapper, this);
		if (0 != rc) {
			errorFormat("%s:%d ERROR: Failed to register for J9HOOK_VM_RUNTIME_STATE_CHANGED hook\n", __FILE__, __LINE__);
		}
	}
}

void
VMRuntimeStateAgent::verifyResult(void)
{
	BOOLEAN errorOccurred = FALSE;

	if (_triggerHook) {
		if (_hookTriggered > 0) {
			message("PASS: J9HOOK_VM_RUNTIME_STATE_CHANGED hook is triggered\n");

			if (_idleToActiveCount == _expectedIdleToActiveCount) {
				message("PASS: Idle to Active transitions are as expected\n");
			} else {
				errorFormat("ERROR: Expected idle to active transitions: %d, found: %d\n", _expectedIdleToActiveCount, _idleToActiveCount);
				errorOccurred = TRUE;
			}
			if (_activeToIdleCount == _expectedActiveToIdleCount) {
				message("PASS: Active to Idle transitions are as expected\n");
			} else {
				errorFormat("ERROR: Expected active to idle transitions: %d, found: %d\n", _expectedActiveToIdleCount, _activeToIdleCount);
				errorOccurred = TRUE;
			}
		} else {
			errorFormat("ERROR: Expected J9HOOK_VM_RUNTIME_STATE_CHANGED to be triggered\n");
			errorOccurred = TRUE;
		}
	} else {
		if (0 == _hookTriggered) {
			message("PASS: J9HOOK_VM_RUNTIME_STATE_CHANGED hook is not triggered\n");
		} else {
			errorFormat("ERROR: Did not expect J9HOOK_VM_RUNTIME_STATE_CHANGED to be triggered\n");
			errorOccurred = TRUE;
		}
	}

	if (!errorOccurred) {
		message("All Tests Completed and Passed\n");
	}
}
