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
#include "JLMAgent.hpp"
#include "ibmjvmti.h"

#define SWAP_4BYTES(v) (\
					   (((v) << (8*3)) & 0xFF000000) | \
					   (((v) << (8*1)) & 0x00FF0000) | \
					   (((v) >> (8*1)) & 0x0000FF00) | \
					   (((v) >> (8*3)) & 0x000000FF)   \
					   )


#define SWAP_8BYTES(v) (\
						(((v) << (8*7)))  | \
						(((v) << (8*5)) & 0x00FF000000000000ull) | \
						(((v) << (8*3)) & 0x0000FF0000000000ull) | \
						(((v) << (8*1)) & 0x000000FF00000000ull) | \
						(((v) & 0x00000000FF000000ull) >> (8*1)) | \
						(((v) & 0x0000000000FF0000ull) >> (8*3)) | \
						(((v) & 0x000000000000FF00ull) >> (8*5)) | \
						(((v) & 0x00000000000000FFull) >> (8*7)) \
					   )


/* used to store reference to the agent */
static JLMAgent* jlmAgent;

/*********************************************************
 * Methods exported for agent
 */
#ifdef __cplusplus
extern "C" {
#endif

void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	JLMAgent* agent = JLMAgent::getAgent(vm, &jlmAgent);
	agent->shutdown();
	agent->kill();
}

jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	JLMAgent* agent = JLMAgent::getAgent(vm, &jlmAgent);
	return agent->setup(options);
}

jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	JLMAgent* agent = JLMAgent::getAgent(vm, &jlmAgent);
	return  agent->setup(options);
	/* need to add code here that will get jvmti_env and jni env and then call startGenerationThread */
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
bool JLMAgent::init()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_jlmSet        = NULL;
	_jlmDump       = NULL;
	_jlmDumpStats  = NULL;
	_jvmti_env 	   = NULL;
	_lockType      = 3;
	_slowThreshold = 0;
	_clearOnCapture = false;

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void JLMAgent::kill()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm java vm that can be used by this manager
 * @returns an instance of the class
 */
JLMAgent* JLMAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	JLMAgent* obj = (JLMAgent*) j9mem_allocate_memory(sizeof(JLMAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) JLMAgent(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * method that generates the calls to runAction at the appropriate intervals *.
 */
void JLMAgent::runAction()
{
	jint result = JVMTI_ERROR_NONE;
	int           dumpOffset = 8;
	int           dumpSize;
	jlm_dump* 	  dump = NULL;
	char* 		  current;


	/* start by getting an env we can use */
	if (_jvmti_env == NULL){
		result = ((JavaVM *) getJavaVM())->GetEnv((void **) &_jvmti_env, JVMTI_VERSION_1_1);
	}

	/* get the dump data and output */
	result = (_jlmDumpStats)(_jvmti_env, &dump, COM_IBM_JLM_DUMP_FORMAT_TAGS);
	if ( result == JVMTI_ERROR_NONE ) {
		dumpSize = (int)(dump->end - dump->begin) - dumpOffset;

		message("DUMP BEGIN:");
		messageUDATA(_lockType);
		message("\n");
		current = (char*)(dump->begin) +  dumpOffset;
		while(current < (char*)dump->end){
			if ((*current & _lockType)&&
				((_slowThreshold==0)||( SWAP_4BYTES(*((U_32*)(current+6))) > _slowThreshold ))){
				/* monitor type */
				messageUDATA(*((char*)current++));
				message(":");

				/* held */
				messageUDATA(*((char*)current++));
				message(":");

				/* enter count */
				messageUDATA(SWAP_4BYTES(*((U_32*)current)));
				current = current + 4;
				message(":");

				/* slow count */
				message("*");
				messageUDATA(SWAP_4BYTES(*((U_32*)current)));
				current = current + 4;
				message(":");

				/* recursive count */
				messageUDATA(SWAP_4BYTES(*((U_32*)current)));
				current = current + 4;
				message(":");

				/* spin2 count */
				messageUDATA(SWAP_4BYTES(*((U_32*)current)));
				current = current + 4;
				message(":");

				/* yield count */
				messageUDATA(SWAP_4BYTES(*((U_32*)current)));
				current = current + 4;
				message(":");

				/* hold time */
				messageU64(SWAP_8BYTES(*((U_64*)current)));
				current = current + 8;
				message(":");

				/* object Id */
				current = current + 8;

				message(current);
				message("\n");
				current = current + strlen(current) + 1;
			} else {
				current = current + 2 + 5*4 + 2*8;
				current = current + strlen(current) + 1;
			}
		}
		message("DUMP END\n\n\n");

		if (_clearOnCapture){
			(_jlmSet)(_jvmti_env, COM_IBM_JLM_STOP_TIME_STAMP);
			(_jlmSet)(_jvmti_env, COM_IBM_JLM_START_TIME_STAMP);
		}

		/* free the buffer allocated when we requested the JLM data */
		_jvmti_env->Deallocate((unsigned char*)dump);
	}
}

/**
 * This method should be called when Agent_OnLoad is invoked by the JVM.  It will parse the
 * options that it understands and then call setupAgentSpecific
 *
 * @param vm a JavaVM that the agent can use
 * @param options the options passed to the agent in Agent_OnLoad
 */
jint JLMAgent::setup(char * options)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	jint                         	extensionCount;
	jvmtiExtensionFunctionInfo* 	extensionFunctions;
	jint 							result = JVMTI_ERROR_NONE;
	int i = 0;

	/* do setup from parent classes */
	result = RuntimeToolsIntervalAgent::setup(options);
	if (result != 0){
		return result;
	}

	/* start by getting an env we can use */
	if (_jvmti_env == NULL){
		result = ((JavaVM *) getJavaVM())->GetEnv((void **) &_jvmti_env, JVMTI_VERSION_1_1);
	}

	/* Find the extended functions and store for later use*/
	result = _jvmti_env->GetExtensionFunctions(&extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != result ) {
		error("Failed GetExtensionFunctions\n");
	}

	if (result == JVMTI_ERROR_NONE){
		for (i = 0; i < extensionCount; i++) {
			if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_JLM) == 0) {
				_jlmSet = extensionFunctions[i].func;
			}
			if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_JLM_DUMP) == 0) {
				_jlmDump = extensionFunctions[i].func;
			}
			if (strcmp(extensionFunctions[i].id, COM_IBM_JLM_DUMP_STATS) == 0) {
				_jlmDumpStats = extensionFunctions[i].func;
			}
		}

		result = _jvmti_env->Deallocate((unsigned char*)extensionFunctions);
		if (result != JVMTI_ERROR_NONE) {
			error("Failed to Deallocate extension functions\n");
		}

		/* validate that we loaded the functions correctly */
		if (_jlmSet == NULL) {
			error("COM_IBM_SET_VM_JLM extension was not found\n");
			result = JNI_ERR;
		}

		if (_jlmDump == NULL) {
			error("COM_IBM_SET_VM_JLM_DUMP extension was not found\n");
			result = JNI_ERR;
		}

		if (_jlmDumpStats == NULL) {
			error("COM_IBM_JLM_DUMP_STATS was not found\n");
			result = JNI_ERR;
		}
	}

	/* enabled JLM if no errors occurred */
	if (result == JVMTI_ERROR_NONE){
		result = (_jlmSet)(_jvmti_env, COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE){
			error("Could not enable JLM with timestamps");
		}
	}
	return result;
}

/**
 * This is called once for each argument, the agent should parse the arguments
 * it supports and pass any that it does not understand to parent classes
 *
 * @param options string containing the argument
 *
 * @returns one of the AGENT_ERROR_ values
 */
jint JLMAgent::parseArgument(char* option)
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());
	jint rc = AGENT_ERROR_NONE;

	/* parse any options */
	if (try_scan(&option,"type:")){
		if (1 != sscanf(option,"%d",&_lockType)){
			error("invalid lock type value passed to agent - invalid format\n");
			return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
		} else {
			if (_lockType >3){
				error("invalid lock type passed to agent\n");
				return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
			}
		}
	} else if (try_scan(&option,"slowThreshold:")){
			if (1 != sscanf(option,"%d",&_slowThreshold)){
				error("invalid slow threshold value passed to agent - invalid format\n");
				return AGENT_ERROR_INVALID_ARGUMENT_VALUE;
			}
	} else if (try_scan(&option,"clearOnCapture")){
		_clearOnCapture = true;
	}else {
		rc = RuntimeToolsIntervalAgent::parseArgument(option);
	}

	return rc;
}
