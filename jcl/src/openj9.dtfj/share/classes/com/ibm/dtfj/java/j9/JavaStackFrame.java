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
import com.ibm.dtfj.java.JavaLocation;

/**
 * @author jmdisher
 *
 */
public class JavaStackFrame implements com.ibm.dtfj.java.JavaStackFrame
{
	private JavaRuntime _javaVM;
	private ImagePointer _basePointer;
	private JavaMethod _method;
	private ImagePointer _methodID;
	private ImagePointer _pc;
	private int _lineNumber;
	private Vector _stackRoots = new Vector();
	
	private JavaStackFrame(JavaRuntime javaVM, ImagePointer basePointer, ImagePointer pc, int lineNumber)
	{
		if (null == basePointer) {
			throw new IllegalArgumentException("A Java stack frame must have a non-null base pointer");
		}
		if (null == pc) {
			throw new IllegalArgumentException("A Java stack frame must have a non-null program counter");
		}
		_javaVM = javaVM;
		_basePointer = basePointer;
		_pc = pc;
		_lineNumber = lineNumber;
	}
	
	public JavaStackFrame(JavaRuntime javaVM, ImagePointer basePointer, JavaMethod method, ImagePointer pc, int lineNumber)
	{
		this(javaVM, basePointer, pc, lineNumber);
		if (null == method) {
			throw new IllegalArgumentException("A Java stack frame must refer to a non-null method");
		}
		_method = method;
	}
	
	public JavaStackFrame(JavaRuntime javaVM, ImagePointer basePointer, ImagePointer methodID, ImagePointer pc, int lineNumber)
	{
		this(javaVM, basePointer, pc, lineNumber);
		_methodID = methodID;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getBasePointer()
	 */
	public ImagePointer getBasePointer() throws CorruptDataException
	{
		// Must never be null
		return _basePointer;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getLocation()
	 */
	public JavaLocation getLocation() throws CorruptDataException
	{
		//create the location with the method id and pc
		if (_method != null) {
			return new com.ibm.dtfj.java.j9.JavaLocation(_method, _pc, _lineNumber);
		} else {
			return new com.ibm.dtfj.java.j9.JavaLocation(_methodID, _pc, _lineNumber);
		}
	}

	public void createObjectRef(long id)
	{
		// Create the object reference for this id. Stack frame objects all have STRONG reachability.
		JavaReference jRef;
		jRef = new JavaReference(_javaVM, this, id, "StackFrame Root", JavaReference.REFERENCE_UNKNOWN, JavaReference.HEAP_ROOT_STACK_LOCAL, JavaReference.REACHABILITY_STRONG);
		_stackRoots.add(jRef);
	}
	
	private static boolean equals(Object o1, Object o2)
	{
		return o1 == o2 || o1 != null && o1.equals(o2);
	}
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaStackFrame) {
			JavaStackFrame local = (JavaStackFrame) obj;
			isEqual = equals(_method, local._method) && equals(_pc, local._pc);
		}
		return isEqual;
	}

	private static int hashCode(Object o1)
	{
		return o1 == null ? 0 : o1.hashCode();
	}
	
	public int hashCode()
	{
		return hashCode(_method) ^ _pc.hashCode() ^ hashCode(_methodID);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getHeapRoots()
	 */
	public Iterator getHeapRoots()
	{
		return _stackRoots.iterator();
	}
}
