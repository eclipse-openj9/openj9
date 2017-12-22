/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#ifndef TRCQUEUE_H
#define TRCQUEUE_H

/* @ddr_namespace: default */
#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#define CLEANING_MSG_FLAG ((qMessage*)(uintptr_t)0x1)

typedef struct message {
	volatile int32_t			 subscriptions;
	volatile int32_t			 pauseCount;
	volatile int32_t			 referenceCount;
	struct message *volatile next;
	struct message			*nextSecondary;
	void					*data;
} qMessage;

typedef struct queue {
	volatile int32_t			 subscriptions;
	volatile int32_t		 alive;
	struct message *volatile head;
	struct message *volatile tail;
	struct subscription		*subscribers;
	struct UtEventSem		*alarm;
	omrthread_monitor_t volatile	 lock;
	int32_t					 allocd;
	struct message *volatile referenceQueue;
	volatile uint32_t			 pause;
} qQueue;

typedef struct subscription {
	struct message *volatile current;
	struct message *volatile last;
	volatile int32_t		 valid;
	struct message			*stop;
	struct subscription		*prev;
	struct subscription		*next;
	struct queue			*queue;
	int32_t					 currentLocked;
	int32_t					 allocd;
	int32_t					 savedReference;
} qSubscription;

omr_error_t createQueue(qQueue **queue);
void destroyQueue(qQueue *queue);

omr_error_t subscribe(qQueue *queue, qSubscription **subscription, qMessage *start, qMessage *stop);
omr_error_t unsubscribe(qSubscription *sub);

int32_t publishMessage(qQueue *queue, qMessage *msg);
qMessage * acquireNextMessage(qSubscription *sub);
void releaseCurrentMessage(qSubscription *sub);
void notifySubscribers(qQueue *queue);

void pauseDequeueAtMessage(qMessage *msg);
void resumeDequeueAtMessage(qMessage *msg);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
#endif /* !TRCQUEUE_H */
