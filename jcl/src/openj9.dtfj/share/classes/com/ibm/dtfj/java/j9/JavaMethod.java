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
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.j9.BytecodeImageSection;
import com.ibm.dtfj.image.j9.JitImageSection;
import com.ibm.dtfj.java.JavaClass;

/**
 * @author jmdisher
 *
 */
public class JavaMethod implements com.ibm.dtfj.java.JavaMethod
{
	private ImagePointer _methodID;
	private String _methodName;
	private String _signature;
	private int _modifiers;
	private JavaClass _declaringClass;
	private Vector _compiledSections = new Vector();
	private Vector _bytecodeSections = new Vector();
	
	public JavaMethod(ImagePointer methodID, String name, String signature, int modifiers, JavaClass declaringClass)
	{
		if (null == methodID) {
			throw new IllegalArgumentException("A Java Method cannot have a null method ID");
		}
		if (null == declaringClass) {
			throw new IllegalArgumentException("A Java Method cannot have a null declaring class");
		}
		_methodID = methodID;
		_methodName = name;
		_signature = signature;
		_modifiers = modifiers;
		_declaringClass = declaringClass;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMethod#getBytecodeSections()
	 */
	public Iterator getBytecodeSections()
	{
		return _bytecodeSections.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMethod#getCompiledSections()
	 */
	public Iterator getCompiledSections()
	{
		return _compiledSections.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getModifiers()
	 */
	public int getModifiers() throws CorruptDataException
	{
		return _modifiers;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getDeclaringClass()
	 */
	public JavaClass getDeclaringClass() throws CorruptDataException,
			DataUnavailable
	{
		return _declaringClass;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getName()
	 */
	public String getName() throws CorruptDataException
	{
		return _methodName;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getSignature()
	 */
	public String getSignature() throws CorruptDataException
	{
		return _signature;
	}

	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaMethod) {
			JavaMethod local = (JavaMethod) obj;
			isEqual = (_declaringClass.equals(local._declaringClass) && _methodID.equals(local._methodID));
		}
		return isEqual;
	}

	public int hashCode()
	{
		return _declaringClass.hashCode() ^ _methodID.hashCode();
	}

	public void createNewBytecodeSection(long start, long size)
	{
		BytecodeImageSection bytecodeSection = new BytecodeImageSection(_methodID.getAddressSpace().getPointer(start), size);
		_bytecodeSections.add(bytecodeSection);
	}

	public void createNewJITSection(long id, long start, long size)
	{
		JitImageSection jitSection = new JitImageSection(_methodID.getAddressSpace().getPointer(id), _methodID.getAddressSpace().getPointer(start), size);
		_compiledSections.add(jitSection);
	}
	public String toString() {
		try {
			return getDeclaringClass().getName() + "." + getName() + getSignature();
		} catch (CorruptDataException e) {
		} catch (DataUnavailable e) {
		}
		return super.toString();
	}
}
