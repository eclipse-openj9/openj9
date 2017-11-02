/*[INCLUDE-IF Sidecar16]*/
package java.lang.ref;

import com.ibm.oti.vm.VM;

/*[IF Sidecar19-SE]*/
import jdk.internal.misc.JavaLangRefAccess;
import jdk.internal.misc.SharedSecrets;
/*[ENDIF]*/

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
 
/**
 * Abstract class which describes behavior common to all reference objects.
 *
 * @author		OTI
 * @version		initial
 * @since		1.2
 */	
public abstract class Reference<T> extends Object {
	private static final int STATE_INITIAL = 0;
	private static final int STATE_CLEARED = 1;
	private static final int STATE_ENQUEUED = 2;
	
	private T referent;
	private ReferenceQueue queue;
	private int state;

	/*[IF Sidecar19-SE]*/
	static {
		SharedSecrets.setJavaLangRefAccess(new JavaLangRefAccess() {
			public boolean waitForReferenceProcessing() throws InterruptedException {
				return waitForReferenceProcessingImpl();
			}
		});
	}
	
	/**
	 *  Wait for progress in reference processing.
	 * return false if there is no processing reference,
	 * return true after wait the notification from the reference processing thread if currently the thread is processing references. 
	 */
	static private native boolean waitForReferenceProcessingImpl();
	
	/*[ENDIF]*/
	
/**
 * Make the referent null.  This does not force the reference object to be enqueued.
 */	
public void clear() {
	synchronized(this) {
		referent = null;
		/* change the state to cleared if it's not already cleared or enqueued */
		if (STATE_INITIAL == state) {
			state = STATE_CLEARED;
		}
	}
}

/**
 * Force the reference object to be enqueued if it has been associated with a queue.
 *
 * @return	true if Reference is enqueued, false otherwise.
 */	
public boolean enqueue() {
	return enqueueImpl();
}

/**
 * Return the referent of the reference object.
 *
 * @return	the referent to which reference refers,
 *			or null if object has been cleared.
 */	
public T get() {
	if (VM.J9_GC_POLICY != VM.J9_GC_POLICY_METRONOME) {
		return referent;
	}
	return getImpl();
}

private native T getImpl();

/**
 * Return whether the reference object has been enqueued.
 *
 * @return	true if Reference has been enqueued, false otherwise.
 */	
public boolean isEnqueued () {
	synchronized(this) {
		return state == STATE_ENQUEUED;
	}
}

/**
 * Enqueue the reference object on the associated queue.
 *
 * @return	true if the Reference was successfully
 *			enqueued, false otherwise.
 */
boolean enqueueImpl() {
	final ReferenceQueue tempQueue;
	boolean result;
	T tempReferent = referent;
	synchronized(this) {
		/* Static order for the following code (DO NOT CHANGE) */
		tempQueue = queue;
		queue = null;
		if (state == STATE_ENQUEUED || tempQueue == null) {
			return false;
		}
		result = tempQueue.enqueue(this);
		if (result) {
			state = STATE_ENQUEUED;
			if (null != tempReferent) {
				reprocess();
			}
		}
		return result;
	}
}

private native void reprocess();

/** 
 * Constructs a new instance of this class.
 */
Reference() {
}

/**
 * Initialize a newly created reference object. Associate the
 * reference object with the referent.
 * 
 * @param r the referent
 */	
void initReference (T r) {
	state = STATE_INITIAL;
	referent = r;
}

/**
 * Initialize a newly created reference object.  Associate the
 * reference object with the referent, and the specified ReferenceQueue.
 * 
 * @param r the referent
 * @param q the ReferenceQueue
 */	
void initReference (T r, ReferenceQueue q) {
	/*[PR 101461] Reference should allow null queues */
	queue = q;
	state = STATE_INITIAL;
	referent = r;
}

/**
 * Called when a Reference has been removed from its ReferenceQueue.
 * Set the enqueued field to false.
 */
void dequeue() {
	/*[PR 112508] not synchronized, so isEnqueued() could return wrong result */
	synchronized(this) {
		state = STATE_CLEARED;
	}
}

/*[IF Sidecar19-SE]*/
/**
 * Used to keep the referenced object strongly reachable so that it is not reclaimable by garbage collection.
 * 
 * @param ref reference of the object.
 * @since		1.9
 */
public static void reachabilityFence(java.lang.Object ref) {
}
/*[ENDIF]*/

}
