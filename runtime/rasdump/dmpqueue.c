/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "rasdump_internal.h"


omr_error_t
insertDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent)
{
	J9RASdumpQueue *queue;

	/*
	 * Sanity check
	 */
	if ( FIND_DUMP_QUEUE(vm, queue) ) {

		J9RASdumpAgent **oldNextPtr = &queue->agents, *node = *oldNextPtr;
		omr_error_t rc = OMR_ERROR_NONE;

		/* install any necessary dump hooks */
		if( (rc = rasDumpEnableHooks(vm, agent->eventMask)) != OMR_ERROR_NONE ) {
			return rc;
		}

		/* Higher priority dumps run before lower priority dumps */
		while ( node && node->priority >= agent->priority ) {
			oldNextPtr = &node->nextPtr; node = *oldNextPtr;
		}

		/* clear count */
		agent->count = 0;

		/* verify start..stop range (allow n..n-1 to indicate open-ended range) */
		if ( agent->stopOnCount < agent->startOnCount ) {
			agent->stopOnCount = agent->startOnCount - 1;
		}

		/* Insert agent at correct priority level */
		agent->nextPtr = node;
		*oldNextPtr = agent;

		return rc;
	}

	return OMR_ERROR_INTERNAL;
}


omr_error_t
removeDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent)
{
	J9RASdumpQueue *queue;

	/*
	 * Sanity check
	 */
	if ( FIND_DUMP_QUEUE(vm, queue) ) {

		J9RASdumpAgent **oldNextPtr = &queue->agents, *node = *oldNextPtr;

		/* Must search to find correct place in the singly-linked list */
		while ( node && node != agent ) {
			oldNextPtr = &node->nextPtr; node = *oldNextPtr;
		}

		if ( node ) {
			/* Remove agent if found */
			*oldNextPtr = node->nextPtr;
			node->nextPtr = NULL;
		}

		return node ? OMR_ERROR_NONE : OMR_ERROR_INTERNAL;
	}

	return OMR_ERROR_INTERNAL;
}


omr_error_t
seekDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr, J9RASdumpFn dumpFn)
{
	J9RASdumpQueue *queue;

	/*
	 * Sanity check
	 */
	if ( FIND_DUMP_QUEUE(vm, queue) ) {

		J9RASdumpAgent *node = *agentPtr;

		/* Start from next agent, else start from the beginning */
		node = node ? node->nextPtr : queue->agents;

		/* Agents with same dumpFn will be found in priority order */
		while ( node && dumpFn && node->dumpFn != dumpFn ) {
			node = node->nextPtr;
		}

		/* Update the current position */
		*agentPtr = node;

		return node ? OMR_ERROR_NONE : OMR_ERROR_INTERNAL;
	}

	/* Blank current position */
	*agentPtr = NULL;

	return OMR_ERROR_INTERNAL;
}
