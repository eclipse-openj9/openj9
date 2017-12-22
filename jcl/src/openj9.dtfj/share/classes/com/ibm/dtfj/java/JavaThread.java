/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageThread;

/**
 * Represents a Java thread.
 * 
 * @see com.ibm.dtfj.java.JavaRuntime#getThreads()
 */
public interface JavaThread  {

	/** The thread is alive	 */
	int STATE_ALIVE						= 0x00000001;
	/** The thread has terminated */
	int STATE_TERMINATED				= 0x00000002;
	/** The thread can be run although may not be actually running */
	int STATE_RUNNABLE					= 0x00000004;
	/** The thread is waiting on a monitor with no timeout value set */
	int STATE_WAITING_INDEFINITELY		= 0x00000010;
	/** The thread is waiting on a monitor but will timeout at some point */
	int STATE_WAITING_WITH_TIMEOUT		= 0x00000020;
	/** The thread is in the Thread.sleep method	 */
	int STATE_SLEEPING					= 0x00000040;
	/** The thread is in a waiting state in native code	 */
	int STATE_WAITING					= 0x00000080;
	/** The thread is in Object.wait */
	int STATE_IN_OBJECT_WAIT			= 0x00000100;
	/** The thread has been deliberately removed from scheduling */
	int STATE_PARKED					= 0x00000200;
	/** The thread is waiting to enter an object monitor	 */
	int STATE_BLOCKED_ON_MONITOR_ENTER	= 0x00000400;
	/** The thread has been suspended by Thread.suspend */
	int STATE_SUSPENDED					= 0x00100000;
	/** The thread has a pending interrupt */
	int STATE_INTERRUPTED				= 0x00200000;
	/** The thread is in native code */
	int STATE_IN_NATIVE					= 0x00400000;
	/** The thread is in a vendor specific state */
	int STATE_VENDOR_1					= 0x10000000;
	/** The thread is in a vendor specific state */
	int STATE_VENDOR_2					= 0x20000000;
	/** The thread is in a vendor specific state */
	int STATE_VENDOR_3					= 0x40000000;
	
    /**
     * Get the address of the JNIEnv structure which represents this thread instance in JNI.
     * @return the address of the JNIEnv structure which represents this thread instance in JNI
     * @throws CorruptDataException 
     */
    public ImagePointer getJNIEnv() throws CorruptDataException;
    
    /**
     * Get the Java priority of the thread.
     * @return the Java priority of the thread (a number between 1 and 10 inclusive)
     * @throws CorruptDataException 
     * 
     * @see Thread#getPriority()
     */
    public int getPriority() throws CorruptDataException;
    

    /**
     * Fetch the java.lang.Thread associated with this thread. If the thread is in the process
     * of being attached, this may return null.
     * 
     * @return the a JavaObject representing the java.lang.Thread associated with this thread
     * @throws CorruptDataException 
     * 
     * @see JavaObject
     * @see Thread
     */
    public JavaObject getObject() throws CorruptDataException;
    
    /**
     * Get the state of the thread when the image was created.
     * @return the state of the thread when the image was created.
     * The result is a bit vector, and uses the states defined by
     * the JVMTI specification.
     * @throws CorruptDataException 
     */
    public int getState() throws CorruptDataException;
    
    /**
     * Represents the joining point between the Java view of execution and the corresponding native view.
     * This method is where the mapping from Java into native threading resources is provided.
     * 
     * @return the ImageThread which this ManagedThread is currently bound to.
     * @throws CorruptDataException If the underlying resource describing the native representation of the thread
     * is damaged
     * @throws DataUnavailable If no mapping is provided due to missing or limited underlying resources (this 
     * exception added in DTFJ 1.1)
     * 
     * @see ImageThread
     */
    public ImageThread getImageThread() throws CorruptDataException, DataUnavailable;

    /**
     * Get the set of ImageSections which make up the managed stack.
     * @return a collection of ImageSections which make up the managed stack.
     * <p>
     * Some Runtime implementations may also use parts of the ImageThread's stack
     * for ManagesStackFrames
     * 
     * @see ImageSection
     * @see ImageThread#getStackSections()
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getStackSections();
    
    /**
     * Get the set of stack frames.
     * @return an iterator to walk the managed stack frames in order from 
     * top-of-stack (that is, the most recent frame) to bottom-of-stack
     * 
     * @see JavaStackFrame
     * @see com.ibm.dtfj.image.CorruptData
     * 
     */
    public Iterator getStackFrames();
    
    /**
     * Return the name of the thread.
     * 
     * Usually this is derived from the object associated with the thread, but if the
     * name cannot be derived this way (e.g. there is no object associated with the thread)
     * DTFJ will synthesize a name for the thread.
     * 
     * @return the name of the thread
     * @throws CorruptDataException 
     */
    public String getName() throws CorruptDataException;
    
    /**
     * For threads that are in STATE_BLOCKED_ON_MONITOR_ENTER this method returns the JavaObject who's monitor they are blocked on.
     * For threads that are in STATE_IN_OBJECT_WAIT this method returns the JavaObject that Object.wait() was called on.
     * For threads that are in STATE_PARKED this method returns the JavaObject that was passed as the "blocker" object to the java.util.concurrent.LockSupport.park() call. It may return null if no blocker object was passed.
     * For threads in any other state this call will return null.
     * The state of the thread can be determined by calling JavaThread.getState()
     * 
     * @return the object this thread is waiting on or null.
     * @throws CorruptDataException
     * @throws DataUnavailable
     * @since 1.6
     */
    public JavaObject getBlockingObject() throws CorruptDataException, DataUnavailable;
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Thread in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
