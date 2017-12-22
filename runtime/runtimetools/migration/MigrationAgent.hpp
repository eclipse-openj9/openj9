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
#ifndef RUNTIMETOOLS_MIGRATIONAGENT_HPP_
#define RUNTIMETOOLS_MIGRATIONAGENT_HPP_

#if defined(AIXPPC)
#include <sys/dr.h>
#endif /* defined(AIXPPC) */

#include "RuntimeToolsIntervalAgent.hpp"

/* definition of the states that the migration can be in */
#define STATE_WAITING					0
#define STATE_PRE_MIGRATE				1
#define STATE_PRE_MIGRATE_COMPLETE		2
#define STATE_MIGRATION_IMMINENT		3
#define STATE_PREPARED_FOR_MIGRATION	4
#define STATE_POST_MIGRATE				5

class MigrationAgent : public RuntimeToolsIntervalAgent
{

    /*
     * Data members
     */
private:
	/* monitor used signal that reconfig signal has been received */
	omrthread_monitor_t _reconfigMonitor;

	/* Indicates current state of the JVM with regard to migration */
	UDATA _migrationState;

	/* count of how many pre migrate messages we have received */
	UDATA _preMigrateCount;

	/* count of how many migration message we have received */
	UDATA _migrateCount;

	/* count of how many post migrate message we have received */
	UDATA _postMigrateCount;

	/* maximum amount of time in milliseconds that will we wait for a migrate after a pre-migrate */
	int _waitForMigrateInterval;

	/* used to signal that agent is stopping */
	bool _stopping;

	/* if true we don't output and status information */
	bool _quiet;

	/* value from agent argument  */
	bool _doGcForPreMigrationPrep;

	/* value from agent argument */
	bool _adjustSoftmx;

	/* value from agent argument */
	bool _forceSoftmx;

	/* value from agent argument */
	bool _onetimeSoftmx;

	/* value from agent argument */
	bool _haltThreadsForMigration;

	/* current target for softmx */
	UDATA _softMx;

	/* last total heap size */
	UDATA _lastTotalMemory;

	/*
	 * Function members
	 */
protected:
	/**
	 * Default constructor
	 *
     * @param vm java vm that can be used by this manager
	 */
	MigrationAgent(JavaVM * vm) : RuntimeToolsIntervalAgent(vm) {}

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
	static MigrationAgent* newInstance(JavaVM * vm);

	/**
	 * This get gets the reference to the agent given the pointer to where it should
	 * be stored.  If necessary it creates the agent
	 * @param vm that can be used by the method
	 * @param agent pointer to storage where pointer to agent can be stored
	 */
	static inline MigrationAgent* getAgent(JavaVM * vm, MigrationAgent** agent){
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
	 * This is called once for each argument, the agent should parse the arguments
	 * it supports and pass any that it does not understand to parent classes
	 *
	 * @param options string containing the argument
	 *
	 * @returns one of the AGENT_ERROR_ values
	 */
	virtual jint parseArgument(char* option);


	/**
	 * this method is called by the handler registered to pass on information when
	 * a reconfig signal is received
     *
	 * @param dr [in] structure containing information about reconfig event
	 * @return DR_RECONFIG_DONE if successful, DR_EVENT_FAIL  otherwise
	 */
#if defined(AIXPPC)
	int reconfigSignalReceived(dr_info_t dr);
#endif

	/**
	 * Called them the vm stops
	 * @param jvmti_env jvmti_evn that can be used by the agent
	 * @param env JNIEnv that can be used by the agent
	 */
	virtual void vmStop(jvmtiEnv *jvmti_env,JNIEnv* env);

	/**
	 * Normalize the counts to ignore events that we are already too late to deal with
	 */
	void normalizeCounts(void);

	/**
	 * Does the next step for pre-migration work
	 * @param step [in] the next step to do
	 * @return true if done, false if more steps to complete
	 *
	 */
	virtual bool doNextPreMigrateStep(int step);

	/**
     * Indicates that we believe migration was aborted
	 * @param step [in] the next step to do
	 */
	virtual void doAbortPreMigrate(int step);

	/**
	 * Does the next step for final-migration work
	 * @param step [in] the next step to do
	 * @return true if done, false if more steps to complete
	 *
	 */
	virtual bool doNextFinalMigrateStep(int step);

	/**
	 * Does all post migration work
	 */
	virtual void completePostMigration(void);
};

#endif /*RUNTIMETOOLS_MIGRATIONAGENT_HPP_*/
