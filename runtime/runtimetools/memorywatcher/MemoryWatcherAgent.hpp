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
#ifndef RUNTIMETOOLS_MEMORYWATCHERAGENT_HPP_
#define RUNTIMETOOLS_MEMORYWATCHERAGENT_HPP_

#include "RuntimeToolsIntervalAgent.hpp"

typedef struct memcategory_data_frame
{
	U_32 category;
	UDATA liveBytes;
	UDATA liveAllocations;
} memcategory_data_frame;

class MemoryWatcherAgent : public RuntimeToolsIntervalAgent
{
private:
	int _currentCategory;
	UDATA* _category_last;
	UDATA _lastUsedVirtual;
	UDATA _lastPeakUsedVirtual;
	UDATA _lastPeakWorkingSet;
	UDATA _lastWorkingSet;
	UDATA _lastAvailVirtual;
	bool _dumpGenerated;

	/* class used to call method to dump a core file */
	jclass _cls;

	/* method used to call method to dump a core file */
	jmethodID _method;

	/* holds the threshold a which we should generate dumps */
	int _dumpThreshHold;

protected:
	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	MemoryWatcherAgent(JavaVM * vm) : RuntimeToolsIntervalAgent(vm) {}

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
	static MemoryWatcherAgent* newInstance(JavaVM * vm);

	/**
	 * This get gets the reference to the agent given the pointer to where it should
	 * be stored.  If necessary it creates the agent
	 * @param vm that can be used by the method
	 * @param agent pointer to storage where pointer to agent can be stored
	 */
	static inline MemoryWatcherAgent* getAgent(JavaVM * vm, MemoryWatcherAgent** agent){
		if (*agent == NULL){
			*agent = newInstance(vm);
		}
		return *agent;
	}

	/**
	 * sets the last bytes for the current category
	 * @param value to set
	 */
	inline void setLastBytes(UDATA liveBytes){_category_last[_currentCategory] = liveBytes;_currentCategory++;};

	/**
	 * sets the last bytes for the current category
	 * @param value to set
	 */
	inline UDATA getLastBytes(){return _category_last[_currentCategory];};

	/**
	 * Method called at the interval specified in the options for the agent
	 */
	virtual void runAction();

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

#endif /*RUNTIMETOOLS_MEMORYWATCHERAGENT_HPP_*/
