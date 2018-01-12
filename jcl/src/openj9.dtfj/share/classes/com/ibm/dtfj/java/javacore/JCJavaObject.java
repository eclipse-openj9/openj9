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

import java.util.Collections;
import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;

public class JCJavaObject implements JavaObject {
	
	private final ImagePointer fAddress;
	private final JavaClass fJavaClass;
	
	private Vector fSections;
	
	public JCJavaObject(ImagePointer address, JavaClass javaClass) throws JCInvalidArgumentsException {
		if (address == null) {
			throw new JCInvalidArgumentsException("Must have a valid object ID");
		}
		fAddress = address;
		fJavaClass = javaClass;
		fSections = new Vector();
	}

	/**
	 * 
	 */
	public void arraycopy(int arg0, Object arg1, int arg2, int arg3) throws CorruptDataException, MemoryAccessException {
		throw new CorruptDataException(new JCCorruptData("No array support implemented", null));
	}

	
	/**
	 * 
	 */
	public int getArraySize() throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData("No array support implemented", null));
	}

	
	/**
	 * 
	 */
	public long getHashcode() throws DataUnavailable, CorruptDataException {
		throw new DataUnavailable("No hashCode for object");
	}

	
	/**
	 * 
	 */
	public ImagePointer getID() {
		return fAddress;
	}

	
	/**
	 * 
	 */
	public JavaClass getJavaClass() throws CorruptDataException {
		if (fJavaClass == null) {
			throw new CorruptDataException(new JCCorruptData("Java Class information not available", null));
		}
		return fJavaClass;
	}

	
	/**
	 * 
	 */
	public long getPersistentHashcode() throws DataUnavailable, CorruptDataException {
		throw new DataUnavailable("No hashCode for object");
	}

	
	/**
	 * 
	 */
	public Iterator getSections() {
		return fSections.iterator();
	}

	/**
	 * 
	 */
	public long getSize() throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData("Size computation not supported", null));
	}

	
	/**
	 * 
	 */
	public boolean isArray() throws CorruptDataException {
		JavaClass type = getJavaClass();
		return type.isArray();
	}

	public Iterator getReferences() {
		return Collections.EMPTY_LIST.iterator();
	}

	public JavaHeap getHeap() throws DataUnavailable {
		throw new DataUnavailable("Heap support not implemented.");
	}
}
