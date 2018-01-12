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

import java.util.HashMap;
import java.util.Map;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.java.IDTFJJavaField;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public abstract class DTFJJavaField implements JavaField, IDTFJJavaField {
	protected static final Class<?>[] whitelist = new Class<?>[]{IllegalArgumentException.class};
	protected static final int FIELD_BOOLEAN = 0x1;
	protected static final int FIELD_BYTE = 0x2;
	protected static final int FIELD_SHORT = 0x4;
	protected static final int FIELD_CHAR = 0x8;
	protected static final int FIELD_INTEGER = 0x10;
	protected static final int FIELD_FLOAT = 0x20;
	protected static final int FIELD_LONG = 0x40;
	protected static final int FIELD_DOUBLE = 0x80;
	protected static final int FIELD_ARRAY = 0x100;
	protected static final int FIELD_OBJECT = 0x200;
	protected final J9ROMFieldShapePointer j9field;
	protected final J9ObjectFieldOffset fieldOffset;
	protected final String name;
	protected DTFJJavaClass clazz = null;
	protected int modifiers = 0;
	private char sigFlag = 0;
	
	public DTFJJavaField(DTFJJavaClass clazz, J9ObjectFieldOffset ptr) throws com.ibm.j9ddr.CorruptDataException {
		this.clazz = clazz;
		fieldOffset = ptr;
		this.j9field = ptr.getField();
		modifiers = J9ROMFieldShapeHelper.getReflectModifiers(j9field); //extract the modifiers to determine the field type
		name = J9ROMFieldShapeHelper.getName(j9field);
	}
	
	protected char getSigFlag() throws CorruptDataException {
		//now check that the field value can be converted
		try {
			if(sigFlag == 0) {
				sigFlag = getSignature().charAt(0);
			}
			switch(sigFlag) {
			case BOOLEAN_SIGNATURE :
			case BYTE_SIGNATURE :
			case CHAR_SIGNATURE :
			case SHORT_SIGNATURE :
			case INTEGER_SIGNATURE :
			case FLOAT_SIGNATURE :
			case LONG_SIGNATURE :
			case DOUBLE_SIGNATURE :
			case ARRAY_PREFIX_SIGNATURE :
			case OBJECT_PREFIX_SIGNATURE :
				return sigFlag;
			default :
				throw J9DDRDTFJUtils.newCorruptDataException(DTFJContext.getProcess(), new com.ibm.j9ddr.CorruptDataException("Signature flag for this field is invalid: " + sigFlag + String.format("[\\u%04x]",(int)sigFlag)));
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	protected DTFJJavaObject validateJavaObject(JavaObject object) throws CorruptDataException {
		if(!(object instanceof DTFJJavaObject)) {
			throw new IllegalArgumentException(String.format("Unregonised JavaObject implementor : %s", object.getClass().getName()));
		}
		if(!isAncestorOf(object.getJavaClass())) {
			throw new IllegalArgumentException("The class for the JavaObject specified as a parameter does not match with the declaring class of this JavaField.");
		}
		return (DTFJJavaObject) object;
	}
	
	protected long checkDataTypeConversion(DTFJJavaObject object, int mask) throws CorruptDataException {
		try {
			switch(getSigFlag()) {
				case BOOLEAN_SIGNATURE :
					if((mask & FIELD_BOOLEAN) != FIELD_BOOLEAN) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return 0;
				case BYTE_SIGNATURE :
					if((mask & FIELD_BYTE) != FIELD_BYTE) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getByteField(object.getJ9ObjectPointer(), fieldOffset);
				case CHAR_SIGNATURE :
					if((mask & FIELD_CHAR) != FIELD_CHAR) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getCharField(object.getJ9ObjectPointer(), fieldOffset);
				case SHORT_SIGNATURE :
					if((mask & FIELD_SHORT) != FIELD_SHORT) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getShortField(object.getJ9ObjectPointer(), fieldOffset);
				case INTEGER_SIGNATURE :
					if((mask & FIELD_INTEGER) != FIELD_INTEGER) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getIntField(object.getJ9ObjectPointer(), fieldOffset);
				case FLOAT_SIGNATURE :
					if((mask & FIELD_FLOAT) != FIELD_FLOAT) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getIntField(object.getJ9ObjectPointer(), fieldOffset);
				case LONG_SIGNATURE :
					if((mask & FIELD_LONG) != FIELD_LONG) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getLongField(object.getJ9ObjectPointer(), fieldOffset);
				case DOUBLE_SIGNATURE :
					if((mask & FIELD_DOUBLE) != FIELD_DOUBLE) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return J9ObjectHelper.getLongField(object.getJ9ObjectPointer(), fieldOffset);
				case ARRAY_PREFIX_SIGNATURE :
					if((mask & FIELD_ARRAY) != FIELD_ARRAY) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return 0;
				case OBJECT_PREFIX_SIGNATURE :
					if((mask & FIELD_OBJECT) != FIELD_OBJECT) {
						throw new IllegalArgumentException(String.format("The field type : %c cannot be converted to the requested data type", sigFlag));
					}
					return 0;
				default :
					throw new IllegalArgumentException("Cannot determine the correct data size");
			}
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw J9DDRDTFJUtils.newCorruptDataException(DTFJContext.getProcess(), e);
		}
	}
	
	/**
	 * Function to check that the supplied class is an ancestor of this one
	 * @param theClass
	 * @return
	 */
	protected boolean isAncestorOf(JavaClass theClass) {
		if (null == theClass) {
			return false;
		} else if (clazz.equals(theClass)) {
			return true;
		} else {
			try {
				return this.isAncestorOf(theClass.getSuperclass());
			} catch (CorruptDataException cde) {
				return false;
			}
		}
	}

	public JavaClass getDeclaringClass() throws CorruptDataException, DataUnavailable {
		if(clazz == null) {		//have to get declaring class
			throw new DataUnavailable("The declaring class is not available");
		}
		return clazz;
	}

	public int getModifiers() throws CorruptDataException {
		return modifiers;
	}

	public String getName() throws CorruptDataException {
		return name;
	}

	private static Map<J9ROMFieldShapePointer,String> signatureCache = new HashMap<J9ROMFieldShapePointer,String>();
	
	public String getSignature() throws CorruptDataException {
		String cachedSignature = signatureCache.get(j9field);
		
		if (null == cachedSignature) {		
			try {
				cachedSignature = J9ROMFieldShapeHelper.getSignature(j9field);
				
				signatureCache.put(j9field,cachedSignature);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		
		return cachedSignature;
	}

	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaField) {
			JavaField field = (JavaField) obj;
			boolean hasSameName;
			try {
				hasSameName = name.equals(field.getName());
			} catch (CorruptDataException cde) {
				hasSameName=false;
			}
			try {
				isEqual =  hasSameName && (getClass().equals(field.getClass())) && clazz.equals(field.getDeclaringClass());
			} catch (Throwable t) {
				J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return isEqual;
	}
	
	public int hashCode()
	{
		return name.hashCode(); 
	}
	
	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		try {
			data.append(getName());
		} catch (CorruptDataException e) {
			data.append("Instance Field [unknown name]");
		}
		data.append(" @0x");
		data.append(Long.toHexString(j9field.getAddress()));
		return data.toString();
	}
	
}
