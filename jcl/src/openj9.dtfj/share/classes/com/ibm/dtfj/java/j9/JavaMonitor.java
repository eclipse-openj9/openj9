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
package com.ibm.dtfj.java.j9;

import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;

/**
 * @author jmdisher
 *
 */
public class JavaMonitor implements com.ibm.dtfj.java.JavaMonitor
{
	private JavaRuntime _javaVM;
	private ImagePointer _monitorID;
	private String _monitorName;
	private ImagePointer _encompassingObjectAddress = null;
	private JavaObject _encompassingObject = null;
	private long _owningThreadID;
	private Vector _blockList = new Vector();
	private Vector _waitList = new Vector();

	
	public JavaMonitor(JavaRuntime runtime, ImagePointer pointer, String name, ImagePointer encompassingObjectAddress, long owningThread)
	{
		if (null == runtime) {
			throw new IllegalArgumentException("A Java Monitor cannot exist in a null Java VM");
		}
		if (null == pointer) {
			throw new IllegalArgumentException("A Java Monitor requires a non-null ID");
		}
		_javaVM = runtime;
		_monitorID = pointer;
		_encompassingObjectAddress = encompassingObjectAddress;
		_owningThreadID = owningThread;
		if (null == name) {
			if (null != _encompassingObjectAddress) {
				_monitorName = "(un-named monitor @0x" + Long.toHexString(pointer.getAddress()) + " for object @0x" + Long.toHexString(_encompassingObjectAddress.getAddress()) + ")";
			} else {
				_monitorName = "(un-named monitor @0x" + Long.toHexString(pointer.getAddress()) + ")";
			}
		} else {
			_monitorName = name;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getObject()
	 */
	public JavaObject getObject() 
	{
		if (null == _encompassingObjectAddress) {
			return null;
		}
		
		// lazy loading of the encompassing object
		if (null == _encompassingObject) {
			try {
				_encompassingObject = _javaVM.getObjectAtAddress(_encompassingObjectAddress);
			} catch (CorruptDataException e) {
				// since we cannot rethrow anything, we just return null 
			} catch (IllegalArgumentException e) {
				// since we cannot rethrow anything, we just return null 
			}
		}
		return _encompassingObject;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getName()
	 */
	public String getName() throws CorruptDataException
	{
		return _monitorName;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getOwner()
	 */
	public JavaThread getOwner() throws CorruptDataException
	{
		JavaThread owningThread = null;
		Iterator allThreads = _javaVM.getThreads();
		
		while (allThreads.hasNext()) {
			JavaThread oneThread = (JavaThread) allThreads.next();
			
			if (oneThread.getJNIEnv().getAddress() == _owningThreadID) {
				owningThread = oneThread;
				break;
			}
		}
		if ((null == owningThread) && (0 != _owningThreadID)) {
			// we have an owner but we couldn't find it.  That implies that the XML is corrupt
			throw new CorruptDataException(new CorruptData("Monitor owner not found", _javaVM.pointerInAddressSpace(_owningThreadID)));
		}
		return owningThread;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getEnterWaiters()
	 */
	public Iterator getEnterWaiters()
	{
		if (null == _blockList) {
			_blockList = new Vector();
		}
		return _blockList.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getNotifyWaiters()
	 */
	public Iterator getNotifyWaiters()
	{
		if (null == _waitList) {
			_waitList = new Vector();
		}
		return _waitList.iterator();
	}
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaMonitor) {
			JavaMonitor local = (JavaMonitor) obj;
			isEqual = (_javaVM.equals(local._javaVM) && _monitorID.equals(local._monitorID));
		}
		return isEqual;
	}

	public int hashCode()
	{
		return _javaVM.hashCode() ^ _monitorID.hashCode();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMonitor#getID()
	 */
	public ImagePointer getID()
	{
		return _monitorID;
	}

	public void addBlockedThread(JavaThread thread)
	{
		_blockList.add(thread);
	}

	public void addWaitingThread(JavaThread thread)
	{
		_waitList.add(thread);
	}
}
