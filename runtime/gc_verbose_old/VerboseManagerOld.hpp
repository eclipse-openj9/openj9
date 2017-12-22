
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(VERBOSEMANAGEROLD_HPP_)
#define VERBOSEMANAGEROLD_HPP_

#include "VerboseManagerBase.hpp"
#include "VerboseOutputAgent.hpp"


/**
 * Contains the old verbose codepath (-Xgc:verboseFormat:deprecated).
 * @ingroup GC_verbose_engine
 */
class MM_VerboseManagerOld : public MM_VerboseManagerBase
{
	/*
	 * Data members
	 */
private:
	J9JavaVM *javaVM;
	
	/* Pointers to the Hook interface */
	J9HookInterface** _mmHooks;

	/* The event stream */
	MM_VerboseEventStream *_eventStream;
	
	/* The Output agents */
	MM_VerboseOutputAgent *_agentChain;

protected:
public:
	
	/*
	 * Function members
	 */
private:

	void chainOutputAgent(MM_VerboseOutputAgent *agent);
	MM_VerboseOutputAgent *findAgentInChain(AgentType type);
	void disableAgents();
	UDATA countActiveOutputAgents();

	AgentType parseAgentType(MM_EnvironmentBase *env, char *filename, UDATA fileCount, UDATA iterations);

	/**
	 * Enable hooks for old verbose which are used in realtime configuration
	 */
	void enableVerboseGCRealtime();

	/**
	 * Disable hooks for old verbose which are used in realtime configuration
	 */
	void disableVerboseGCRealtime();

	/**
	 * Enable hooks for old verbose which are used in non-realtime configurations
	 */
	void enableVerboseGCNonRealtime();

	/**
	 * Disable hooks for old verbose which are used in non-realtime configurations
	 */
	void disableVerboseGCNonRealtime();
	
	/**
	 * Enable hooks for old verbose which are used in VLHGC configuration
	 */
	void enableVerboseGCVLHGC();

	/**
	 * Disable hooks for old verbose which are used in VLHGC configuration
	 */
	void disableVerboseGCVLHGC();
	
protected:

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:

	MM_VerboseEventStream *getEventStreamForEvent(MM_VerboseEvent *event);
	MM_VerboseEventStream *getEventStream() { return _eventStream; }

	/* Interface for Dynamic Configuration */
	virtual bool configureVerboseGC(OMR_VM *omrVM, char* filename, UDATA fileCount, UDATA iterations);

	/**
	 * Determine the number of currently active output mechanisms.
	 * @return a count of the number of active output mechanisms.
	 */
	virtual UDATA countActiveOutputHandlers();

	/* Call for Event Stream */
	void passStreamToOutputAgents(MM_EnvironmentBase *env, MM_VerboseEventStream *stream);

	virtual void enableVerboseGC();
	virtual void disableVerboseGC();

	static MM_VerboseManagerOld *newInstance(MM_EnvironmentBase *env, OMR_VM* vm);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Close all output mechanisms on the receiver.
	 * @param env vm thread.
	 */
	virtual void closeStreams(MM_EnvironmentBase *env);

	J9HookInterface** getHookInterface(){ return _mmHooks; }

	MM_VerboseManagerOld(OMR_VM *vm)
		: MM_VerboseManagerBase(vm)
		, _mmHooks(NULL)
		, _eventStream(NULL)
		, _agentChain(NULL)
	{
	}
};

#endif /* VERBOSEMANAGEROLD_HPP_ */
