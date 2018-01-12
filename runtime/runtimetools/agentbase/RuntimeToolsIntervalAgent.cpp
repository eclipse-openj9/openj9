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
#include "RuntimeToolsIntervalAgent.hpp"

/**
 * C method for thread started to call runAction at appropriate interval
 */
static IDATA J9THREAD_PROC
intervalThread(RuntimeToolsIntervalAgent* agent){
	 agent->generateRunActionCalls();
	 return 0;
}

/************************************************************************
 * Methods for object life-cyle management
 *
 */


/**
 * This method is used to initialize an instance
 */
bool RuntimeToolsIntervalAgent::init()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsAgentBase::init()){
		return false;
	}
	_interval = 0;
	_done = false;

	/* create monitor used to sync with thread which calls runAction at the appropriate interval */
	if (0 != omrthread_monitor_init_with_name(&_monitor, 0, "run action monitor")){
		error("Failed to create monitor for Interval agent");
		return false;
	}

	return true;
}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void RuntimeToolsIntervalAgent::kill()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());



	j9mem_free_memory(this);
}

/**
 * method that generates the calls to runAction at the appropriate intervals *.
 */
void RuntimeToolsIntervalAgent::generateRunActionCalls()
{
	IDATA waitResult = 0;
	JavaVMAttachArgs threadArgs;

	memset(&threadArgs, 0, sizeof(threadArgs));
	threadArgs.version = JNI_VERSION_1_6;
	threadArgs.name = (char *)"interval generation thread";
	threadArgs.group = NULL;

	/* attach the thread so we are visible to the vm and we can run JNI if needed */
	if (0 == ((JavaVM *)getJavaVM())->AttachCurrentThreadAsDaemon((void**)&_env, (void*)&threadArgs)) {
	} else {
		error("failed to attach interval generator thread ");
	}

	omrthread_monitor_enter(_monitor);
	runAction();
	while(TRUE){
		waitResult = omrthread_monitor_wait_timed(_monitor, _interval, 0);
		if (J9THREAD_TIMED_OUT == waitResult){
			runAction();
		} else {
			if (TRUE == _done){
				runAction();
				break;
			}
		}
	}
	omrthread_monitor_exit(_monitor);
	omrthread_monitor_destroy(_monitor);

}

/**
 * This method is used to create an instance
 *
 * @param vm java vm that can be used by this manager
 */
RuntimeToolsIntervalAgent* RuntimeToolsIntervalAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	RuntimeToolsIntervalAgent* obj = (RuntimeToolsIntervalAgent*) j9mem_allocate_memory(sizeof(RuntimeToolsIntervalAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) RuntimeToolsIntervalAgent(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}


/**
 * Called them the vm starts
 * @param jvmti_env jvmti_evn that can be used by the agent
 * @param env JNIEnv that can be used by the agent
 */
void RuntimeToolsIntervalAgent::vmStart(jvmtiEnv *jvmti_env,JNIEnv* env)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	omrthread_t generatorThread = NULL;
	IDATA createrc = 0;

	if (_outputFileName != NULL){
		_outputFileNameFD = j9file_open(_outputFileName , EsOpenWrite | EsOpenCreate | EsOpenAppend  , 0666);
		if(-1 == _outputFileNameFD){
			error("Failed to open output file\n");
			return ;
		}
	}

	/* create a new thread which will wake up and and call runAction at the appropriate interval */
	RuntimeToolsIntervalAgent* agent = this;
	J9JavaVM *vm = getJavaVM();
	createrc = vm->internalVMFunctions->createThreadWithCategory(
				&generatorThread,
				0,
				J9THREAD_PRIORITY_NORMAL,
				0,
				(omrthread_entrypoint_t) intervalThread,
				agent,
				J9THREAD_CATEGORY_APPLICATION_THREAD);

	if (0 != createrc) {
		error("Failed to start runAction thread\n");
		return;
	}
}

/**
 * Called them the vm stops
 * @param jvmti_env jvmti_evn that can be used by the agent
 * @param env JNIEnv that can be used by the agent
 */
void RuntimeToolsIntervalAgent::vmStop(jvmtiEnv *jvmti_env,JNIEnv* env)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	if (-1 != _outputFileNameFD){
		j9file_close(_outputFileNameFD);
	}
	omrthread_monitor_enter(_monitor);
	_done = TRUE;
	omrthread_monitor_notify(_monitor);
	omrthread_monitor_exit(_monitor);
}

/**
 * This is called once for each argument, the agent should parse the arguments
 * it supports and pass any that it does not understand to parent classes
 *
 * @param options string containing the argument
 *
 * @returns one of the AGENT_ERROR_ values
 */
jint RuntimeToolsIntervalAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	/* parse any options */
	if (try_scan(&option,"Interval:")){
		/* time is in milliseconds */
		if (1 != sscanf(option,"%d",&_interval)){
			error("invalid interval value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else {
		rc = RuntimeToolsAgentBase::parseArgument(option);
	}

	return rc;
}


