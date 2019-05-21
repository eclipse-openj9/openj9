/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "rastrace_internal.h"
#include "omrthread.h"
#include "omrutilbase.h"



/*
 * A messages subscriptions value starts with a value of untagged prior to being queued and undergoes the
 * following atomic transitions once it is on the queue:
 *  * incremented by (queue->subscriptions - untagged value)
 *  * decremented by 1, as each subscriber is finished with it
 *
 * The result of this is that the value cannot be zero until all subscribers have finished with it AND
 * it's subscriber count has been incremented by publishing. This allows us to use a message->subscriptions
 * value of zero as a definitive indication that the message is finished with.
 *
 * Complexity is introduced by subscriptions and unsubscriptions. In these cases we need to ensure that the
 * message->subscriptions value is consistent with the actual number of subscribers that will process it.
 *
 * In order to maintain the consistency between number and state of subscribers and the message subscriptions
 * we need to know exactly which message is the last queued before we update the global queue->subscriptions
 * value. This is done via two-phase tagging, full serialization of subscribe/unsubscribe and a guarantee of
 * strict time ordering in the subscription counts of messages on the queue.
 *
 * Tags are values of a magnitude such that no combination of the atomic transitions that are applied to a
 * messages count will cause it to be a valid value for a messages subscription count given the current number
 * of subscribers. We use a positive tag for subscription and negative for unsubscription to aid in debugging.
 */


/* TODO: should this be (-1 >> 1) /2 ? */
#define UNTAGGED ((uint32_t)0x0000FFFF)

#define WORK_FROM_TAIL(s) \
	do { \
		qSubscription *macroSub = s; \
		if (macroSub->current != NULL) { \
			macroSub->last = macroSub->current; \
			macroSub->current = NULL; \
		} else { \
			/* indicator that we're working from tail */ \
			macroSub->last = (qMessage*)-1; \
		} \
	} while (0)


/*
 * Pointers to qMessage structures have two invalid states, NULL and 0x1.
 *
 * We use this to block queuing of messages to head while head == tail and tail is being
 * cleaned. We use this instead of additional flags so that we can still update the state
 * with cmpxchg atomic instructions. This functions through publishMessage and clean both doing
 * a compare-and-swap of msg->next against NULL before operating on a message. publishMessage will
 * update msg->next to be the message being published, clean will update it to be 0x1.
 */
#define IS_VALID_MSG_PTR(m) (((uintptr_t)m != (uintptr_t)NULL) && ((qMessage*)m != CLEANING_MSG_FLAG))

/*
 * This function checks the message queue and frees any messages on it that have been processed
 * by all expected subscribers. Freeing is done though the function pointer specified during
 * queue creation.
 * If an internal reference to the message is being held then the message is placed on the reference
 * queue rather than freed and freed only when no references are held.
 */
static void
clean(qQueue *queue)
{
	qMessage *head, *tail;

	UT_DBGOUT(3, ("<UT> cleaning queue "UT_POINTER_SPEC"\n", queue));

	/* we don't do anything sophisticated for the reference queue as it should remain quite limited */
	do {
		/* a reference count can't increase once it's on the reference queue */
		tail = queue->referenceQueue;
		UT_DBGOUT(4, ("<UT> checking reference queue message "UT_POINTER_SPEC"\n", tail));
		if (tail == NULL || tail->referenceCount > 0) {
			/* nothing to collect */
			tail = NULL;
			break;
		}
		/* TODO: this is an ABA fallacy, is it a problem? */
	} while (!twCompareAndSwapPtr((uintptr_t *)&queue->referenceQueue, (uintptr_t)tail, (uintptr_t)tail->nextSecondary));

	/* if there's a referenceQueue item to collect, free it */
	if (tail != NULL) {
		tail->nextSecondary = NULL;
		UT_ATOMIC_SUB((volatile uint32_t*)&tail->pauseCount, UNTAGGED);
		UT_DBGOUT(5, ("<UT> freeing buffer "UT_POINTER_SPEC"\n", tail));
		freeBuffers(tail);
		UT_DBGOUT(4, ("<UT> freed buffer "UT_POINTER_SPEC"\n", tail));
	}

	tail = queue->tail;
	UT_DBGOUT(4, ("<UT> checking free queue message "UT_POINTER_SPEC", (pause=%i, subscriptions=%i)\n", tail, tail ? tail->pauseCount : 0, tail ? tail->subscriptions : 0));

	for (;tail != NULL && tail->subscriptions == 0 && tail->pauseCount == 0 && !queue->pause; tail = queue->tail) {
		qMessage *queueTail = NULL;
		int32_t tailSubs = 0;
		int32_t paused = FALSE;

		/* make sure that nothing's tried to pause this message since we entered the loop, and stop anything from succeeding in pausing it
		 * We set pause back to zero before finishing in this loop in cases where the message could be left on the queue.
		 */
		if (compareAndSwapUDATA((uintptr_t*)&tail->next, (uintptr_t)CLEANING_MSG_FLAG, (uintptr_t)CLEANING_MSG_FLAG) == (uintptr_t)CLEANING_MSG_FLAG) {
			/* we've picked up this message before it's finished being published and msg->next has yet to be nulled */
			return;
		}

		if (!twCompareAndSwap32((uint32_t*)(&tail->pauseCount), 0, UNTAGGED)) {
			/* this message was either paused or some other thread has started cleaning it */
			UT_DBGOUT(2, ("<UT> skipping cleaning for paused free queue message "UT_POINTER_SPEC", (pause=%i, subscriptions=%i)\n", tail, tail ? tail->pauseCount : 0, tail ? tail->subscriptions : 0));
			return;
		}


		/* make sure no one else will be cleaning this tail and that it's valid for us to remove it from the queue */
		queueTail = (qMessage*)compareAndSwapUDATA((uintptr_t*)&queue->tail, (uintptr_t)CLEANING_MSG_FLAG, (uintptr_t)CLEANING_MSG_FLAG);
		tailSubs = (uint32_t)compareAndSwapU32((uint32_t*)&tail->subscriptions, 0, 0);
		paused = (int32_t)compareAndSwapU32((uint32_t*)&queue->pause, 0, 0);
		if (queueTail != tail || tailSubs != 0 || paused != 0) {
			UT_ATOMIC_SUB((volatile uint32_t*)&tail->pauseCount, UNTAGGED);
			return;
		}

		UT_DBGOUT(5, ("<UT> processing free queue message "UT_POINTER_SPEC", (pause=%i, subscriptions=%i)\n", tail, tail ? tail->pauseCount : 0, tail ? tail->subscriptions : 0));
		/* we make sure that tail->next is non-null. If tail == head this prevents new items being queued while we empty the queue
		 * and causing issues for the correct updating of queue->tail. If tail != head then the update fails.
		 * Tagging head in this way prevents the possibility of both publish paths being taken at once while cleaning
		 */
		if (!twCompareAndSwapPtr((uintptr_t *)&tail->next, (uintptr_t)NULL, (uintptr_t)CLEANING_MSG_FLAG)) {
			/* if head != NULL then we want to grab the next on the queue */
			if (!twCompareAndSwapPtr((uintptr_t *)&queue->tail, (uintptr_t)tail, (uintptr_t)tail->next)) {

				/* this should never happen... we shouldn't be able to get this far with the same message, but leaving it in for now */
				DBG_ASSERT(0);

				/* a different thread has cleaned tailed from the queue already */
				UT_DBGOUT(1, ("<UT ERROR> not cleaning tail "UT_POINTER_SPEC" because it's either flagged (tail->next = "UT_POINTER_SPEC") or no longer tail despite the fact we're guarded by the pause count\n", tail, tail->next));

				/* we paused this message and are not cleaning it so unpause it now */
				UT_ATOMIC_SUB((volatile uint32_t*)&tail->pauseCount, UNTAGGED);
				return;
			}

			/* we use cmpxchg to set this as that's how we check it */
			twCompareAndSwapPtr((uintptr_t *)&tail->next, (uintptr_t)tail->next, (uintptr_t)CLEANING_MSG_FLAG);
			UT_DBGOUT(4, ("<UT> new tail is "UT_POINTER_SPEC"\n", tail->next));
		} else {
			/* head and tail were the same, so we empty the queue, head first to preserve the invariant requirement */
			if (!twCompareAndSwapPtr((uintptr_t *)&queue->head, (uintptr_t)tail, (uintptr_t)NULL)) {
				/* consumers have run ahead of the publishers and have finished with this message before it's been set
				 * as head. We have to allow the publish to complete before we can clean so we let the next clean take care
				 * of this.
				 */
				UT_DBGOUT(3, ("<UT> consumer run ahead, so letting publishing catch up. Tail is "UT_POINTER_SPEC"\n", tail));

				/* the publish of this message as head could continue without us unsetting the flag, however it'll block subsequent publishes until this is done */
				UT_ASSERT(twCompareAndSwapPtr((uintptr_t *)&tail->next, (uintptr_t)CLEANING_MSG_FLAG, (uintptr_t)NULL));

				UT_ATOMIC_SUB((volatile uint32_t*)&tail->pauseCount, UNTAGGED);
				return;
			}

			/* this could fail if, once we've nulled head, a new message is published as head&tail before we hit here, so we
			 * fail quietly.
			 */
			twCompareAndSwapPtr((uintptr_t *)&queue->tail, (uintptr_t)tail, (uintptr_t)NULL);
			UT_DBGOUT(3, ("<UT> queue emptied, last message was "UT_POINTER_SPEC"\n", tail));
		}

		/* by this point tail->next == CLEANING_MSG_FLAG regardless of path */

		/* if there are still referees for this item, move to the reference queue, otherwise free it */
		if (tail->referenceCount > 0) {
			do {
				head = queue->referenceQueue;
				tail->nextSecondary = head;
			} while (!twCompareAndSwapPtr((uintptr_t *)&queue->referenceQueue, (uintptr_t)head, (uintptr_t)tail));
			UT_DBGOUT(4, ("<UT> moved buffer "UT_POINTER_SPEC" to reference queue\n", tail));
		} else {
			UT_ATOMIC_SUB((volatile uint32_t*)&tail->pauseCount, UNTAGGED);
			UT_DBGOUT(4, ("<UT> freeing buffer "UT_POINTER_SPEC"\n", tail));
			freeBuffers(tail);
		}
	}

	/* TODO: add in some consistency checks to clean so that if something does damage the state of the queue then we don't swallow
	 * all of the memory available. A basic check would be to ensure that we never have more items on the reference queue than we
	 * have subscribers given it's a maximum of one each.
	 */
}

/*
 * Creates an n-to-n queue that dispatches all published messages to all subscribers. Once messages have been
 * consumed by all subscribers the function pointer "free" is used to release them. If memory for queue management
 * data is allocated by this function then it will be freed by destroyQueue, if memory is passed in then it will
 * not be freed.
 *
 * queue - a ** to memory to use for the queue management data. If *queue is null then memory will be allocated
 *		 by this function.
 * free  - function used to free messages once they are finished with
 */
omr_error_t
createQueue(qQueue **queue)
{
	qQueue *que;
	omr_error_t result = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (queue == NULL) {
		UT_DBGOUT(2, ("<UT> NULL queue pointer passed to createQueue\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* malloc memory if none was passed in */
	if (*queue == NULL) {
		*queue = j9mem_allocate_memory(sizeof(qQueue), OMRMEM_CATEGORY_TRACE);
		if (*queue == NULL) {
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		(*queue)->allocd = TRUE;
	} else {
		(*queue)->allocd = FALSE;
	}

	UT_DBGOUT(1, ("<UT> creating queue at "UT_POINTER_SPEC"\n", *queue));

	que = *queue;

	que->alive = TRUE;
	que->head = NULL;
	que->tail = NULL;
	que->subscriptions = 0;
	que->subscribers = NULL;
	que->referenceQueue = NULL;
	que->pause = FALSE;

	result = initEvent(&que->alarm, "Trace Queue Alarm");
	if (OMR_ERROR_NONE != result) {
		UT_DBGOUT(1, ("<UT> failed to create queue alarm, returned: %i\n", result));
		goto out;
	}

	result = (int32_t)omrthread_monitor_init_with_name((omrthread_monitor_t*)&que->lock, 0, "Trace Queue");
	if (OMR_ERROR_NONE != result) {
		UT_DBGOUT(1, ("<UT> failed to create queue mutex, returned: %i\n", result));
		destroyEvent(que->alarm);
		goto out;
	}

out:
	if (OMR_ERROR_NONE != result) {
		que->alive = FALSE;
		if(que->allocd) {
			j9mem_free_memory(que);
		}

		que = NULL;
	}

	return result;
}

/*
 * Clean up function for the queue. This will invalidate the queue and block until
 * all subscribers have stopped. If memory was allocated by createQueue for queue management
 * data then it will be freed here.
 */
void
destroyQueue(qQueue *queue)
{
	omrthread_monitor_t lock;
	UtEventSem *alarm;
	int32_t subscriptions;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> entered destroy queue for "UT_POINTER_SPEC"\n", queue));
	if (queue == NULL) {
		return;
	}

	issueWriteBarrier();
	lock = queue->lock;

	if (lock == NULL) {
		UT_DBGOUT(1, ("<UT> tried to destroy destroyed queue "UT_POINTER_SPEC"\n", queue));
		return;
	}

	omrthread_monitor_enter(queue->lock);

	alarm = queue->alarm;
	subscriptions = queue->subscriptions;

	queue->alive = FALSE;

	postEventAll(alarm);

	if (subscriptions != 0) {
		UT_DBGOUT(1, ("<UT> destroy returning while %i subscribers finish\n", subscriptions));
		omrthread_monitor_exit(lock);
		return;
	}

	UT_DBGOUT(3, ("<UT> destroying queue\n"));

	queue->lock = NULL;
	queue->alarm = NULL;

	clean(queue);

	/* we don't exit the mutex before destroying or it'll open a timing hole */
	omrthread_monitor_destroy(lock);
	destroyEvent(alarm);

	if (queue->allocd) {
		j9mem_free_memory(queue);
	}

	UT_DBGOUT(1, ("<UT> destroyed queue "UT_POINTER_SPEC"\n", queue));
}


/*
 * Publishes a message to the queue, either as the head if the list is empty, or as a precursor to head
 * otherwise. Queued messages can be obtained by a call to acquireNextMessage with a valid subscription.
 * This function will contest with other threads publishing to the queue and may spin until atomic updates
 * succeed.
 *
 * Removal of a buffer from this queue has the invariant requirement that head must always be nulled before tail or
 * you can end up in a state where tail remains NULL despite buffers on the queue.
 */
int32_t
publishMessage(qQueue *queue, qMessage *msg)
{
	qMessage * volatile head;
	int subscriptions;

	if (!queue->alive) {
		UT_DBGOUT(4, ("<UT> not publishing message "UT_POINTER_SPEC" to dead queue "UT_POINTER_SPEC"\n", msg, queue));
		return FALSE;
	}

	/* so we can tell if we've been queued and not yet had the subscription set */
	if (msg == NULL || compareAndSwapU32((uint32_t *)&msg->subscriptions, 0, UNTAGGED) != 0) {
		UT_DBGOUT(1, ("<UT> Dropping message "UT_POINTER_SPEC" instead of publishing to queue "UT_POINTER_SPEC" due to invalid message state\n", msg, queue));
		return FALSE;
	}


	do {
		/* refreshing the value of subscriptions within the loop here and prior to swapping (if needed)
		 * guarantees time ordering of the subscription counts in the output queue. This is necessary for the
		 * tagging to work. We have to set the value of head within the loop because it can unblock publishing
		 * as soon as the queue->head = msg swap completes.
		 */
		head = queue->head;

		if (head == NULL) {
			subscriptions = queue->subscriptions;
			twCompareAndSwap32((uint32_t*)&msg->subscriptions, UNTAGGED, subscriptions);
			if (twCompareAndSwapPtr((uintptr_t *)&queue->head, (uintptr_t)NULL, (uintptr_t)msg)) {
				break;
			} else {
				twCompareAndSwap32((uint32_t*)&msg->subscriptions, subscriptions, UNTAGGED);
			}
		} else {
			/* It's possible that there'll be a delay here, one long enough that the local "head" gets replaced, processed,
			 * cleaned, freed and, potentially, re-queued.
			 *
			 * In this lifecycle there is only one point at which it is valid for us to execute the instruction below, therefore
			 * qMessage->next must be non null throughout the entirety of that lifecycle. This is why next is set to CLEANING_MSG_FLAG
			 * when a message is cleaned. We shouldn't be freeing memory used for messages while the queue's still alive unless
			 * we can be sure there's nothing in publish.
			 */
			if (twCompareAndSwapPtr((uintptr_t *)&head->next, (uintptr_t)NULL, (uintptr_t)msg)) {
				break;
			}
		}
	} while (TRUE);

	/* All other contenders for non-null head will block at the cmpxchg until queue->head->next == NULL */
	if (head != NULL) {
		/* store this here for time ordering. This has to be cached before we allow competitors out of the cmpxchg loop */
		subscriptions = queue->subscriptions;

		/* this assignment allows competitors for the cmpxchg loop above to continue when msg->next is null*/
		if (!twCompareAndSwapPtr((uintptr_t *)&queue->head, (uintptr_t)head, (uintptr_t)msg)) {
			UT_ASSERT(0);
		}

		if (queue->tail == NULL) {
			UT_DBGOUT(2, ("<UT> tail is null and replacing queue->head "UT_POINTER_SPEC" with "UT_POINTER_SPEC"\n", head, msg));
		}


		/* don't bother preventing reordering of subscriptions update and the head update as it doesn't actually matter. It may
		 * block for longer if the subscription update is done first.
		 */
		UT_ATOMIC_SUB((volatile uint32_t*)&msg->subscriptions, UNTAGGED - subscriptions);
	}

	/* Does tail need setting? This must used the saved value for head as only one contender
	 * will exit the cmpxchg with trcHead as null
	 */
	if (head == NULL) {
		qMessage *currentTail;

		/* tail could be null or old value of queue->tail */
		do {
			currentTail = queue->tail;
		} while (!twCompareAndSwapPtr((uintptr_t *)&queue->tail, (uintptr_t)currentTail, (uintptr_t)msg));
		UT_DBGOUT(4, ("<UT> msg "UT_POINTER_SPEC" is new head&tail, tail was "UT_POINTER_SPEC" for queue "UT_POINTER_SPEC"\n", msg, currentTail, queue));
	}

	/* ensure that we can queue new messages now that msg has a fixed place in the queue */
	/* CMVC 149252: moved this from just after the cmpxchg loop to here so that publishing is guaranteed to have finished */
	msg->next = NULL;
	return TRUE;
}

/*
 * Not reentrant with the same subscription and may block if no messages are available.
 *
 * Obtains the next message on the queue for the provided subscription
 */
qMessage *
acquireNextMessage(qSubscription *sub)
{
#ifdef J9ZOS390
/* defect 157477 */
#pragma option_override(acquireNextMessage, "opt(level, 0)")
#endif

	qMessage *msg = NULL;
	qQueue *queue = sub->queue;

	if (sub == NULL || sub->queue == NULL) {
		return NULL;
	}

	queue = sub->queue;
	if (queue == NULL) {
		return NULL;
	}

	if (sub->currentLocked) {
		releaseCurrentMessage(sub);
	}

	/* have we hit the stop point? Done at the start rather than the
	 * end to force a run through the clean up
	 */
	if (sub->current != NULL && sub->current == sub->stop) {
		UT_DBGOUT(2, ("<UT> subscription "UT_POINTER_SPEC" has reached it's stop bound\n", sub));
		sub->valid = FALSE;
	}

	/* fast path check */
	if (sub->current != NULL) {
		msg = sub->current->next;
	}

	if (!IS_VALID_MSG_PTR(msg)) {
		while (!IS_VALID_MSG_PTR(msg) && sub->valid == TRUE) {
			/* we take a local copy of the value of head at this point so our logic isn't
			 * dealing with potentially changing values across conditions in if tests
			 */
			qMessage *head = queue->head;

			if (head != NULL && (sub->current == NULL || head != sub->current)) {
				/* looks like something's been added */
				if (sub->current != NULL) {
					msg = sub->current->next;
				}

				if (!IS_VALID_MSG_PTR(msg)) {
					/* If we're here and IS_VALID_MSG_PTR(msg) == FALSE then current could be:
					 * 	A. in the middle of being published, not yet assigned to head (but will be), or
					 * 	B. on the reference queue
					 *
					 * We do:
					 *  for A: wait for publishing of current to complete by falling through to waitEvent
					 *  for B: pick up tail
					 *
					 * Case 2 is generally distinguishable by current->subscriptions == 0. It's possible that this will
					 * match case 1 as well through various timings, but in that case queue->tail will also have 0
					 * subscriptions and the subsequent test will catch it.
					 *
					 * sub->current == NULL means we're working from the tail.
					 *
					 * under no circumstances will we ever want to pick up a message that's got a subscription count of zero.
					 * This could happen if head's nulled as the queue's emptied, subscriber is set to work from tail and picks
					 * up the expired tail before it's nulled.
					 * compareAndSwapX(y, z, z) will return y if the value is/was y otherwise the actual value of x so this can
					 * be used as if it's the variable but in a reliable fashion when using compare/swap
					 */
					if (sub->current == NULL || sub->current->subscriptions == 0) {
						msg = queue->tail;

						if  (msg == sub->current || !IS_VALID_MSG_PTR(msg)
							|| compareAndSwapU32((uint32_t*)&msg->subscriptions, 0, 0) == 0
							|| compareAndSwapUDATA((uintptr_t*)&queue->tail, 0, 0) != (uintptr_t)msg) {

							UT_DBGOUT(2, ("<UT> subscription "UT_POINTER_SPEC" picked up current from tail of queue, reverting to null\n", sub));
							msg = NULL;
							omrthread_yield();
							continue;
						}
					}
				}
			} else if (head == NULL) {
				/* head's null and we're waiting so anything new is valid. No need to maintain last processed state */
				qMessage *tmpMessage = sub->current;

				UT_DBGOUT(2, ("<UT> subscription "UT_POINTER_SPEC" now working from tail of queue\n", sub));

				/* reset subscription to work from tail */
				WORK_FROM_TAIL(sub);

				/* we're done with current. */
				if (tmpMessage != NULL) {
					UT_ASSERT(sub->savedReference == TRUE);
					sub->savedReference = FALSE;
					UT_ATOMIC_DEC((volatile uint32_t*)&tmpMessage->referenceCount);
				}
			}

			if (!IS_VALID_MSG_PTR(msg)) {
				/* TODO: add a new state, moribund, that prevents queuing new buffers and means subscribers exit rather than wait */
				if (queue->alive) {
					UtEventSem *alarm = queue->alarm;
					UT_DBGOUT(2, ("<UT> subscription "UT_POINTER_SPEC" waiting for message to be published to queue "UT_POINTER_SPEC"\n", sub, queue));
					if (alarm != NULL) {
						waitEvent(queue->alarm);
					}
				} else {
					/* need to notify in case we stole the posted state of alarm */
					notifySubscribers(queue);
					break;
				}
			}
		}
	}

	if (!IS_VALID_MSG_PTR(msg) && sub->valid) {
		/* error or queue is dead so must set state and finish */
		sub->valid = FALSE;
		WORK_FROM_TAIL(sub);

		UT_DBGOUT(1, ("<UT> queue "UT_POINTER_SPEC" is dead or error for subscription "UT_POINTER_SPEC"\n", queue, sub));
		return NULL;
	}

	/* we test this before and after setting last so that we can be sure of state in unsubscribe */
	if (!sub->valid) {
		UT_DBGOUT(1, ("<UT> subscription "UT_POINTER_SPEC" unsubscribed so exiting. current "UT_POINTER_SPEC", last: "UT_POINTER_SPEC"\n", sub, sub->current, sub->last));
		/* we're unsubscribed so don't need to set any state */
		return NULL;
	}

	/* this allows deregistration to determine if it's record of "current" straddled the termination check as there's
	 * only a small period of time where current == last
	 */
	UT_DBGOUT(4, ("<UT> setting last "UT_POINTER_SPEC" to current "UT_POINTER_SPEC"\n", sub->last, sub->current));
	sub->last = sub->current;

	if (!sub->valid) {
		/* we've been unsubscribed, so untag from last not current */
		sub->current = NULL;

		UT_DBGOUT(1, ("<UT> subscription "UT_POINTER_SPEC" is unsubscribed\n", sub));
		return NULL;
	}

	/* we now longer have the possibility of needing a reference to the current last in unsubscribe, so we're done with it */
	if (sub->last != NULL && sub->savedReference == TRUE) {
		UT_DBGOUT(5, ("<UT> removing reference to message "UT_POINTER_SPEC"\n", queue, sub->last));
		sub->savedReference = FALSE;
		UT_ATOMIC_DEC((volatile uint32_t*)&sub->last->referenceCount);
	}

	/* TODO: add test that unsubscribes this subscription while we block here */

	/* we may need to keep a reference to this message so that its next pointer remains valid */
	UT_DBGOUT(5, ("<UT> saving reference to message "UT_POINTER_SPEC"\n", queue, msg));
	sub->savedReference = TRUE;
	UT_ATOMIC_INC((volatile uint32_t*)&msg->referenceCount);

	sub->currentLocked = TRUE;

	/* setting current frees up any pending unsubscribe to continue running */
	issueWriteBarrier();
	sub->current = msg;

	UT_DBGOUT(5, ("<UT> returning "UT_POINTER_SPEC" for subscription "UT_POINTER_SPEC"\n", msg, sub));
	return msg;
}

/*
 * This method is used to release our lock on the subscriptions current message. This will be done
 * by acquireNextMessage if not called explicitly.
 * The only time this is likely to be used is if it's necessary to avoid a potentially blocking call
 * to acquireNextMessage but the current message is finished with.
 *
 * subscription - the lock for the subscriptions current message will be released
 */
void
releaseCurrentMessage(qSubscription *sub)
{
	qMessage *current;
	qQueue *queue;
	if (sub == NULL) {
		UT_DBGOUT(1, ("<UT> request to release message for null subscription\n"));
		return;
	} else {
		current = sub->current;
		if (current == NULL) {
			UT_DBGOUT(1, ("<UT> request to release invalid message for subscription "UT_POINTER_SPEC"\n", sub));
			return;
		}
	}


	queue = sub->queue;
	if (sub->currentLocked) {
		UT_ATOMIC_DEC((volatile uint32_t*)&current->subscriptions);

		sub->currentLocked = FALSE;

		UT_DBGOUT(5, ("<UT> message "UT_POINTER_SPEC" has %i subscribers remaining after release\n", current, current->subscriptions));

		if (queue != NULL) {
			clean(queue);
		}
	}

	if (queue == NULL && !sub->valid && sub->savedReference == TRUE) {
		uint32_t oldFlag;

		/* TODO: set up savedReference cleanup on unsubscribe so that it doesn't require
		 * atomic access.
		 * Use test/set at the end of unsubscribe and acquireNextMessage to determine
		 * which does clean up.
		 */
		do {
			oldFlag = sub->savedReference;
		} while (!twCompareAndSwap32((uint32_t*)&sub->savedReference, TRUE, FALSE));


		if (oldFlag == TRUE) {
			UT_ATOMIC_DEC((volatile uint32_t*)&current->referenceCount);
		}
	}
}


/*
 * Creates a subscription for the queue. Subscriptions to a queue are intended for use within a single thread.
 * Any functions taking a subscription as a parameter may be used simultaneously by different threads so
 * long as each passes a different subscription.
 *
 * It is not valid to call, for example, releaseMessage and acquireNextMessage concurrently on different threads
 * with the same subscription.
 *
 * subscription - ** to memory used for subscription management data. If *subscription is NULL then memory
 *				will be allocated within this function.
 *				NOTE: *subscription == NULL is currently not allowed due to issues with cleanup on unsubscribe.
 * start - the message to start processing from, this can be three things:
 *		 1. -1   - this indicates that processing should start at the current head of the queue.
 *		 2. NULL - this indicates that processing should start at the current tail of the queue
 *		 3. a pointer to a message on the queue. If the message can't be found the behavior is as if
 *			-1 was specified.
 * stop - the last message to process (inclusive). The subscriber will be unsubscribed by the next call
 *		to acquireNextMessage and will return NULL. If stop is NULL the subscriber will continue
 *		indefinitely.
 *
 * Returns:
 *  OMR_ERROR_OUT_OF_NATIVE_MEMORY - memory couldn't be allocated during subscription
 *  OMR_ERROR_ILLEGAL_ARGUMENT - one of the parameters passed in was invalid. If *subscription != NULL then this
 *				is advisory only and the subscription can be used.
 * OMR_ERROR_NOT_AVAILABLE - the maximum number of subscriptions has been reached
 */
omr_error_t
subscribe(qQueue *queue, qSubscription **subscription, qMessage *start, qMessage *stop)
{
	int32_t originalCount, tagValue, threshold;
	qMessage *fixupHead, *fixupTail, *lastNeedingInc;
	qMessage *tailA = NULL, *tailB = NULL, *workingCurrent = NULL;
	qSubscription *sub = NULL;
	omr_error_t result = OMR_ERROR_NONE;

	if (subscription == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (queue == NULL || !queue->alive) {
		*subscription = NULL;
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	UT_DBGOUT(1, ("<UT> new subscription to queue, "UT_POINTER_SPEC"\n", *subscription));

	omrthread_monitor_enter(queue->lock);

	/* malloc memory if none was passed in */
	if (*subscription == NULL) {
		/* TODO: unless we can determine if there is a thread in acquireNextMessage we can't safely free the subscription
		 * within unsubscribe. As such this is currently disabled.
		 */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
#if 0
		*subscription = j9mem_allocate_memory(sizeof(qSubscription), OMRMEM_CATEGORY_TRACE);
		if (*subscription == NULL) {
			result = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			UT_DBGOUT(1, ("<UT> out of memory creating subscription\n"));
			goto out;
		}
		(*subscription)->allocd = TRUE;
#endif
	} else {
		(*subscription)->allocd = FALSE;
	}

	UT_DBGOUT(1, ("<UT> subscription created at "UT_POINTER_SPEC"\n", *subscription));

	sub = *subscription;

	sub->stop = stop;
	sub->last = (qMessage*)-1;
	sub->prev = NULL;
	sub->next = NULL;
	sub->currentLocked = FALSE;
	sub->valid = TRUE;
	sub->queue = queue;
	sub->savedReference = FALSE;

	if (queue->subscribers == NULL) {
		queue->subscribers = sub;
	} else {
		sub->next = queue->subscribers;
		queue->subscribers->prev = sub;
		queue->subscribers = sub;
	}

	originalCount = queue->subscriptions;
	tagValue = originalCount + originalCount + 2;
	threshold = originalCount + 2;

	/* check for tagValue wrap */
	if (tagValue < threshold) {
		UT_DBGOUT(1, ("<UT> reached the maximum number of subscribers (%i) for queue "UT_POINTER_SPEC"\n", queue->subscriptions, queue));
		result = OMR_ERROR_NOT_AVAILABLE;
		goto out;
	}

	/* global pause to catch newly queued messages */
	twCompareAndSwap32((uint32_t*)&queue->pause, 0, 1);

	/* Freeze the tail if non null to prevent it being cleaned. This also prevents us picking up a message
	 * that's in the throes of being cleaned already */
	do {
		qMessage *qTail = NULL;
		uint32_t tailPauseCount = 0;

		tailA = queue->tail;
		if (tailA != NULL) {
			UT_ATOMIC_INC((volatile uint32_t*)&tailA->pauseCount);
			tailPauseCount = (uint32_t)compareAndSwapU32((uint32_t*)&tailA->pauseCount, 0, 0);
		}

		tailB = queue->tail;

		/* this is a bit of overkill because one of the last two conditions should always be true if the first
		 * condition is true, but I think the first condition is the most common scenario.
		 */
		qTail = (qMessage*)compareAndSwapUDATA((uintptr_t*)&queue->tail, 0, 0);
		if (tailA != tailB || qTail != tailA || tailPauseCount >= UNTAGGED) {
			/* the tail of the output queue changed under us so undo and retry */
			if (tailA != NULL) {
				UT_ATOMIC_DEC((volatile uint32_t*)&tailA->pauseCount);
			}
			tailA = NULL;
		}
	} while (tailA != tailB);

	/* no instruction barrier or memory barrier needed in block because the queue values are declared volatile */
	/* if head's non null at this assignment then it can't be cleaned because of the global pause */
	fixupTail = queue->head;
	queue->subscriptions = tagValue;
	fixupHead = queue->head;
	queue->subscriptions = originalCount + 1;

	/* basic case first*/
	if (queue->head == NULL) {
		/* we can't remove tagged buffers or buffers with the incremented count, so if head is null current should be null */
		workingCurrent = NULL;
	} else {
		/* if the queue was empty or fixupTail has been cleaned then we work from tail. Tagged values can't have been cleaned, nor
		 * can messages with the new subscription count */
		if (fixupTail == NULL) {
			fixupTail = queue->tail;
		}

		/* when non-null fixupHead is either tagged or has the original Value. Either way we don't want to go past it
		 * while looking for untagged values or we could start roaming into the buffers with the new count. Initial fixupTail
		 * value can't be tagged, so we start looking at fixupTail->next
		 */
		if (fixupHead != NULL) {
			while (fixupTail != NULL) {
				if (fixupTail == fixupHead) {
					break;
				}

				/* if the subscription count for the message hasn't yet been set then we can't tell it if was
				 * tagged or not so check again until it is set.
				 */
				while (fixupTail->next->subscriptions == UNTAGGED) {
					omrthread_yield();
				}

				if (IS_VALID_MSG_PTR(fixupTail->next) && fixupTail->next->subscriptions >= threshold) {
					break;
				}

				fixupTail = fixupTail->next;
			}
		} else {
			fixupTail = queue->tail;
		}

		/* sub->current->next will be the first buffer processed by the subscriber */
		workingCurrent = fixupTail;
		lastNeedingInc = workingCurrent;

		/* move fixupTail on to the first message tagged or with the new count */
		if (fixupTail != NULL) {
			fixupTail = fixupTail->next;
		}

		while (IS_VALID_MSG_PTR(fixupTail) && fixupTail->subscriptions >= threshold) {
			if (fixupTail->subscriptions == UNTAGGED) {
				omrthread_yield();
				continue;
			}

			UT_ATOMIC_SUB((volatile uint32_t*)&fixupTail->subscriptions, threshold - 1);
			fixupTail = fixupTail->next;
		}

		/* we're currently set to work from head->next as it was when we registered.
		 * We now need to deal with the case where something else was requested
		 */
		if (start != (qMessage*)-1) {
			UT_DBGOUT(5, ("<UT> revising initial message for subscription "UT_POINTER_SPEC" to match start bound "UT_POINTER_SPEC"\n", sub, start));

			/* If start != NULL then we scan forward from tail to find where we start from. We
			 * mustn't go past our current starting buffer or we would be expected to process buffers
			 * prior to our start point.
			 */
			if (start != NULL && tailA != start) {
				while (IS_VALID_MSG_PTR(tailA) && tailA->next != start && tailA != lastNeedingInc) {
					tailA = tailA->next;
				}

				if (!IS_VALID_MSG_PTR(tailA) || tailA == lastNeedingInc) {
					/* we were passed a buffer that's no longer on the queue */
					result = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					workingCurrent = tailA;
					tailA = tailA->next;
				}
			} else {
				/* we want the tail */
				workingCurrent = NULL;
			}

			/* we want to increment from start (tailA) to the head. We don't stop at "stop" because
			 * it makes very hard to unsubscribe as the buffer subscriber counts don't have a direct
			 * relation to the queue subscriber count. Instead we invalidate the subscriber when it
			 * sees stop and let unsubscribe decrement any unprocessed buffers.
			 */
			if (workingCurrent != lastNeedingInc) {
				while (IS_VALID_MSG_PTR(tailA)) {
					UT_ATOMIC_INC((volatile uint32_t*)&tailA->subscriptions);
					if (tailA == lastNeedingInc) {
						break;
					}
					tailA = tailA->next;
				}
			}
		}

		/* increase the reference count for the new subscriptions current reference */
		if (workingCurrent != NULL) {
			sub->savedReference = TRUE;
			UT_ATOMIC_INC((volatile uint32_t*)&workingCurrent->referenceCount);
		}
	}

	/* unpause the cleaning */
	if (tailB != NULL) {
		UT_ATOMIC_DEC((volatile uint32_t*)&tailB->pauseCount);
	}

	/* allow global cleaning to continue */
	twCompareAndSwap32((uint32_t*)&queue->pause, 1, 0);


	sub->current = workingCurrent;
	if (sub->current != NULL) {
		UT_DBGOUT(5, ("<UT> reference message for subscription "UT_POINTER_SPEC" is "UT_POINTER_SPEC", first to process is "UT_POINTER_SPEC"\n", sub, workingCurrent, workingCurrent->next));
	} else {
		UT_DBGOUT(5, ("<UT> initial message for subscription "UT_POINTER_SPEC" will be tail\n", sub));
	}

out:
	omrthread_monitor_exit(queue->lock);

	/* we don't release malloc'd memory here on failure because it makes it possible to ask for start/stop
	 * bounds in a fail soft fashion
	 */
	return result;
}

/*
 * Allows a subscriber to unsubscribe from a queue. A subscriber MUST be unsubscribed from a queue if
 * they are going to cease processing messages. Failure to do so will cause messages to back up in
 * memory.
 * If the subscriber was registered with an alarm function and that was called prior to unsubscription
 * then the subscriber was forcibly unsubscribed due to an error or the queue shutting down. In this
 * case unsubscription is not needed.
 *
 * If the memory for the subscription was allocated by "subscribe" rather than passed in then this function will
 * invalidate ALL state for the subscription. If memory was passed into subscribe then any existing data will remain
 * valid and any messages held must be released before references to them are discarded.
 */
omr_error_t
unsubscribe(qSubscription *sub)
{
	int32_t originalCount, tagValue;
	qMessage *fixupHead, *fixupTail;
	qQueue *queue = NULL;
	omr_error_t result = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (sub == NULL || sub->queue == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	sub->valid = FALSE;
	queue = sub->queue;
	sub->queue = NULL;

	notifySubscribers(queue);

	UT_DBGOUT(1, ("<UT> unsubscribing "UT_POINTER_SPEC" from queue "UT_POINTER_SPEC"\n", sub, queue));

	omrthread_monitor_enter(queue->lock);

	/* this ensures we get the value of current either before the null check or after, but not interleaved */
	do {
		omrthread_yield();
		fixupTail = sub->current;
	} while (sub->current == sub->last);

	originalCount = queue->subscriptions;

	/* tag any new buffers */
	tagValue = 0 - originalCount;
	queue->subscriptions = tagValue;
	issueReadBarrier();
	fixupHead = queue->head;
	queue->subscriptions = originalCount - 1;

	/* check the most basic case */
	/* we can't have tagged anything if head is null, because tagged buffers can't be removed. We must have processed all our
	 * buffers or they would be on the queue still, so nothing to be done.
	 */
	issueReadBarrier();
	if (queue->head != NULL) {
		/* from the buffer after current we need to decrement the count. If current isn't valid then the buffer after the last processed, i.e.
		 * the one that would be current if the consumer retrieved another buffer.
		 */
		qMessage *reference = NULL;

		if (fixupTail == NULL) {
			/* the subscriber has exited, so work from subscription->last rather than current */
			UT_DBGOUT(5, ("<UT> subscriber had exited so working from "UT_POINTER_SPEC" instead of "UT_POINTER_SPEC"\n", sub->last, sub->current));
			fixupTail = sub->last;
			if (fixupTail == (qMessage*)-1) {
				/* the subscriber never did any work on the current queue and had no defined start point, so work from tail */
				fixupTail = queue->tail;
			} else {
				reference = fixupTail;
				fixupTail = fixupTail->next;
			}
		} else {
			reference = fixupTail;
			fixupTail = fixupTail->next;
		}

		if (fixupTail == CLEANING_MSG_FLAG) {
			fixupTail = queue->tail;
		}

		/* unless we were working from tail the message we're working from needs it's reference count decrementing. The unsubscribe is left with
		 * this job because we need it to not be cleaned while we're in this code
		 */
		if (reference != NULL) {
			UT_ATOMIC_DEC((volatile uint32_t*)&reference->referenceCount);
		}

		/* fixupHead is either tagged or has the original Value if it's non null and is not the same as reference (reference has already been processed
		 * by the subscription). Either way we don't want to go past it while looking for untagged values or we could start trampling the buffers with the new count
		 */
		if (fixupHead != NULL && fixupHead != reference) {
			while (IS_VALID_MSG_PTR(fixupTail) && fixupTail->subscriptions > tagValue) {
				if (fixupTail->subscriptions == UNTAGGED) {
					omrthread_yield();
					continue;
				}

				UT_ATOMIC_DEC((volatile uint32_t*)&fixupTail->subscriptions);
				UT_DBGOUT(5, ("<UT> fixed up subscription count for "UT_POINTER_SPEC", new count is %i\n", fixupTail, fixupTail->subscriptions));

				if (fixupTail == fixupHead) {
					fixupTail = fixupTail->next;
					break;
				}
				fixupTail = fixupTail->next;
			}
		} else {
			fixupTail = queue->tail;
		}

		while (IS_VALID_MSG_PTR(fixupTail)) {
			while (fixupTail->subscriptions == UNTAGGED) {
				omrthread_yield();
			}

			if (fixupTail->subscriptions <= tagValue) {
				UT_ATOMIC_ADD((volatile uint32_t*)&fixupTail->subscriptions, (2*originalCount) - 1);
				fixupTail = fixupTail->next;
			} else {
				break;
			}
		}
	}

	/* remove from the subscriber list */
	if (sub->prev != NULL) {
		sub->prev->next = sub->next;
	}
	if (sub->next != NULL) {
		sub->next->prev = sub->prev;
	}
	if (sub->prev == NULL) {
		queue->subscribers = sub->next;
	}

	omrthread_monitor_exit(queue->lock);

	clean(queue);

	notifySubscribers(queue);

	if (sub->allocd) {
		/* if we allocated the memory for the subscription we need to free it here. If that's the case then it invalidates
		 * all state for that subscription at that point so we need to clean up here. If the subscription is being used for
		 * acquiring or publishing a message then we can't free the memory because we need to reference the subscription in
		 * those functions.
		 */
		releaseCurrentMessage(sub);
		j9mem_free_memory(sub);
	}

	if ((originalCount - 1) == 0 && queue->alive == FALSE) {
		/* if this is the last subscriber exiting and the queue is dead then call back into destroy */
		destroyQueue(queue);
	}

	return result;
}


/*
 * Wakes all subscribers waiting for messages on the queue
 */
void
notifySubscribers(qQueue *queue)
{
	UtEventSem *alarm = queue->alarm;

	if (alarm != NULL) {
		postEventAll(alarm);
	}
}

/*
 * Calling this for a message that is queued or will be queued blocks freeing of
 * this and subsequent messages from the queue. This is useful mainly in
 * conjunction with a bounded subscription when you need to guarantee that a given
 * message will still be on the queue when subscribing.
 *
 * More than one actor can request that a given message pause freeing. All actors must
 * call resume for the message before freeing will continue.
 */
void
pauseDequeueAtMessage(qMessage *msg)
{
	UT_DBGOUT(1, ("<UT> pausing message "UT_POINTER_SPEC"\n", msg));
	UT_ATOMIC_INC((volatile uint32_t*)&msg->pauseCount);
}

/*
 * Allows this and subsequent messages to be freed from the queue.
 */
void
resumeDequeueAtMessage(qMessage *msg)
{
	UT_DBGOUT(1, ("<UT> unpausing message "UT_POINTER_SPEC"\n", msg));
	UT_ATOMIC_DEC((volatile uint32_t*)&msg->pauseCount);
}
