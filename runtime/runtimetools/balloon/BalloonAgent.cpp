/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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

#include "BalloonAgent.hpp"

/* used to store reference to the agent which is then used by C callbacks registered
 * with JVMTI
 */
static BalloonAgent* balloonAgent;

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
	BalloonAgent* agent = BalloonAgent::getAgent(vm, &balloonAgent);
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
	BalloonAgent* agent = BalloonAgent::getAgent(vm, &balloonAgent);
	return agent->setup(options);
}

/**
 * callback invoked when an attach is made on the jvm
 * @param vm       [in] jvm that can be used by the agent
 * @param options  [in] options specified on command line for agent
 * @param reserved [in/out] reserved for future use
 */
jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	BalloonAgent* agent = BalloonAgent::getAgent(vm, &balloonAgent);
	return  agent->setup(options);
	/* need to add code here that will get jvmti_env and jni env and then call startGenerationThread
	 * currently the agent does not support late attach
	 */
}

#ifdef __cplusplus
}
#endif

/************************************************************************
 * Start of class methods
 *
 */

/**
 * This method is used to initialize an instance
 * @returns true if initialization was successful
 */
bool
BalloonAgent::init(void)
{
	jbyte counter = 0;
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()) {
		return false;
	}

	_balloonSize = 0;
	_balloonDelay = 0;
	_balloonInterval = 0;
	_holdTime = 0;


	/* initialize the buffer used to set the bytes in each of the balloon objects
	 * we don't want them to be 0 as otherwise compression or other optimization techniques may be
	 * able to easily eliminate them
	 */
	for (int i=0;i<NUMBER_OF_BYTES_IN_1M;i++){
		initializeBuffer[i] = counter;
		counter++;
	}

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void
BalloonAgent::kill(void)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm [in] java vm that can be used by this manager
 * @returns an instance of the class
 */
BalloonAgent*
BalloonAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	BalloonAgent* obj = (BalloonAgent*) j9mem_allocate_memory(sizeof(BalloonAgent), OMRMEM_CATEGORY_VM);
	if (NULL != obj) {
		new (obj) BalloonAgent(vm);
		if (!obj->init()) {
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * method that generates the calls to runAction at the appropriate intervals
 */
void
BalloonAgent::runAction(void)
{
	if (0 != _balloonSize) {
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());
		J9JavaVM *vm =getJavaVM();

		jobject* allocatedArrays = (jobject*) j9mem_allocate_memory(sizeof(jobject)*MAX_BALLOON_SIZE, OMRMEM_CATEGORY_VM);
		if (0 == allocatedArrays ) {
			message("BalloonAgent: Failed to allocate array for balloon\n");
			_balloonSize = 0;
			return;
		}

		/* wait for the requested delay */
		if (0 != _balloonDelay) {
			omrthread_sleep(_balloonDelay);
		}

		/* now inflate the balloon */
		message("BalloonAgent: Starting inflation\n");
		memset(allocatedArrays, 0, sizeof(jobject)*MAX_BALLOON_SIZE);
		for (int i=0;i<_balloonSize;i++){
			/* balloon size is in M, validate that we have not already inflated bigger than the heap can possibly go */
			if ((vm->memoryManagerFunctions->j9gc_heap_total_memory(vm)/NUMBER_OF_BYTES_IN_1M) < (UDATA) _balloonSize) {
				allocatedArrays[i] = getEnv()->NewByteArray(NUMBER_OF_BYTES_IN_1M);
				if (NULL == allocatedArrays[i]) {
					message("Failed to allocate object for balloon:");
					messageUDATA((UDATA) i);
					message("\n");
					break;
				}
				getEnv()->SetByteArrayRegion((jbyteArray)(allocatedArrays[i]),0,NUMBER_OF_BYTES_IN_1M,initializeBuffer);

				if (0 != _balloonInterval) {
					omrthread_sleep(_balloonInterval);
				}
			} else {
				break;
			}
		}

		/* wait for the requested delay */
		if (0 != _holdTime) {
			omrthread_sleep(_holdTime);
		}

		/* ok now deflate the balloon */
		message("BalloonAgent: Starting deflation\n");
		for (int i=0; i<_balloonSize; i++){
			if (NULL != allocatedArrays[i]) {
				getEnv()->DeleteLocalRef(allocatedArrays[i]);
				if (0 != _balloonInterval) {
					omrthread_sleep(_balloonInterval);
				}
			} else {
				break;
			}
		}

		j9mem_free_memory(allocatedArrays);
		message("BalloonAgent: Deflation complete\n");
	}

	/* we only want to inflate/deflate once */
	_balloonSize = 0;
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
BalloonAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	/* check for threshHold option */
	if (try_scan(&option,"size:")) {
		if (1 != sscanf(option,"%d",&_balloonSize)) {
			error("invalid balloon size value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if ((_balloonSize > MAX_BALLOON_SIZE)||(_balloonSize <0)) {
			error("balloon size larger than max allowed or less than zero\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"delay:")) {
		if (1 != sscanf(option,"%d",&_balloonDelay)) {
			error("invalid balloon delay value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_balloonDelay < 0) {
			error("negative balloon delay is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"sleeptime:")) {
		if (1 != sscanf(option,"%d",&_balloonInterval)) {
			error("invalid balloon interval value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_balloonInterval < 0) {
			error("negative balloon interval is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	} else if (try_scan(&option,"holdtime:")) {
		if (1 != sscanf(option,"%d",&_holdTime)) {
			error("invalid balloon holdtime value passed to agent\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else if (_holdTime < 0) {
			error("negative balloon holdtime is invalid\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		}
	}
	else {
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}
