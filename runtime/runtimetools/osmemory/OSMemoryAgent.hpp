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
#ifndef RUNTIMETOOLS_OSMEMORYAGENT_HPP_
#define RUNTIMETOOLS_OSMEMORYAGENT_HPP_

#include "RuntimeToolsIntervalAgent.hpp"

#if defined(AIXPPC)

/* for experimentation we simply hard coded this value */
#define MAX_JVMS_IN_HIVE 4

/* structure used to return data obtained through the perf stat interface */
typedef struct PerfStatData {
	u_longlong_t hpi; /* hypervisor pages in since LPAR start */
	u_longlong_t hpit; /* hypervisor page in time since LPAR start in nanoseconds */
	u_longlong_t pmem; /* assigned physical memory in bytes */
};

/* structure used to send data between JVMS */
typedef struct HiveData {
	long 			jvmId; /* unique id assigned to JVM on command line */
	u_longlong_t	currentHpi; /* hpi value for last interval */
	u_longlong_t	assignedPhysical; /* currently assigned physical memory in bytes */
};
#endif

class OSMemoryAgent : public RuntimeToolsIntervalAgent
{

    /*
     * Data members
     */
private:

	/* if true the agent adjust softmx automatically */
	bool _adjustSoftMx;

	/* the softmx value in bytes as of the last runAction invocation */
	int _lastSoftMx;

	/* the softmx value in bytes currently set as the target */
	UDATA _currentSoftMx;

	/* the target free physical memory in M */
	UDATA _freeMemoryTarget;

	/* the number of gcs initiated where we did not see the heap size decrease */
	int _gcs_initiated;

	/* the size of the heap in bytes on the last runAction invocation */
	UDATA _lastHeapSize;

	/* the maximum number of gcs we allow without seeing the heap contract */
	int _maxgcs;

	/* the original heap size in bytes  */
	UDATA  _maxHeapSize;

	/* if true we don't output and status information */
	bool _quiet;

	/* interval in milliseconds between gcs initiated to try to get heap to contract to softmx target */
	int _gcInterval;

	/* the delay in milliseconds between the start of the agent and polling/softmx adjustment */
	int _startDelay;

	/* true the first time runAction is called, false otherwise */
	bool _firstInvocation;

	/* number of runAction invocations were the free bytes in the heap have not changed */
	int _freeUnchanged;

	/* number of bytes free in the heap on the last runAction invocation */
	UDATA _lastFree;

	/* The time of the last sample */
	I_64 _lastRunTime;

#if defined(AIXPPC)
	/* target for hypervisor pageins from the command line */
	int _hpiTarget;

	/* target for hypervisor pagein time, only used on AIX from the command line */
	int _hpiTimeTarget;

	/* used to store the target for total free physical memory for the shared pool in MB from the command line */
	int _totalFreePhysicalTarget;

	/* used to hold if we were requested to use dropping physical memory as an indicator of memory pressure */
	bool _physicalDropping;

	/* holds the unique id for the JVM from command line */
	int _jvmId;

	/* last hpi value */
	u_longlong_t _lastHPI;

	/* last hpi value */
	u_longlong_t _lastHPITime;

	/* used to store the total hpi count for the hive */
	long _totalHPI;

	/* used to store the total physical memory use by the hive lpars */
	u_longlong_t _totalPhysical;

	/* used to store physical memory assigned to this lpar */
	u_longlong_t _lastAssignedPhysical;

	/* the multicast sockets used to communicate with the other JVMs */
	int _multicastSocketIn;

	/* holds the hive data values for the JVM */
	u_longlong_t _hpiValues[MAX_JVMS_IN_HIVE];
	u_longlong_t _assignedPhysical[MAX_JVMS_IN_HIVE];

#endif

	/*
	 * Function members
	 */
protected:
	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	OSMemoryAgent(JavaVM * vm) : RuntimeToolsIntervalAgent(vm) {}

	/**
	 * This method is used to initialize an instance
	 */
	bool init(void);

#if defined(AIXPPC)
	/**
	 * This method uses the perfstats API to get hpi, hpit and assigned physical memory
	 * for the partition the JVM is running in
	 * @param data[out] pointer to the structure in which the data should be returned
	 */
	void getPerfStatData(PerfStatData* data);

	/**
	 * This method broadcasts the data provided to the hive
	 *
	 * @param data[in] structure with the data to be broadcast
	 */
	void broadcastToHive(HiveData* data);

	/**
	 * This method reads all data available to the hive and updates its local storage
	 *
	 */
	void updateFromHive(void);

#endif

public:
	/**
	 * This method should be called to destroy the object and free the associated memory
	 */
	virtual void kill(void);

	/**
	 * This method is used to create an instance
     * @param vm java vm that can be used by this manager
	 */
	static OSMemoryAgent* newInstance(JavaVM * vm);

	/**
	 * This get gets the reference to the agent given the pointer to where it should
	 * be stored.  If necessary it creates the agent
	 * @param vm that can be used by the method
	 * @param agent pointer to storage where pointer to agent can be stored
	 */
	static inline OSMemoryAgent* getAgent(JavaVM * vm, OSMemoryAgent** agent){
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

};

#endif /*RUNTIMETOOLS_OSMEMORYAGENT_HPP_*/
