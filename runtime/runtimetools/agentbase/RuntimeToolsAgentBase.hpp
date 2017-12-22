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
#ifndef RUNTIMETOOLS_AGENTBASE_HPP_
#define RUNTIMETOOLS_AGENTBASE_HPP_

#include "j9protos.h"
#include "j9comp.h"
#include "jvmti.h"
#include "string.h"

#define AGENT_ERROR_NONE 0
#define AGENT_ERROR_INVALID_ARGUMENT 1
#define AGENT_ERROR_INVALID_ARGUMENT_VALUE 2
#define AGENT_ERROR_COULD_NOT_SET_REQ_EVENTS 3
#define AGENT_ERROR_FAILED_TO_GET_JNI_ENV 4
#define AGENT_ERROR_MISSING_ARGUMENT 5
#define AGENT_ERROR_COULD_NOT_SET_LOCAL_STORAGE 6
#define AGENT_ERROR_FAILED_TO_ALLOCATE_MEMORY 7

#define ARGUMENT_SEPARATOR_STRING ","

class RuntimeToolsAgentBase
{
private:

protected:
	/* java vm that will be available to all agents */
	JavaVM * _vm;

	/* store the filename that output may be sent to */
	char* _outputFileName;
	IDATA _outputFileNameFD;

	/**
	 * This redefines the new operator for this class and subclasses
	 *
	 * @param size the size of the object
	 * @param the pointer to the object
	 * @returns a pointer to the object
	 */
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	RuntimeToolsAgentBase(JavaVM * vm) {_vm = vm;}

	/**
	 * This method is used to initialize an instance
	 */
	bool init();

	/**
	 * pointers to callbacks for vm start/stop
	 */
	jvmtiEventCallbacks _callbacks;

public:
	/**
	 * This method should be called to destroy the object and free the associated memory
	 */
	virtual void kill();

	/**
	 * This method is used to create an instance
	 * @param vm java vm that can be used by this manager
	 */
	static RuntimeToolsAgentBase* newInstance(JavaVM * vm);

	/**
	 * returns pointer to J9JavaVM
	 */
	inline J9JavaVM * getJavaVM() { return ((J9InvocationJavaVM *)_vm)->j9vm; }

	/**
	 * This method should be called when Agent_OnLoad is invoked by the JVM.  It will parse the
	 * options that it understands and then call the same method on its parent
	 *
	 * @param options the options passed to the agent in Agent_OnLoad
	 */
	virtual jint setup(char * options);

	/**
	 * This method should be called when Agent_OnUnLoad is invoked by the JVM.
	 */
	virtual void shutdown(){};

	/**
	 * Helper to output error messages
	 * @param string the string containing the messages to be output
	 */
	virtual void error(const char* string)
	{
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());

		j9tty_printf(PORTLIB, string);
	}

	/**
	 * Helper to output error messages
	 * @param string the string containing the messages to be output
	 */
	virtual void errorFormat(const char* format, ...)
	{
		va_list args;
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());

		va_start(args, format);
		j9tty_vprintf(format, args);
		va_end(args);
	}

	/**
	 * Helper to output information messages
	 * @param string the string containing the messages to be output
	 */
	virtual void message(const char* string)
	{
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());
		if (_outputFileNameFD != -1){
			j9file_printf(PORTLIB,_outputFileNameFD, string);
		} else {
			j9tty_printf(PORTLIB, string);
		}
	}

	/**
	 * Helper to output information messages
	 * @param string the string containing the messages to be output
	 */
	virtual void messageUDATA(UDATA val)
	{
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());
		if (_outputFileNameFD != -1){
					j9file_printf(PORTLIB,_outputFileNameFD, "%zu",val);
		} else {
			j9tty_printf(PORTLIB, "%zu",val);
		}
	}


	/**
	 * Helper to output information messages
	 * @param string the string containing the messages to be output
	 */
	virtual void messageU64(U_64 val)
	{
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());
		if (_outputFileNameFD != -1){
					j9file_printf(PORTLIB,_outputFileNameFD, "%llu",val);
		} else {
			j9tty_printf(PORTLIB, "%llu",val);
		}
	}

	/**
	 * Helper to output information messages
	 * @param string the string containing the messages to be output
	 */
	virtual void messageIDATA(IDATA val)
	{
		PORT_ACCESS_FROM_JAVAVM(getJavaVM());
		if (_outputFileNameFD != -1){
			j9file_printf(PORTLIB,_outputFileNameFD, "%zd",val);
		} else {
			j9tty_printf(PORTLIB, "%zd",val);
		}
	}

	/**
	 * Called them the vm starts
	 * @param jvmti_env jvmti_evn that can be used by the agent
	 * @param env JNIEnv that can be used by the agent
	 */
	virtual void vmStart(jvmtiEnv *jvmti_env,JNIEnv* env)
	{
		message("vm started\n");
	}

	/**
	 * Called them the vm stops
	 * @param jvmti_env jvmti_evn that can be used by the agent
	 * @param env JNIEnv that can be used by the agent
	 */
	virtual void vmStop(jvmtiEnv *jvmti_env,JNIEnv* env)
	{
		message("vm stopped\n");
	}

	/**
	 * Called to parse the arguments passed to the agent
	 * @param options string containing the options
	 * @returns one of the AGENT_ERROR_ values
	 */
	jint parseArguments(char* options);

	/**
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* options);

};

#endif /*RUNTIMETOOLS_AGENTBASE_HPP_*/
