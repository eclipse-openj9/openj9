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
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public class JavaStaticField extends JavaField
{
	private String _value;
	
	public JavaStaticField(JavaRuntime runtime, String name, String sig, int modifiers, String value, long declaringClassID)
	{
		super(runtime, name, sig, modifiers, declaringClassID);
		if (null == runtime) {
			throw new IllegalArgumentException("A Java static field cannot exist in a null Java VM");
		}
		_value = value;
	}

	public Object getReferenceType(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		String sig = getSignature();
		
		if (sig.startsWith(OBJECT_PREFIX_SIGNATURE) || sig.startsWith(ARRAY_PREFIX_SIGNATURE)) {
			try {
				ImagePointer pointer = getDeclaringClass().getID().getAddressSpace().getPointer(parse(16));
				//CMVC 173262 - return null if the reference object is null
				if(0 == pointer.getAddress()) {
					return null;		//points to a null reference
				}
				try {
					return _javaVM.getObjectAtAddress(pointer);
				} catch (IllegalArgumentException e) {
					// getObjectAtAddress can throw an IllegalArgumentException if the address is not aligned
					throw new CorruptDataException(new com.ibm.dtfj.image.j9.CorruptData(e.getMessage(),pointer));
				}
			} catch (NumberFormatException e) {
				return null;
			}
		} else {
			throw new IllegalArgumentException("field is not of suitable type. Actual signature is "+getSignature());
		}
	}

	public boolean getBoolean(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(BOOLEAN_SIGNATURE)) {
			if (null == _value ) throw new CorruptDataException(new CorruptData("parse error: value is null", null));
			return _value.charAt(0) != '0';
		} else {
			throw new IllegalArgumentException("field is not of type boolean. actual signature is "+getSignature());
		}
	}

	public byte getByte(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(BYTE_SIGNATURE)) {
			return (byte) parse(2);
		} else {
			throw new IllegalArgumentException("field is not of type byte. actual signature is "+getSignature());
		}
	}

	public char getChar(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
 		if (getSignature().equals(CHAR_SIGNATURE)) {
			return (char) parse(4);
		} else {
			throw new IllegalArgumentException("field is not of type char. actual signature is "+getSignature());
		}
	}

	public double getDouble(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(DOUBLE_SIGNATURE)) {
			long temp = parse(16);
			return Double.longBitsToDouble(temp);
		} else {
			return super.getDouble(object);
		}
	}

	public float getFloat(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(FLOAT_SIGNATURE)) {
			int temp = (int)parse(8);
			return Float.intBitsToFloat(temp);
		} else {
			return super.getFloat(object);
		}
	}

	public int getInt(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(INTEGER_SIGNATURE)) {
			return (int) parse(8);
		} else {
			return super.getInt(object);
		}
	}

	public long getLong(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(LONG_SIGNATURE)) {
			return parse(16);
		} else {
			return super.getLong(object);
		}
	}

	public short getShort(JavaObject object) throws CorruptDataException, MemoryAccessException
	{
		if (getSignature().equals(SHORT_SIGNATURE)) {
			return (short) parse(4);
		} else {
			return super.getShort(object);
		}
	}
	
	private long parse(int maxLength) throws CorruptDataException {
		if (null == _value ) throw new CorruptDataException(new CorruptData("parse error: value is null", null));
		if (_value.length() > maxLength) throw new CorruptDataException(new CorruptData("parse error: value ["+_value+ "] length "+_value.length()+" exceeds maximum of "+maxLength, null));			

		if (16 == _value.length()) {
			// split and shift since this would overflow
			String highS = _value.substring(0, 8);
			String lowS = _value.substring(8, 16);
			long high = Long.parseLong(highS, 16);
			long low = Long.parseLong(lowS, 16);
			return (high << 32) | low;
		}
		return Long.parseLong(_value, 16);
	}
}
