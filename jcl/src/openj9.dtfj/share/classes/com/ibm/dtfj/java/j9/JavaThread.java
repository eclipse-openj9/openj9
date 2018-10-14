/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.dtfj.java.j9;

import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public class JavaThread implements com.ibm.dtfj.java.JavaThread
{
	private JavaRuntime _javaVM;
	private ImagePointer _objectID;
	private String _threadState;
	private ImagePointer _jniEnv;
	private Vector _stackFrames = new Vector();
	private ImageThread _imageThread;
	private JavaStackSection _stackSection;
	
	private JavaObject _javaObject = null;

	private static final HashMap _threadStateMap = new HashMap();
	static {
		_threadStateMap.put("Dead", 	Integer.valueOf(STATE_TERMINATED));
		_threadStateMap.put("Suspended",Integer.valueOf(STATE_ALIVE | STATE_SUSPENDED));
		_threadStateMap.put("Running", 	Integer.valueOf(STATE_ALIVE | STATE_RUNNABLE));
		_threadStateMap.put("Blocked", 	Integer.valueOf(STATE_ALIVE | STATE_BLOCKED_ON_MONITOR_ENTER));
		_threadStateMap.put("Waiting", 			Integer.valueOf(STATE_ALIVE | STATE_WAITING | STATE_WAITING_INDEFINITELY | STATE_IN_OBJECT_WAIT));
		_threadStateMap.put("Waiting timed", 	Integer.valueOf(STATE_ALIVE | STATE_WAITING | STATE_WAITING_WITH_TIMEOUT | STATE_IN_OBJECT_WAIT));
		_threadStateMap.put("Sleeping", 		Integer.valueOf(STATE_ALIVE | STATE_WAITING | STATE_SLEEPING));
		_threadStateMap.put("Parked",   		Integer.valueOf(STATE_ALIVE | STATE_WAITING | STATE_WAITING_INDEFINITELY | STATE_PARKED));
		_threadStateMap.put("Parked timed",   	Integer.valueOf(STATE_ALIVE | STATE_WAITING | STATE_WAITING_WITH_TIMEOUT | STATE_PARKED));
	};
	
	public JavaThread(JavaRuntime vm, ImagePointer nativeID, ImagePointer objectID, String state, ImageThread imageThread)
	{
		if (null == vm) {
			throw new IllegalArgumentException("A Java thread cannot exist in a null Java VM");
		}
		_javaVM = vm;
		_imageThread = imageThread;
		_objectID = objectID;
		_threadState = state;
		_jniEnv = nativeID;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getJNIEnv()
	 */
	public ImagePointer getJNIEnv() throws CorruptDataException
	{
		return _jniEnv;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getPriority()
	 */
	public int getPriority() throws CorruptDataException
	{
		JavaObject theObject = getObject();
		if (null != theObject) {
			JavaClass threadClass = _javaLangThreadSuperclass();
			Iterator fields = threadClass.getDeclaredFields();
			
			while (fields.hasNext()) {
				JavaField oneField = (JavaField) fields.next();
				if (oneField.getName().equals("priority")) {
					try {
						return oneField.getInt(theObject);
					} catch (MemoryAccessException e) {
						throw new CorruptDataException(new CorruptData("unable to read memory for 'priority' field", null));
					}
				}
			}
			throw new CorruptDataException(new CorruptData("unable to find 'priority' field", null));
		} else {
			//this happens if the thread is not attached so it does not have any java priority
			//TODO:  have a good exception for this sort of thing
			return -1;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getObject()
	 */
	public JavaObject getObject() throws CorruptDataException
	{
		if (null == _javaObject && 0 != _objectID.getAddress()) {
			try {
				_javaObject = _javaVM.getObjectAtAddress(_objectID);
			} catch (IllegalArgumentException e) {
				// getObjectAtAddress can throw an IllegalArgumentException if the address is not aligned
				throw new CorruptDataException(new com.ibm.dtfj.image.j9.CorruptData(e.getMessage(),_objectID));
			}
		}
		return _javaObject;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getState()
	 */
	public int getState() throws CorruptDataException
	{
		if (_threadStateMap.containsKey(_threadState)) {
			return 	((Integer)_threadStateMap.get(_threadState)).intValue();
		} else {
			throw new CorruptDataException(new CorruptData("Invalid thread state: "+_threadState, _jniEnv));
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getImageThread()
	 */
	public ImageThread getImageThread() throws CorruptDataException, DataUnavailable
	{
		if (null == _imageThread) {
			throw new DataUnavailable("Native thread not found");
		}
		return _imageThread;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getStackSections()
	 */
	public Iterator getStackSections()
	{
		List toReturn = null;
		
		if (null == _stackSection) {
			//we didn't set the stack section so this thread must be corrupt
			toReturn = Collections.singletonList(new CorruptData("stack section missing", _jniEnv));
		} else {
			toReturn = Collections.singletonList(_stackSection);
		}
		return toReturn.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getStackFrames()
	 */
	public Iterator getStackFrames()
	{
		return _stackFrames.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getName()
	 */
	public String getName() throws CorruptDataException
	{
		JavaObject theObject = getObject();
		if (null != theObject) {
			JavaClass threadClass = _javaLangThreadSuperclass();
			Iterator fields = threadClass.getDeclaredFields();
			
			while (fields.hasNext()) {
				JavaField oneField = (JavaField) fields.next();
				if (oneField.getName().equals("name")) {
					try {
						return oneField.getString(theObject);
					} catch (MemoryAccessException e) {
						throw new CorruptDataException(new CorruptData("unable to read memory for 'name' field", null));
					}
				}
			}
			throw new CorruptDataException(new CorruptData("unable to find 'name' field", null));
		} else {
			return "vmthread @" + _jniEnv.getAddress();
		}
	}

	private com.ibm.dtfj.java.JavaClass _javaLangThreadSuperclass() throws CorruptDataException
	{
		com.ibm.dtfj.java.JavaClass clazz = getObject().getJavaClass();
		while ((null != clazz) && (!clazz.getName().equals("java/lang/Thread"))) {
			clazz = clazz.getSuperclass();
		}
		if (null == clazz) {
			throw new CorruptDataException(new CorruptData("JavaThread is not a subclass of java/lang/Thread", getObject().getID()));
		}
		return clazz;
	}

	public void addFrame(Object frame)
	{
		_stackFrames.add(frame);
	}

	public void setStackSection(long base, long size)
	{
		_stackSection = new JavaStackSection(_jniEnv.getAddressSpace().getPointer(base), size, this);
	}
	
	/**
	 * @author jmdisher
	 * It might make sense to put this in its own class, at some point, but it is only used here
	 */
	class JavaStackSection implements ImageSection
	{
		private ImagePointer _start;
		private long _size;
		private JavaThread _parent;
		
		
		public JavaStackSection(ImagePointer start, long size, JavaThread parent)
		{
			_start = start;
			_size = size;
			_parent = parent;
		}
		
		public ImagePointer getBaseAddress()
		{
			return _start;
		}

		public long getSize()
		{
			return _size;
		}

		public String getName()
		{
			String parentName = null;
			try {
				parentName = _parent.getName();
			} catch (CorruptDataException e) {
				parentName = "(corrupt data)";
			}
			return "JavaStackSection for JavaThread: " + parentName;
		}

		public boolean isExecutable() throws DataUnavailable
		{
			return _start.isExecutable();
		}

		public boolean isReadOnly() throws DataUnavailable
		{
			return _start.isReadOnly();
		}

		public boolean isShared() throws DataUnavailable
		{
			return _start.isShared();
		}
		
		public Properties getProperties() {
			return _start.getProperties();
		}
	}

	public boolean equals(Object obj)
	{
		//note that we can't get image threads on all platforms and in all situations but we still need to return equals correctly
		boolean isEqual = (null == _imageThread) ? (this == obj) : false;
		
		if ((null != _imageThread) && (obj instanceof JavaThread)) {
			JavaThread local = (JavaThread) obj;
			isEqual = (_javaVM.equals(local._javaVM) && (_imageThread.equals(local._imageThread)));
		}
		return isEqual;
	}

	public int hashCode()
	{
		int hash = (null == _imageThread) ? _jniEnv.hashCode() : _imageThread.hashCode();
		return (_javaVM.hashCode() ^ hash);
	}

	public JavaStackFrame addNewStackFrame(long arguments, long method, long pc, int lineNumber)
	{
		JavaStackFrame frame = null;
			JavaMethod javaMethod = _javaVM.methodForID(method);
			ImagePointer frameAddr = _javaVM.pointerInAddressSpace(arguments);
			ImagePointer programCounter = _javaVM.pointerInAddressSpace(pc);
			if (javaMethod == null) {
				ImagePointer meth = _javaVM.pointerInAddressSpace(method);
				frame = new JavaStackFrame(_javaVM, frameAddr, meth, programCounter, lineNumber);
			} else {
				frame = new JavaStackFrame(_javaVM, frameAddr, javaMethod, programCounter, lineNumber);
			}
		addFrame(frame);
		return frame;
	}
	
	/**
	 * Called by the parser if it finds an error tag where the stack was expected
	 */
	public void setStackCorrupt()
	{
		addFrame(new CorruptData("vmthread encountered error tag where stack expected", _jniEnv));
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaThread#getBlockingObject()
	 */
	public JavaObject getBlockingObject() throws CorruptDataException, DataUnavailable {
		throw new DataUnavailable("Not available.");
	}
}
