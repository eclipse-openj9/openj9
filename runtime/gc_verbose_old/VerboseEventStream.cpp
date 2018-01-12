
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

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "VerboseEvent.hpp"
#include "VerboseEventStream.hpp"
#include "GCExtensions.hpp"
#include "VerboseManagerOld.hpp"

/**
 * Create a new MM_VerboseEventStream instance.
 * @return Pointer to the new MM_VerboseEventStream.
 */
MM_VerboseEventStream *
MM_VerboseEventStream::newInstance(MM_EnvironmentBase *env, MM_VerboseManagerOld *manager)
{
	MM_VerboseEventStream *eventStream = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	
	eventStream = (MM_VerboseEventStream *)extensions->getForge()->allocate(sizeof(MM_VerboseEventStream), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (eventStream) {
		new(eventStream) MM_VerboseEventStream(env, manager);
	}
	return eventStream;
}

/**
 * Kill the MM_VerboseEventStream instance.
 * Tears down the related structures and frees any storage.
 */
void
MM_VerboseEventStream::kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	extensions->getForge()->free(this);
}

/**
 * Tear down the structures managed by the MM_VerboseEventStream.
 */
void
MM_VerboseEventStream::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseEvent *event, *nextEvent;

	event = _eventChain;
	_eventChain = NULL;
	_eventChainTail = NULL;
	
	while(NULL != event){
		nextEvent = event->getNextEvent();
		event->kill(env);
		event = nextEvent;
	}
}

/**
 * Calls each events consume routine.
 */
void
MM_VerboseEventStream::callConsumeRoutines(MM_EnvironmentBase *env)
{
	MM_VerboseEvent *event = _eventChain;
	
	while(NULL != event) {
		event->consumeEvents();
		event = event->getNextEvent();
	}
}

/**
 * Removes events which don't output form the chain.
 */
void
MM_VerboseEventStream::removeNonOutputEvents(MM_EnvironmentBase *env)
{
	MM_VerboseEvent *event, *nextEvent;
	
	event = _eventChain;
	while(NULL != event){
		nextEvent = event->getNextEvent();
		if(!event->definesOutputRoutine()){
			removeEventFromChain(env, event);
		}
		event = nextEvent;
	}
}

/**
 * Remove a specified event from the event chain.
 * @param event Pointer to the event to be removed.
 */
void
MM_VerboseEventStream::removeEventFromChain(MM_EnvironmentBase *env, MM_VerboseEvent *event)
{
	MM_VerboseEvent *previousEvent = event->getPreviousEvent();
	MM_VerboseEvent *nextEvent = event->getNextEvent();
	
	if(NULL != previousEvent) {
		previousEvent->setNextEvent(nextEvent);
	} else {
		/* We are removing the head of the chain */
		_eventChain = nextEvent;
	}
	
	if(NULL != nextEvent) {
		nextEvent->setPreviousEvent(previousEvent);
	} else {
		/* We are removing the tail of the chain */
		_eventChainTail = previousEvent;
	}
	
	event->kill(env);
}

/**
 * Add an event to the event chain.
 * @param event Pointer to the event to be added.
 */
void
MM_VerboseEventStream::chainEvent(MM_EnvironmentBase *env, MM_VerboseEvent *event)
{
	UDATA oldValue, newValue;
	
	newValue = (UDATA)event;
	
	do {
		oldValue = (UDATA)_eventChainTail;
		((MM_VerboseEvent *)newValue)->setPreviousEvent((MM_VerboseEvent *)oldValue);
	} while((UDATA)oldValue != MM_AtomicOperations::lockCompareExchange((volatile UDATA *)&_eventChainTail, oldValue, newValue));
	
	if (oldValue) {
		((MM_VerboseEvent *)oldValue)->setNextEvent((MM_VerboseEvent*)newValue);
	} else {
		_eventChain = event;
	}
}

/**
 * Process the event stream.
 * Prepares the event stream for output and notifies the verbose manager to pass the stream to the output agents.
 */
void
MM_VerboseEventStream::processStream(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	_manager->incrementOutputCount();
	callConsumeRoutines(env);
	removeNonOutputEvents(env);
	_manager->passStreamToOutputAgents(env, this);
	if(isDisposable()) {
		kill(env);
	} else {
		_manager->setLastOutputTime(j9time_hires_clock());
		tearDown(env);
	}
}

/**
 * Returns an event of a specific type.
 * Walks the event stream backwards from the event passed in and returns the first event of the type requested.
 * @param eventID the type of event required.
 * @param hookInterface interface associated with eventID
 * @param event pointer to the current event.
 * @return Pointer an event of that type.
 */
MM_VerboseEvent *
MM_VerboseEventStream::returnEvent(UDATA eventid, J9HookInterface** hookInterface, MM_VerboseEvent *event)
{
	while(NULL != event) {
		if((eventid == event->getEventType()) && (hookInterface == event->getHookInterface())) {
			return event;
		}
		event = event->getPreviousEvent();
	}
	return NULL;
}

/**
 * Returns an event of a specific type.
 * Walks the event stream backwards from the event passed in and returns the first event of the type requested.
 * HOWEVER, if an event of type stopEventID is encountered first, NULL is returned.
 * @param eventID the type of event required.
 * @param hookInterface interface associated with eventID
 * @param event pointer to the current event.
 * @param stopEventID the type of event to stop at if encountered.
 * @param stopHookInterface interface associated with stopEventID
 * @return Pointer an event of that type.
 */
MM_VerboseEvent *
MM_VerboseEventStream::returnEvent(UDATA eventid, J9HookInterface** hookInterface, MM_VerboseEvent *event, UDATA stopEventID, J9HookInterface** stopHookInterface)
{
	while(NULL != event) {
		if((stopEventID == event->getEventType()) && (stopHookInterface == event->getHookInterface())) {
			/* we've hit a "stopEventID" first, so return NULL */
			return NULL;
		}
		if((eventid == event->getEventType()) && (hookInterface == event->getHookInterface())) {
			return event;
		}
		event = event->getPreviousEvent();
	}
	return NULL;
}
