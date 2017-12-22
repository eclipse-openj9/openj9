
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

#if !defined(EVENT_CLASS_UNLOADING_END_HPP_)
#define EVENT_CLASS_UNLOADING_END_HPP_
 
#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to the end of class unloading.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventClassUnloadingEnd : public MM_VerboseEvent
{
private:
	/* Passed Data */
	UDATA	_classLoadersUnloadedCount;	/**< the number of classloader unloaded */
	UDATA	_classesUnloadedCount; /**< the number of classes unloaded */
	U_64 	_cleanUpClassLoadersStartTime; /**< the time to call cleanUpClassLoadersStart */
	U_64	_cleanUpClassLoadersTime; /**< the time the gc was waiting on the cleanUpClassLoader call */
	U_64 	_cleanUpClassLoadersEndTime; /**< the time to call cleanUpClassLoadersEnd */
	U_64	_classUnloadMutexQuiesceTime; /**< Time the GC is waiting on the classUnloadMutex held by the JIT */
	
	/* Consumed Data */
	U_64	_classUnloadingStartTime;
	
public:
	
	static MM_VerboseEvent *newInstance(MM_ClassUnloadingEndEvent *event, J9HookInterface** hookInterface);
	
	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };

	MM_VerboseEventClassUnloadingEnd(MM_ClassUnloadingEndEvent *event, J9HookInterface** hookInterface)
		: MM_VerboseEvent(event->currentThread->omrVMThread, event->timestamp, event->eventid, hookInterface)
		, _classLoadersUnloadedCount(event->classLoaderCount)
		, _classesUnloadedCount(event->classesCount)
		, _cleanUpClassLoadersStartTime(event->cleanUpClassLoadersStartTime)
		, _cleanUpClassLoadersTime(event->cleanUpClassLoaders)
		, _cleanUpClassLoadersEndTime(event->cleanUpClassLoadersEndTime)
		, _classUnloadMutexQuiesceTime(event->quiesceTime)
		, _classUnloadingStartTime(0)
	{}
};

#endif /* EVENT_CLASS_UNLOADING_END_HPP_ */
