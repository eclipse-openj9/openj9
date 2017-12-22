/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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
#ifndef RUNTIMETOOLS_BALLOONAGENT_HPP_
#define RUNTIMETOOLS_BALLOONAGENT_HPP_

#include "RuntimeToolsIntervalAgent.hpp"

#define MAX_BALLOON_SIZE 17000
#define NUMBER_OF_BYTES_IN_1K 1024
#define NUMBER_OF_BYTES_IN_1M (1024*1024)

class BalloonAgent : public RuntimeToolsIntervalAgent
{
private:
    /*
     * Data members
     */
	/* the size of the balloon in megabytes */
	int _balloonSize;

	/* the length of the delay in milliseconds between the agent start and balloon inflation start */
	int _balloonDelay;

	/* the length of time in milliseconds between each 1M inflate or deflate of the balloon */
	int _balloonInterval;

	/* the length of time in milliseconds that the balloon is fully inflated */
	int _holdTime;

	/* used to ensure the balloon bytes are not all 0s */
	jbyte initializeBuffer[NUMBER_OF_BYTES_IN_1M];

protected:
	/**
	 * Default constructor
	 *
     * @param vm [in] java vm that can be used by this manager
	 */
	BalloonAgent(JavaVM * vm) : RuntimeToolsIntervalAgent(vm) {}

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
     * @param vm [in] java vm that can be used by this manager
	 */
	static BalloonAgent* newInstance(JavaVM * vm);

	/**
	 * This get gets the reference to the agent given the pointer to where it should
	 * be stored.  If necessary it creates the agent
	 * @param vm    [in] that can be used by the method
	 * @param agent [out] pointer to storage where pointer to agent can be stored
	 */
	static inline BalloonAgent* getAgent(JavaVM * vm, BalloonAgent** agent){
		if (NULL == *agent) {
			*agent = newInstance(vm);
		}
		return *agent;
	}

	/**
	 * Method called at the interval specified in the options for the agent
	 */
	virtual void runAction(void);

	/**
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options [in] string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* option);

};

#endif /*RUNTIMETOOLS_BALLOONAGENT_HPP_*/
