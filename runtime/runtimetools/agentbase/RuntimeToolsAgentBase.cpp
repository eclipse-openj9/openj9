/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
#include "j9port.h"
#include "j9memcategories.h"
#include "RuntimeToolsAgentBase.hpp"

/* wrappers for jvmti callbacks which invoke the appropriate method on the agent instance */
extern "C" {
static void JNICALL
vmStartWrapper(jvmtiEnv *jvmti_env,JNIEnv* env)
{
	RuntimeToolsAgentBase* agent;
	jvmtiError result = jvmti_env->GetEnvironmentLocalStorage((void**) &agent);
	if (NULL == agent) {
		fprintf(stderr, "ERROR: Failed to get pointer to agent via environment-local storage\n");
		return;
	}
	agent->vmStart(jvmti_env,env);
}

static void JNICALL
vmStopWrapper(jvmtiEnv *jvmti_env,JNIEnv* env)
{
	RuntimeToolsAgentBase* agent;
	jvmtiError result = jvmti_env->GetEnvironmentLocalStorage((void**) &agent);
	if (NULL == agent) {
		fprintf(stderr, "ERROR: Failed to get pointer to agent via environment-local storage\n");
		return;
	}
	agent->vmStop(jvmti_env,env);
}
}
/************************************************************************
 * Methods for object life-cyle management
 *
 */


/**
 * This method is used to initialize an instance
 */
bool RuntimeToolsAgentBase::init()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	_outputFileName = NULL;
	_outputFileNameFD = -1;

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void RuntimeToolsAgentBase::kill()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	j9mem_free_memory(this);
}

/**
 * This method is used to create an instance
 *
 * @param vm java vm that can be used by this manager
 */
RuntimeToolsAgentBase* RuntimeToolsAgentBase::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	RuntimeToolsAgentBase* obj = (RuntimeToolsAgentBase*) j9mem_allocate_memory(sizeof(RuntimeToolsAgentBase), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) RuntimeToolsAgentBase(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * This method should be called when Agent_OnLoad is invoked by the JVM.  It will parse the
 * options that it understands and then call setupAgentSpecific
 *
 * @param vm a JavaVM that the agent can use
 * @param options the options passed to the agent in Agent_OnLoad
 */
jint RuntimeToolsAgentBase::setup(char * options)
{
	jint rc = JVMTI_ERROR_NONE;
	jvmtiEnv * jvmti_env = NULL;
	char* currentOption = options;

	/* parse any options */
	rc = parseArguments(options);
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

	/*
	 * this this object instance as part of the local storage so that we have access to it on
	 * later calls
	 */
	RuntimeToolsAgentBase* agent = this;
	rc = jvmti_env->SetEnvironmentLocalStorage((void*)this);

	if (JVMTI_ERROR_NONE != rc){
		return AGENT_ERROR_COULD_NOT_SET_LOCAL_STORAGE;
	}

	/* set and enable the the vmstart and stop callbacks */
	memset(&_callbacks,0,sizeof(jvmtiEventCallbacks));
	_callbacks.VMStart = vmStartWrapper;
	_callbacks.VMDeath = vmStopWrapper;
	rc = jvmti_env->SetEventCallbacks(&_callbacks,sizeof(jvmtiEventCallbacks));

	if (JVMTI_ERROR_NONE == rc){
		rc = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_VM_START,NULL);
	}
	if (JVMTI_ERROR_NONE == rc){
		rc = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_VM_DEATH,NULL);
	}

	if (JVMTI_ERROR_NONE != rc){
		return AGENT_ERROR_COULD_NOT_SET_REQ_EVENTS;
	}

	return AGENT_ERROR_NONE;
}

/**
 * Called to parse the arguments passed to the agent
 * @param options string containing the options
 * @returns one of the AGENT_ERROR_ values
 */
jint RuntimeToolsAgentBase::parseArguments(char* options)
{
	char* nextArg = NULL;
	char result = AGENT_ERROR_NONE;

	/* split up the arguments using the command separator and call parseArgument for each one */
	if (options != NULL){
		nextArg = strtok(options,ARGUMENT_SEPARATOR_STRING);
		if (nextArg != NULL){
			while(nextArg != NULL){
				result = parseArgument(nextArg);
				if (result != AGENT_ERROR_NONE){
					break;
				}
				nextArg = strtok(NULL,ARGUMENT_SEPARATOR_STRING);
			}
		} else {
			result = parseArgument(options);
		}
	}
	return result;
}

/**
 * Called to parse the arguments passed to the agent
 * @param options string containing the options
 * @returns one of the AGENT_ERROR_ values
 */
jint RuntimeToolsAgentBase::parseArgument(char* option){
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;
	if (strcmp(option,"")!= 0){
		if (try_scan(&option,"file:")){
			int length = (int) strlen(option)+1;
			_outputFileName=(char*) j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
			if (_outputFileName == NULL){
				error("invalid output file name passed to agent\n");
				return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
			}
			memset(_outputFileName,0,length);
			memcpy(_outputFileName,option,length);
		} else{
			error("invalid option passed to agent:");
			error(option);
			error("\n");
			rc = AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	}
	return rc;
}
