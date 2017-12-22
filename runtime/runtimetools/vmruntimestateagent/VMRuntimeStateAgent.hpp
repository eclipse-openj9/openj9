/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
#ifndef RUNTIMETOOLS_VMRUNTIMESTATEAGENT_HPP_
#define RUNTIMETOOLS_VMRUNTIMESTATEAGENT_HPP_

#include "RuntimeToolsAgentBase.hpp"

class VMRuntimeStateAgent: public RuntimeToolsAgentBase
{
private:
	/*
	 * Data members
	 */
	/* pointer to java.lang.Class.getName() */
	jmethodID _getNameMID;

	/* name of the main class */
	char *_appClass;

	/* indicates if J9HOOK_VM_RUNTIME_STATE_CHANGED event is expected to be triggered */
	BOOLEAN _triggerHook;

	/* indicates if J9HOOK_VM_RUNTIME_STATE_CHANGED event has triggered */
	U_32 _hookTriggered;

	/* current VM runtime state */
	U_32 _currentState;

	/* number of idle->active transitions */
	U_32 _idleToActiveCount;

	/* number of active->idle transitions */
	U_32 _activeToIdleCount;

	/* expected number of idle->active transitions */
	U_32 _expectedIdleToActiveCount;

	/* expected number of active->idle transitions */
	U_32 _expectedActiveToIdleCount;

protected:
	/**
	 * Default constructor
	 *
	 * @param vm [in] java vm that can be used by this agent
	 */
	VMRuntimeStateAgent(JavaVM * vm) : RuntimeToolsAgentBase(vm) {}

	/**
	 * This method is used to initialize an instance
	 */
	bool init(void);

public:
	/**
	 * This method should be called to destroy the object and free the associated memory
	 */
	virtual void kill(void);

	jint setup(char * options);

	/**
	 * This method is used to create an instance
	 *
	 * @param vm [in] java vm that can be used by this class
	 */
	static VMRuntimeStateAgent* newInstance(JavaVM * vm);

	/**
	 * This method gets the reference to the agent given the pointer to where it should
	 * be stored. If necessary it creates the agent.
	 *
	 * @param vm    [in] that can be used by the method
	 * @param agent [out] pointer to storage where pointer to agent can be stored
	 */
	static inline VMRuntimeStateAgent* getAgent(JavaVM * vm, VMRuntimeStateAgent** agent){
		if (NULL == *agent) {
			*agent = newInstance(vm);
		}
		return *agent;
	}

	/**
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options [in] string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* option);

	/**
	 * Callback for VMInit event
	 */
	void JNICALL cbVMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);

	/**
	 * Callback for ClassPrepare event
	 */
	void JNICALL cbClassPrepare(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jclass klass);

	/**
	 * Callback for J9HOOK_VM_RUNTIME_STATE_CHANGED event
	 */
	void cbVMRuntimeStateChange(void* eventData);

	/**
	 * Registers hook for J9HOOK_VM_RUNTIME_STATE_CHANGED event
	 */
	void registerHook(void);

	/**
	 * Verifies the results and prints appropriate error/pass messages
	 */
	void verifyResult(void);
};

#endif /* RUNTIMETOOLS_VMRUNTIMESTATEAGENT_HPP_ */
