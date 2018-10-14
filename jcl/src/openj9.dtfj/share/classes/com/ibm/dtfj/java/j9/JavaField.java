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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 */
public abstract class JavaField implements com.ibm.dtfj.java.JavaField
{
	//these should be put somewhere more accessible since they are useful constants for any user of the API
	protected static final String BOOLEAN_SIGNATURE = "Z";
	protected static final String BYTE_SIGNATURE = "B";
	protected static final String CHAR_SIGNATURE = "C";
	protected static final String SHORT_SIGNATURE = "S";
	protected static final String INTEGER_SIGNATURE = "I";
	protected static final String LONG_SIGNATURE = "J";
	protected static final String FLOAT_SIGNATURE = "F";
	protected static final String DOUBLE_SIGNATURE = "D";
	protected static final String OBJECT_PREFIX_SIGNATURE = "L";
	protected static final String ARRAY_PREFIX_SIGNATURE = "[";

	protected JavaRuntime _javaVM;
	private String _name;
	private String _signature;
	private int _modifiers;
	private long _declaringClassID;
	
	protected JavaField(JavaRuntime vm, String name, String signature, int modifiers, long declaringClassID)
	{
		if (null == vm) {
			throw new IllegalArgumentException("Java VM for a field must not be null");
		}
		if (null == name) {
			throw new IllegalArgumentException("A Java Field requires a non-null name");
		}
		if (null == signature) {
			throw new IllegalArgumentException("A Java Field requires a non-null signature");
		}
		_javaVM = vm;
		_name = name;
		_signature = signature;
		_modifiers = modifiers;
		_declaringClassID = declaringClassID;
	}

	public Object get(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(BOOLEAN_SIGNATURE)) {
			return Boolean.valueOf(getBoolean(object));
		} else if (getSignature().equals(BYTE_SIGNATURE)) {
			return Byte.valueOf(getByte(object));
		} else if (getSignature().equals(CHAR_SIGNATURE)) {
			return Character.valueOf(getChar(object));
		} else if (getSignature().equals(SHORT_SIGNATURE)) {
			return Short.valueOf(getShort(object));
		} else if (getSignature().equals(INTEGER_SIGNATURE)) {
			return Integer.valueOf(getInt(object));
		} else if (getSignature().equals(LONG_SIGNATURE)) {
			return Long.valueOf(getLong(object));
		} else if (getSignature().equals(FLOAT_SIGNATURE)) {
			return Float.valueOf(getFloat(object));
		} else if (getSignature().equals(DOUBLE_SIGNATURE)) {
			return Double.valueOf(getDouble(object));
		} else if (getSignature().startsWith(OBJECT_PREFIX_SIGNATURE) || getSignature().startsWith(ARRAY_PREFIX_SIGNATURE)) {
			return getReferenceType(object);
		} else {
			//who knows what this is?
			throw new IllegalArgumentException("unidentified object signature ["+getSignature()+"]");
		}
	}
	
	public long getLong(JavaObject object) throws CorruptDataException, MemoryAccessException {
		if (getSignature().equals(LONG_SIGNATURE)) {
			// should not happen - handled in subclasses
			throw new IllegalArgumentException();
		} else if (getSignature().equals(BYTE_SIGNATURE)){
			return getByte(object);
		} else if (getSignature().equals(SHORT_SIGNATURE)) {
			return getShort(object);
		} else if (getSignature().equals(CHAR_SIGNATURE)) {
			return getChar(object);
		} else if (getSignature().equals(INTEGER_SIGNATURE)){
			return getInt(object);
		} else {
			throw new IllegalArgumentException("unexpected object signature ["+getSignature()+"] cannot retrieve long");
		}

	}
	
	public double getDouble(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(DOUBLE_SIGNATURE)) {
			// should not happen - handled in subclasses
			throw new IllegalArgumentException();
		} else if (getSignature().equals(FLOAT_SIGNATURE)){
			return getFloat(object);
		} else if (getSignature().equals(BYTE_SIGNATURE)){
			return getByte(object);
		} else if (getSignature().equals(SHORT_SIGNATURE)) {
			return getShort(object);
		} else if (getSignature().equals(CHAR_SIGNATURE)) {
			return getChar(object);
		} else if (getSignature().equals(INTEGER_SIGNATURE)){
			return getInt(object);
		} else if (getSignature().equals(LONG_SIGNATURE)){
			return getLong(object);
		} else {
			throw new IllegalArgumentException("unexpected object signature ["+getSignature()+"] cannot retrieve double");
		}
	}
	
	public float getFloat(JavaObject object) throws CorruptDataException, MemoryAccessException {
		if (getSignature().equals(FLOAT_SIGNATURE)) {
			// should not happen - handled in subclasses
			throw new IllegalArgumentException();
		} else if (getSignature().equals(BYTE_SIGNATURE)){
			return getByte(object);
		} else if (getSignature().equals(SHORT_SIGNATURE)) {
			return getShort(object);
		} else if (getSignature().equals(CHAR_SIGNATURE)) {
			return getChar(object);
		} else if (getSignature().equals(INTEGER_SIGNATURE)){
			return getInt(object);
		} else if (getSignature().equals(LONG_SIGNATURE)){
			return getLong(object);
		} else {
			throw new IllegalArgumentException("unexpected object signature ["+getSignature()+"] cannot retrieve float");
		}
	}

	public int getInt(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(INTEGER_SIGNATURE)) {
			// should not happen - handled in subclasses
			throw new IllegalArgumentException();
		} else if (getSignature().equals(BYTE_SIGNATURE)){
			return getByte(object);
		} else if (getSignature().equals(SHORT_SIGNATURE)) {
			return getShort(object);
		} else if (getSignature().equals(CHAR_SIGNATURE)) {
			return getChar(object);
		} else {
			throw new IllegalArgumentException("unexpected object signature ["+getSignature()+"] cannot retrieve int");
		}
	}
	
	public short getShort(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(SHORT_SIGNATURE)) {
			// should not happen - handled in subclasses
			throw new IllegalArgumentException();
		} else if (getSignature().equals(BYTE_SIGNATURE)){
			return getByte(object);
		} else {
			throw new IllegalArgumentException("unexpected object signature ["+getSignature()+"] cannot retrieve short");
		}
	}

	
	protected abstract Object getReferenceType(JavaObject object) throws CorruptDataException, MemoryAccessException;
	
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
	public JavaClass getDeclaringClass() throws CorruptDataException
	{
		JavaClass ret = _javaVM.getClassForID(_declaringClassID);
		if (ret == null) {
			throw new CorruptDataException(new CorruptData("Unknown declaring class ID " + _declaringClassID, null));
		}
		return ret;
		
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getName()
	 */
	public String getName() throws CorruptDataException
	{
		return _name;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaMember#getSignature()
	 */
	public String getSignature() throws CorruptDataException
	{
		return _signature;
	}
	
	public String getString(JavaObject hostObject) throws CorruptDataException, MemoryAccessException
	{
		// Cannot implement this method without knowing the underlying endianness of the core file since Strings since Java 9 are encoded using
		// a backing byte[] which can (if compression is disabled) represent a character using two bytes in native endianness.
		throw new CorruptDataException(new CorruptData("Cannot read String field in an endian aware manner", null));
	}
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaField) {
			JavaField field = (JavaField) obj;
			boolean hasSameName;
			try {
				hasSameName=_name.equals(field.getName());
			} catch (CorruptDataException cde) {
				hasSameName=false;
			}
			isEqual =  hasSameName && (getClass().equals(field.getClass())) && (_javaVM.equals(field._javaVM) && (_declaringClassID == field._declaringClassID));
		}
		return isEqual;
	}
	
	public int hashCode()
	{
		return (_name.hashCode() ^ _signature.hashCode() ^ _javaVM.hashCode() ^ (((int)_declaringClassID) ^ ((int) (_declaringClassID >> 32)))); 
	}
	public String toString() {
		try {
			return getDeclaringClass().getName() + "." + getName() + " : " + getSignature();
		} catch (CorruptDataException e) {
		}
		return super.toString();
	}
}
