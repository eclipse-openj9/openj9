/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.image.javacore.JCImageThread;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCJavaThread implements JavaThread {
	
	/*
	 * These must be immutable as they may be used
	 * repeatedly for conditional control, and
	 * need to retain the same state throughout the life
	 * of the DTFJ Image.
	 */
	private final ImagePointer fThreadID;
	private final CorruptData fCorruptData;
	private final JCJavaRuntime fRuntime;

	private Vector fStackFrames;
	private Vector fStackSections;
	
	private ImagePointer fJNIEnv;
	private JCImageThread fImageThread;
	private JavaObject fJavaObject;
	private JavaObject blockingJavaObject;
	private String fName;
	private int fPriority;
	private int fState;

	public JCJavaThread(JCJavaRuntime runtime, ImagePointer threadID) throws JCInvalidArgumentsException {
		if (runtime == null) {
			throw new JCInvalidArgumentsException("A Java thread must be associated with a valid Java runtime");
		}
		fRuntime = runtime;
		fCorruptData = new JCCorruptData(fThreadID = checkPointer(threadID));
		init(null, IBuilderData.NOT_AVAILABLE, IBuilderData.NOT_AVAILABLE, null);
		fRuntime.addJavaThread(this);
	}
	
	public JCJavaThread(JCJavaRuntime runtime, ImagePointer threadID, String name) throws JCInvalidArgumentsException {
		if (runtime == null) {
			throw new JCInvalidArgumentsException("A Java thread must be associated with a valid Java runtime");
		}
		fRuntime = runtime;
		fCorruptData = new JCCorruptData(fThreadID = checkPointer(threadID));
		init(name, IBuilderData.NOT_AVAILABLE, IBuilderData.NOT_AVAILABLE, null);
		fRuntime.addJavaThread(this);
	}

	/**
	 * 
	 * @param pointer
	 * 
	 */
	private ImagePointer checkPointer(ImagePointer pointer) throws JCInvalidArgumentsException {
		if (pointer == null) {
			throw new JCInvalidArgumentsException("A java thread must have a non null ID");
		}
		return pointer;
	}
	
	/**
	 * 
	 * @param name
	 * @param priority
	 * @param state
	 */
	private void init(String name, int priority, int state, JavaObject javaObject) {
		fStackFrames = new Vector();
		fStackSections = new Vector();
		fName = name;
		fPriority = priority;
		fState = state;
		fJavaObject = javaObject;
	}
	
	
	/**
	 * 
	 */
	public ImageThread getImageThread() throws CorruptDataException, DataUnavailable {
		if (fImageThread == null) {
			throw new DataUnavailable("No image thread found for this java thread");
		}
		return fImageThread;
	}
	
	
	/**
	 * 
	 */
	public ImagePointer getJNIEnv() throws CorruptDataException {
		if (fJNIEnv == null) {
			throw new CorruptDataException(fCorruptData);
		}
		return fJNIEnv;
	}

	
	/**
	 * 
	 * 
	 * 
	 */
	public String getName() throws CorruptDataException {
		if (fName == null) {
			fName = "vmthread @" + fThreadID.getAddress();
		}
		return fName;
	}

	/**
	 * 
	 */
	public JavaObject getObject() throws CorruptDataException {
		if (fJavaObject == null) {
			throw new CorruptDataException(fCorruptData);
		}
		return fJavaObject;
	}
	
	/**
	 * 
	 */
	public int getPriority() throws CorruptDataException {
		if (fPriority == IBuilderData.NOT_AVAILABLE) {
			throw new CorruptDataException(fCorruptData);
		}
		return fPriority;
	}

	/**
	 * 
	 */
	public Iterator getStackFrames() {
		return fStackFrames.iterator();
	}

	/**
	 * 
	 *
	 */
	public void addStackFrame(JavaStackFrame javaStackFrame) {
		if (javaStackFrame != null) {
			fStackFrames.add(javaStackFrame);
		}
	}
	
	/**
	 * 
	 */
	public Iterator getStackSections() {
		return fStackSections.iterator();
	}

	/**
	 * 
	 */
	public int getState() throws CorruptDataException {
		if (fState == IBuilderData.NOT_AVAILABLE) {
			throw new CorruptDataException(fCorruptData);
		}
		return fState;
	}
	
	/**
	 * NON-DTFJ
	 * @param imageThread
	 */
	public void setImageThread(JCImageThread imageThread) {
		fImageThread = imageThread;
	}
	
	/**
	 * NON-DTFJ. For internal building purposes. Do not call as part of DTFJ.
	 * @return image thread or null if non found.
	 */
	public JCImageThread internalGetImageThread() {
		return fImageThread;
	}
	
	/**
	 * NON-DTFJ
	 * @param name
	 */
	public void setName(String name) {
		fName = name;
	}
	
	/**
	 * NON-DTFJ
	 * @param priority
	 */
	public void setPriority(int priority) {
		fPriority = priority;
	}
	
	/**
	 * NON-DTFJ
	 * @param state
	 */
	public void setState(String state) {
		if (state == null) return;
		if (state.equals("R")) fState = JavaThread.STATE_ALIVE | JavaThread.STATE_RUNNABLE;
		else if (state.equals("CW")) fState = JavaThread.STATE_ALIVE | JavaThread.STATE_WAITING | JavaThread.STATE_IN_OBJECT_WAIT;
		else if (state.equals("P")) fState = JavaThread.STATE_ALIVE | JavaThread.STATE_WAITING | JavaThread.STATE_PARKED;
		else if (state.equals("B")) fState = JavaThread.STATE_ALIVE | JavaThread.STATE_WAITING | JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER;
		else if (state.equals("MW")) fState = JavaThread.STATE_ALIVE | JavaThread.STATE_WAITING | JavaThread.STATE_BLOCKED_ON_MONITOR_ENTER;
	}
	
	/**
	 * Not in DTFJ. Used only for building purposes.
	 * 
	 */
	public ImagePointer getThreadID() {
		return fThreadID;
	}
	
	/**
	 * Not in DTFJ. Used only for building purposes.
	 * 
	 */
	public ImagePointer setJNIEnv(ImagePointer env) {
		return fJNIEnv = env;
	}
		
	/**
	 * NOT in DTFJ
	 * @param threadObject
	 */
	public void setObject(JavaObject threadObject) {
		fJavaObject = threadObject;
	}

	/**
	 * NOT in DTFJ
	 * @param blockingObject
	 */
	public void setBlockingObject(JavaObject blockingObject) {
		this.blockingJavaObject = blockingObject;
	}
	
	public JavaObject getBlockingObject() throws CorruptDataException,
			DataUnavailable {
		return blockingJavaObject;
	}
}
