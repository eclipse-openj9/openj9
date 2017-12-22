/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.view.dtfj.DTFJConstants.ARRAY_PREFIX_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.BOOLEAN_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.BYTE_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.CHAR_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.DOUBLE_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.FLOAT_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.INTEGER_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.LONG_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.OBJECT_PREFIX_SIGNATURE;
import static com.ibm.j9ddr.view.dtfj.DTFJConstants.SHORT_SIGNATURE;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.java.helper.DTFJJavaFieldHelper;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class DTFJJavaFieldStatic extends DTFJJavaField {
		
	public DTFJJavaFieldStatic(DTFJJavaClass clazz, J9ObjectFieldOffset ptr)	throws com.ibm.j9ddr.CorruptDataException {
		super(clazz, ptr);
	}

	public Object get(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {	
			switch(getSigFlag()) {
				case BOOLEAN_SIGNATURE :
					return Boolean.valueOf(getBoolean(object));
				case BYTE_SIGNATURE :
					return Byte.valueOf(getByte(object));
				case CHAR_SIGNATURE :
					return Character.valueOf(getChar(object));
				case SHORT_SIGNATURE :
					return Short.valueOf(getShort(object));
				case INTEGER_SIGNATURE :
					return Integer.valueOf(getInt(object));
				case FLOAT_SIGNATURE :
					return new Float(getFloat(object));
				case LONG_SIGNATURE :
					return Long.valueOf(getLong(object));
				case DOUBLE_SIGNATURE :
					return new Double(getDouble(object));
				case ARRAY_PREFIX_SIGNATURE :
				case OBJECT_PREFIX_SIGNATURE :
					J9ObjectPointer data = getObject();
					if(data.isNull()) {
						return null;
					} else {
						return new DTFJJavaObject(null, data);
					}
				default :
					throw new IllegalArgumentException("Cannot determine the correct data type");
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public boolean getBoolean(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			int data = getIntField();
			return (data == 1);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public byte getByte(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			byte data = (byte)(getIntField() & 0xFF);	
			return data;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public char getChar(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			char data = (char)(getIntField() & 0xFFFF);
			return data;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public double getDouble(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			long data = getLongField();
			return Double.longBitsToDouble(data);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public float getFloat(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			int data = getIntField();
			return Float.intBitsToFloat(data);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public int getInt(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			int data = getIntField();
			return data;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}
	
	public long getLong(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			long data = getLongField();
			return data;
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public short getShort(JavaObject object) throws CorruptDataException, MemoryAccessException {
		try {
			int data = getIntField();
			return (short)(data & 0xFFFF);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}

	public String getString(JavaObject object) throws CorruptDataException, MemoryAccessException {
		if ( ! DTFJJavaFieldHelper.fieldIsString(this)) {
			throw new IllegalArgumentException("JavaField.getString() called on non-String field.");
		}
		try {
			J9ObjectPointer data = getObject();
			return J9ObjectHelper.stringValue(data);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAllButMemAccExAsCorruptDataException(DTFJContext.getProcess(), t, whitelist);
		}
	}
	
	private J9ObjectPointer getObject() throws com.ibm.j9ddr.CorruptDataException {

		UDATAPointer pointer = clazz.getJ9Class().ramStatics().addOffset(fieldOffset.getOffsetOrAddress());
		return J9ObjectPointer.cast(pointer.at(0));
	}
	
	private int getIntField() throws com.ibm.j9ddr.CorruptDataException {
		I32Pointer pointer = I32Pointer.cast(clazz.getJ9Class().ramStatics().addOffset(fieldOffset.getOffsetOrAddress()));
		return pointer.at(0).intValue();
	}
	
	private long getLongField() throws com.ibm.j9ddr.CorruptDataException {
		I64Pointer pointer = I64Pointer.cast(clazz.getJ9Class().ramStatics().addOffset(fieldOffset.getOffsetOrAddress()));
		return pointer.at(0).longValue();
	}
	
	public boolean isNestedPacked() {
		// vm29 does not support packed
		return false;
	}

	public boolean isNestedPackedArray() {
		// vm29 does not support packed
		return false;
	}

	public int getPackedLengthAnnotationValue() {
		// vm29 does not support packed
		return 0;
	}

}
