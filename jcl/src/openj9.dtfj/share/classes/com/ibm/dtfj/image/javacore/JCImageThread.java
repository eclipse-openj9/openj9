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
package com.ibm.dtfj.image.javacore;

import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageRegister;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;

public class JCImageThread implements ImageThread {
	
	/*
	 * This should be in the form 0x...
	 */
	private final String fImageThreadID;
	
	
	private final ImagePointer fNativeThreadID;
	
	private Properties fProperties;
	private Vector fRegisters;
	private Vector fStackSections;
	private Vector fStackFrames;
	private ImagePointer fSystemThreadID;
	/**
	 * 
	 * @param nativeThreadID
	 */
	public JCImageThread(ImagePointer nativeThreadID) throws JCInvalidArgumentsException {
		if (nativeThreadID == null) {
			throw new JCInvalidArgumentsException("Must pass a valid threadID pointer");
		}
		fImageThreadID = "0x" + Long.toHexString(nativeThreadID.getAddress());
		fNativeThreadID = nativeThreadID;
		fRegisters = new Vector();
		fStackSections = new Vector();
		fStackFrames = new Vector();
		fProperties = new Properties();
	}
	
	/**
	 * 
	 */
	public String getID() throws CorruptDataException {
		if (fImageThreadID == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fImageThreadID;
	}
	
	/**
	 * 
	 */
	public Properties getProperties() {
		return fProperties;
	}
	
	/**
	 * 
	 * @param key
	 * @param value
	 */
	public void addProperty(Object key, Object value) {
		fProperties.put(key, value);
	}

	/**
	 * 
	 */
	public Iterator getRegisters() {
		return fRegisters.iterator();
	}

	/**
	 * 
	 */
	public Iterator getStackSections() {
		return fStackSections.iterator();
	}
	
	/**
	 * Not available in javacore
	 */
	public Iterator getStackFrames() throws DataUnavailable {
		if (fStackFrames.isEmpty()) {
			throw new DataUnavailable("Native stack frame data not available");
		}
		return fStackFrames.iterator();
		
	}
	
	/**
	 * NOT in DTFJ
	 * @param stackFrame
	 */
	public void addImageStackFrame(ImageStackFrame stackFrame) {
		if (stackFrame != null) {
			fStackFrames.add(stackFrame);
		}
	}

	
	/**
	 * NOT in DTFJ
	 * Add a stack section for this thread.
	 * Duplicates are not removed.
	 * Do NOT use this outside the building process.
	 * @param stackSection
	 */
	public void addImageStackSection(ImageSection stackSection) {
		if (stackSection != null) {
			fStackSections.add(stackSection);
		}
	}
	
	/**
	 * NON-DTFJ. Used internally as it bypasses the exception, since due to internal implementation,
	 * an exception thrown when the id is not set does not necessarily mean an internal building error.
	 * Do NOT use this outside the building process.
	 * 
	 * 
	 */
	public ImagePointer getInternalID() {
		return fNativeThreadID;
	}
	
	/**
	 * NON-DTFJ. For building purposes only. Don't use as part of DTFJ.
	 * @param systemThreadID
	 */
	public void setSystemThreadID(ImagePointer systemThreadID) {
		fSystemThreadID = systemThreadID;
	}
	
	/**
	 * NON-DTFJ. For building purposes only. Don't use as part of DTFJ.
	 * @return system thread id or null if not set.
	 */
	public ImagePointer getSystemThreadID() {
		return fSystemThreadID;
	}
	
	/**
	 * NOT in DTFJ
	 * @param reg register
	 */
	public void addRegister(ImageRegister reg) {
		fRegisters.add(reg);
	}
}
