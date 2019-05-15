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
#ifndef RUNTIMETOOLS_INTERVALAGENT_HPP_
#define RUNTIMETOOLS_INTERVALAGENT_HPP_

#include "j9protos.h"
#include "j9comp.h"
#include "jvmti.h"
#include "RuntimeToolsAgentBase.hpp"

class RuntimeToolsIntervalAgent : public RuntimeToolsAgentBase
{
private:
	/*
	 * holds the length of the interval in seconds between invocations of the
	 * runAction method
	 */
	jint _interval;

	/* indicator of whether the agent should stop doing its action */
	bool _done;

	/* monitor used to coordinate with the thread that calls runAction on each interval */
	omrthread_monitor_t _monitor;

	/* env that interval thread can use when running jni */
	JNIEnv* _env;

protected:
	int _dumpThreshHold;

	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	RuntimeToolsIntervalAgent(JavaVM * vm) : RuntimeToolsAgentBase(vm) {}

	/**
	 * This method is used to initialize an instance
	 */
	bool init();

public:
	/**
	 * This method should be called to destroy the object and free the associated memory
	 */
	virtual void kill();

	/**
	 * This method is used to create an instance
     * @param vm java vm that can be used by this manager
	 */
	static RuntimeToolsIntervalAgent* newInstance(JavaVM * vm);

	/**
	 * method that generates the calls to runAction at the appropriate intervals *.
	 */
	void generateRunActionCalls();

	/**
	 * Called them the vm starts
	 * @param jvmti_env jvmti_evn that can be used by the agent
	 * @param env JNIEnv that can be used by the agent
	 */
	virtual void vmStart(jvmtiEnv *jvmti_env,JNIEnv* env);

	/**
	 * Called them the vm stops
	 * @param jvmti_env jvmti_evn that can be used by the agent
	 * @param env JNIEnv that can be used by the agent
	 */
	virtual void vmStop(jvmtiEnv *jvmti_env,JNIEnv* env);

	/**
	 * Method called at the interval specified in the options for the agent
	 */
	virtual void runAction(){};

	/** return an env that can be used for jni calls */
	inline JNIEnv* getEnv(){return _env;};

	/**
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* option);
};

#endif /*RUNTIMETOOLS_INTERVALAGENT_HPP_*/
