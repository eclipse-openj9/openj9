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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public class JavaInstanceField extends JavaField
{
	private int _offset;
	
	public JavaInstanceField(JavaRuntime vm, String name, String signature, int modifiers, int offset, long classID)
	{
		super(vm, name, signature, modifiers, classID);
		if (null == vm) {
			throw new IllegalArgumentException("Java VM for an instance field cannot be null");
		}
		_offset = offset;
	}
	
	private void checkDeclaringClass(JavaObject object) throws CorruptDataException {
		com.ibm.dtfj.java.j9.JavaClass declaringClass = (com.ibm.dtfj.java.j9.JavaClass)getDeclaringClass();
		if (!(declaringClass.isAncestorOf(object.getJavaClass()))) {
			throw new IllegalArgumentException("The class for the JavaObject specified as a parameter does not match with the declaring class of this JavaField.");
		}
	}
	
	public Object getReferenceType(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			String sigPrefix = getSignature();
			
			if (sigPrefix.startsWith(OBJECT_PREFIX_SIGNATURE) || sigPrefix.startsWith(ARRAY_PREFIX_SIGNATURE)) {
				ImagePointer value = ((com.ibm.dtfj.java.j9.JavaObject)object).getFObjectAtOffset(_offset);
				//CMVC 173262 - return null if the reference object is null
				if(0 == value.getAddress()) {
					return null;		//points to a null reference
				}
				try {
					return _javaVM.getObjectAtAddress(value);
				} catch (IllegalArgumentException e) {
					// getObjectAtAddress can throw an IllegalArgumentException if the address is not aligned
					throw new CorruptDataException(new com.ibm.dtfj.image.j9.CorruptData(e.getMessage(),value));
				}
			} else {
				throw new IllegalArgumentException();
			}
		} else {
			throw new NullPointerException();
		}
	}

	public boolean getBoolean(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(BOOLEAN_SIGNATURE)) {
				//how big are our booleans?
				return ( 0 != object.getID().getIntAt(_offset));
			} else {
				throw new IllegalArgumentException();
			}
		} else {
			throw new NullPointerException();
		}
	}

	public byte getByte(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(BYTE_SIGNATURE)) {
				return (byte)object.getID().getIntAt(_offset);
			} else {
				throw new IllegalArgumentException();
			}
		} else {
			throw new NullPointerException();
		}
	}

	public char getChar(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(CHAR_SIGNATURE)) {
				return (char) object.getID().getIntAt(_offset);
			} else {
				throw new IllegalArgumentException();
			}
		} else {
			throw new NullPointerException();
		}
	}

	public double getDouble(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(DOUBLE_SIGNATURE)) {
				return object.getID().getDoubleAt(_offset);
			} else {
				return super.getDouble(object);
			}
		} else {
			throw new NullPointerException();
		}
	}

	public float getFloat(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(FLOAT_SIGNATURE)) {
				return object.getID().getFloatAt(_offset);
			} else {
				return super.getFloat(object);
			}
		} else {
			throw new NullPointerException();
		}
	}

	public int getInt(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(INTEGER_SIGNATURE)) {
				return object.getID().getIntAt(_offset);
			} else {
				return super.getInt(object);
			}
		} else {
			throw new NullPointerException();
		}
	}

	public long getLong(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(LONG_SIGNATURE)) {
				return object.getID().getLongAt(_offset);
			} else {
				return super.getLong(object);
			}
		} else {
			throw new NullPointerException();
		}
	}

	public short getShort(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		//sanity check
		if (_isSafeToAccess(object)) {
			checkDeclaringClass(object);
			if (getSignature().equals(SHORT_SIGNATURE)) {
				return (short)object.getID().getIntAt(_offset);
			} else {
				return super.getShort(object);
			}
		} else {
			throw new NullPointerException();
		}
	}
	
	private boolean _isSafeToAccess(JavaObject object)
	{
		return ((null != object) && (0 != object.getID().getAddress()));
	}
}
