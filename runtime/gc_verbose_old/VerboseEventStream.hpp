
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

#if !defined(EVENTSTREAM_HPP_)
#define EVENTSTREAM_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "Base.hpp"
#include "EnvironmentBase.hpp"

class MM_VerboseEvent;
class MM_VerboseManager;
class MM_VerboseManagerOld;

/**
 * Part of the verbose gc mechanism.
 * Provides management routines and the anchor point for the chain of MM_VerboseEvents.
 * @ingroup GC_verbose_engine
 */
class MM_VerboseEventStream : public MM_Base
{
private:
	J9JavaVM *_javaVM;
	MM_VerboseManagerOld* _manager;

	/* Anchor point for chained events */
	MM_VerboseEvent *_eventChain;
	/* Last entry in Event chain */
	MM_VerboseEvent * volatile _eventChainTail;

	bool _disposable; /**< Determines if the event stream should be disposed of immediately after processing. */
	
	void callConsumeRoutines(MM_EnvironmentBase *env);
	void removeNonOutputEvents(MM_EnvironmentBase *env);
	
	void removeEventFromChain(MM_EnvironmentBase *env, MM_VerboseEvent *event);
	
	void tearDown(MM_EnvironmentBase *env);
	
public:
	static MM_VerboseEventStream *newInstance(MM_EnvironmentBase *env, MM_VerboseManagerOld *manager);
	virtual void kill(MM_EnvironmentBase *env);

	MMINLINE MM_VerboseEvent *getHead() { return _eventChain; }
	MMINLINE MM_VerboseEvent *getTail() { return (MM_VerboseEvent *)_eventChainTail; }

	void chainEvent(MM_EnvironmentBase *env, MM_VerboseEvent *event);
	void processStream(MM_EnvironmentBase *env);
	
	MM_VerboseEvent *returnEvent(UDATA eventid, J9HookInterface** hook, MM_VerboseEvent *event);
	MM_VerboseEvent *returnEvent(UDATA eventid, J9HookInterface** hook, MM_VerboseEvent *event, UDATA stopEventID, J9HookInterface** stopHookInterface);
	
	void setDisposable(bool disposable) { _disposable = disposable; }
	bool isDisposable() { return _disposable; }

	MM_VerboseEventStream(MM_EnvironmentBase *env, MM_VerboseManagerOld *manager) :
		MM_Base(),
		_javaVM((J9JavaVM *)env->getLanguageVM()),
		_manager(manager),
		_eventChain(NULL),
		_eventChainTail(NULL),
		_disposable(false)
	{}
};

#endif /* EVENTSTREAM_HPP_ */
