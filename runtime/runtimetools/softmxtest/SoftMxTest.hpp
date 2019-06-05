/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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
#ifndef RUNTIMETOOLS_SOFTMXTEST_HPP_
#define RUNTIMETOOLS_SOFTMXTEST_HPP_

#include "RuntimeToolsIntervalAgent.hpp"

class SoftMxTest : public RuntimeToolsIntervalAgent
{

    /*
     * Data members
     */
private:

	/* used to indicate what kind of callback testing we are going */
	bool _testCallbackOOM;
	bool _testCallbackExpand;

	/* pointer to gc hooks */
	J9HookInterface** _gcOmrHooks;

	/*
	 * Function members
	 */
protected:
	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	SoftMxTest(JavaVM * vm) : RuntimeToolsIntervalAgent(vm) {}

	/**
	 * This method is used to initialize an instance
	 */
	bool init(void);

public:
	/**
	 * This method should be called to destroy the object and free the associated memory
	 */
	virtual void kill(void);

	/**
	 * This method is used to create an instance
     * @param vm java vm that can be used by this manager
	 */
	static SoftMxTest* newInstance(JavaVM * vm);

	/**
	 * This get gets the reference to the agent given the pointer to where it should
	 * be stored.  If necessary it creates the agent
	 * @param vm that can be used by the method
	 * @param agent pointer to storage where pointer to agent can be stored
	 */
	static inline SoftMxTest* getAgent(JavaVM * vm, SoftMxTest** agent){
		if (*agent == NULL){
			*agent = newInstance(vm);
		}
		return *agent;
	}

	/**
	 * Method called at the interval specified in the options for the agent
	 */
	virtual void runAction(void);

	/**
	 * This method should be called when Agent_OnLoad is invoked by the JVM.  After calling the setup for
	 * the class it subclasses it does the setup required for this class.
	 *
	 * @param options the options passed to the agent in Agent_OnLoad
	 *
	 * @returns 0 on success, and error value otherwise
	 */
	virtual jint setup(char * options);

	/**
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* option);

	/**
	 * This method is called if an OOM is about to occur due to the softmx value set.
	 * It gives us a chance to adjust the softmx value upwards and try again
	 *
	 * @param timestamp time of the callback
	 * @param maxHeapSize -Xmx value specified when jvm was started
	 * @param currentHeapSize current size of memory committed for the heap
	 * @param currentSoftMx current softMx value
	 * @param bytesRequired bytes required to satisfy allocation
	 */
	void adjustSoftmxForOOM(U_64 timestamp, UDATA maxHeapSize, UDATA currentHeapSize, UDATA currentSoftMx, UDATA bytesRequired);

};

#endif /*RUNTIMETOOLS_SOFTMXTEST_HPP_*/
