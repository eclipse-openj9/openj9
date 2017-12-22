/*[INCLUDE-IF Sidecar16]*/
package java.lang;

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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
 
/**
 * Object is the root of the java class hierarchy. All non-base types
 * respond to the messages defined in this class.
 *
 * @author		OTI
 * @version		initial
 */
 
public class Object {

/**
 * Constructs a new instance of this class.
 */
public Object() {
}

/**
 * Answers a new instance of the same class as the receiver,
 * whose slots have been filled in with the values in the
 * slots of the receiver.
 * <p>
 * Classes which wish to support cloning must specify that
 * they implement the Cloneable interface, since the native
 * implementation checks for this.
 *
 * @return		Object
 *					a shallow copy of this object.
 * @exception	CloneNotSupportedException
 *					if the receiver's class does not implement
 *					the interface Cloneable.
 */
protected Object clone() throws CloneNotSupportedException
{
	return J9VMInternals.primitiveClone(this);
}

/**
 * Compares the argument to the receiver, and answers true
 * if they represent the <em>same</em> object using a class
 * specific comparison. The implementation in Object answers
 * true only if the argument is the exact same object as the
 * receiver (==).
 *
 * @param		o Object
 *					the object to compare with this object.
 * @return		boolean
 *					<code>true</code>
 *						if the object is the same as this object
 *					<code>false</code>
 *						if it is different from this object.
 * @see			#hashCode
 */
public boolean equals (Object o) {
	return this == o;
}

/**
 * Called by the virtual machine when there are no longer any (non-weak)
 * references to the receiver. Subclasses can use this facility to
 * guarantee that any associated resources are cleaned up before
 * the receiver is garbage collected. Uncaught exceptions which are
 * thrown during the running of the method cause it to terminate
 * immediately, but are otherwise ignored.
 * <p>
 * Note: The virtual machine assumes that the implementation
 * in class Object is empty.
 *
 * @exception	Throwable
 *					The virtual machine ignores any exceptions
 *					which are thrown during finalization.
 *
/*[IF Sidecar19-SE]
 * @deprecated  May cause performance issues, deadlocks and hangs. 
 *              Errors in  finalizers can lead to resource leaks.
/*[ENDIF]
 */
/*[IF]
	We leave this one in even if no Thread flag because the user can still
	explicitly call runAllFinalizers(). So, he won't get finalization for free,
	but he can still invoke it. Since this method is empty, the space cost for it
	is negligible, so we leave it in.
/*[ENDIF]*/
/*[IF Sidecar19-SE] */
@Deprecated(forRemoval=false, since="9")
/*[ENDIF]*/
protected void finalize () throws Throwable {
}

/**
 * Answers the unique instance of java.lang.Class which 
 * represents the class of the receiver.
 *
 * @return	the receiver's Class
 */
public final native Class<? extends Object> getClass();

/**
 * Answers an integer hash code for the receiver. Any two 
 * objects which answer <code>true</code> when passed to 
 * <code>.equals</code> must answer the same value for this
 * method.
 *
 * @return	the receiver's hash.
 *
 * @see	#equals
 */
public int hashCode() {
	return J9VMInternals.fastIdentityHashCode(this);
}

/**
 * Causes one thread which is <code>wait</code>ing on the
 * receiver to be made ready to run. This does not guarantee
 * that the thread will immediately run. The method can only
 * be invoked by a thread which owns the receiver's monitor.
 *
 * @see			#notifyAll
 * @see			#wait()
 * @see			#wait(long)
 * @see			#wait(long,int)
 * @see			java.lang.Thread
 */
public final native void notify();

/**
 * Causes all threads which are <code>wait</code>ing on the
 * receiver to be made ready to run. The threads are scheduled
 * according to their priorities as specified in class Thread.
 * Between any two threads of the same priority the one which
 * waited first will be the first thread that runs after
 * being notified. The method can only be invoked by a thread 
 * which owns the receiver's monitor.
 *
 * @see			#notify
 * @see			#wait()
 * @see			#wait(long)
 * @see			#wait(long,int)
 * @see			java.lang.Thread
 */
public final native void notifyAll();

/**
 * Answers a string containing a concise, human-readable
 * description of the receiver.
 *
 * @return		String
 *					a printable representation for the receiver.
 */
public String toString () {
	/*[IF] 
	The explicit use of StringBuffer is not necessary as the hex string will not exceed 8 chars
	and thus the StringBuffer (allocated with classname.length()+16) in the  will not grow.
	/*[ENDIF]*/
	return this.getClass().getName() + '@' + Integer.toHexString(this.hashCode());
}

/**
 * Causes the thread which sent this message to be made not
 * ready to run pending some change in the receiver (as
 * indicated by <code>notify</code> or <code>notifyAll</code>).
 * The method can only be invoked by a thread which owns the 
 * receiver's monitor. A waiting thread can be sent
 * <code>interrupt()</code> to cause it to prematurely stop
 * waiting, so senders of wait should check that the condition
 * they were waiting for has been met.
 * <p>
 * When the thread waits, it gives up ownership of the 
 * receiver's monitor. When it is notified (or interrupted) it
 * re-acquires the monitor before it starts running.
 *
 * @exception	InterruptedException
 *					to interrupt the wait.
 *
 * @see			Thread#interrupt
 * @see			#notify
 * @see			#notifyAll
 * @see			#wait(long)
 * @see			#wait(long,int)
 * @see			java.lang.Thread
 */
public final void wait () throws InterruptedException {
	wait(0, 0);
}

/**
 * Causes the thread which sent this message to be made not
 * ready to run either pending some change in the receiver 
 * (as indicated by <code>notify</code> or <code>notifyAll</code>) 
 * or the expiration of the timeout. The method can only be invoked 
 * by a thread which owns the receiver's monitor. A waiting thread 
 * can be sent <code>interrupt()</code> to cause it to prematurely 
 * stop waiting, so senders of wait should check that the condition
 * they were waiting for has been met.
 * <p>
 * When the thread waits, it gives up ownership of the 
 * receiver's monitor. When it is notified (or interrupted) it
 * re-acquires the monitor before it starts running.
 *
 * @param		time long
 *					The maximum time to wait in milliseconds.
 * @exception	InterruptedException
 *					to interrupt the wait.
 *
 * @see			#notify
 * @see			#notifyAll
 * @see			#wait()
 * @see			#wait(long,int)
 * @see			java.lang.Thread
 */
public final void wait (long time) throws InterruptedException {
	wait(time, 0);
}

/**
 * Causes the thread which sent this message to be made not
 * ready to run either pending some change in the receiver 
 * (as indicated by <code>notify</code> or <code>notifyAll</code>) 
 * or the expiration of the timeout. The method can only be invoked 
 * by a thread which owns the receiver's monitor. A waiting thread 
 * can be sent <code>interrupt()</code> to cause it to prematurely 
 * stop waiting, so senders of wait should check that the condition
 * they were waiting for has been met.
 * <p>
 * When the thread waits, it gives up ownership of the 
 * receiver's monitor. When it is notified (or interrupted) it
 * re-acquires the monitor before it starts running.
 *
 * @param		time long
 *					The maximum time to wait in milliseconds.
 * @param		frac int
 *					The fraction of a mSec to wait, specified
 *					in nano seconds.
 * @exception	InterruptedException
 *					to interrupt the wait.
 *
 * @see			#notify
 * @see			#notifyAll
 * @see			#wait()
 * @see			#wait(long)
 * @see			java.lang.Thread
 */
public final native void wait (long time, int frac) throws InterruptedException;

/*
 * Used as a prototype for the jit.
 */
/*[PR CMVC 114139]InstantiationException has wrong detail message */
private Object newInstancePrototype(Class callerClass) {
	return null;
}
}
