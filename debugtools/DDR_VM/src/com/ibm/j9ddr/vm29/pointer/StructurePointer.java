/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.GeneratedFieldAccessor;
import com.ibm.j9ddr.GeneratedPointerClass;
import com.ibm.j9ddr.NullPointerDereference;
import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.util.RuntimeTypeResolutionUtils;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U32;

/**
 * Root of the hierarchy for VM C structures.
 */
public abstract class StructurePointer extends AbstractPointer {
	
	private final static String nl = System.getProperty("line.separator");
	
	protected StructurePointer(long address) {
		super(address);
	}
	
	public final long nonNullFieldEA(long offset) throws NullPointerDereference {
		return nonNullAddress() + offset;
	}

	public DataType at(long count) {
		throw new UnsupportedOperationException("StructurePointers are implicitly dereferenced.  Use add(long count) instead.");
	}
	
	public DataType at(Scalar count) {
		throw new UnsupportedOperationException("StructurePointers are implicitly dereferenced.  Use add(Scalar count) instead.");
	}
	
	protected int getStartingBit(int s, int b) {
		switch (BITFIELD_FORMAT) {
		case StructureReader.BIT_FIELD_FORMAT_LITTLE_ENDIAN:
			return b + (s % StructureReader.BIT_FIELD_CELL_SIZE);
		case StructureReader.BIT_FIELD_FORMAT_BIG_ENDIAN:
			return  32 - (s % StructureReader.BIT_FIELD_CELL_SIZE);
		default:
			throw new IllegalArgumentException("Unsupported bitfield format");
		}
	}

	protected U32 getU32Bitfield(int s, int b) throws CorruptDataException {
		int cell = s/StructureReader.BIT_FIELD_CELL_SIZE;
		U32 cellValue = new U32(getIntAtOffset(cell * StructureReader.BIT_FIELD_CELL_SIZE / 8));
		int n = getStartingBit(s, b);
		U32 returnValue = cellValue.leftShift(StructureReader.BIT_FIELD_CELL_SIZE - n);
		returnValue = returnValue.rightShift(StructureReader.BIT_FIELD_CELL_SIZE - b);
		return returnValue;
	}
	
	protected I32 getI32Bitfield(int s, int b) throws CorruptDataException {
		int cell = s/StructureReader.BIT_FIELD_CELL_SIZE;
		I32 cellValue = new I32(getIntAtOffset(cell * StructureReader.BIT_FIELD_CELL_SIZE / 8));
		int n = getStartingBit(s, b);
		I32 returnValue = cellValue.leftShift(StructureReader.BIT_FIELD_CELL_SIZE - n);
		returnValue = returnValue.rightShift(StructureReader.BIT_FIELD_CELL_SIZE - b);
		return returnValue;
	}
	
	public StructurePointer getAsRuntimeType()
	{
		// Design 42819
		// To make debugging easier we provide the facility to obtain the pointer 
		// as its actual run-time type (if this information is available).

		try {
			Method typeIdGetter = getClass().getMethod("_typeId");
			U8Pointer strPtr = (U8Pointer) typeIdGetter.invoke(this);
			String typeStr = strPtr.getCStringAtOffset(0);
			if (strPtr.notNull()) {
				typeStr = RuntimeTypeResolutionUtils.cleanTypeStr(typeStr);
				Class<?> runtimeClass = Class.forName(getClass().getPackage().getName() + "." + typeStr + "Pointer");
				Method castMethod = runtimeClass.getMethod("cast", AbstractPointer.class);
				Object retVal = castMethod.invoke(null, this);
				return (StructurePointer) retVal;
			}
		} catch (Throwable th) {
			// Do nothing.
		}
		return this;
	}
	
	public static class StructureField implements Comparable<StructureField> 
	{	
		public final int offset;
		public final String name;
		public final DataType value;
		public final String type;
		public final CorruptDataException cde;
		
		StructureField(String name, String type, int offset, DataType value, CorruptDataException cde)
		{
			this.name = name;
			this.offset = offset;
			this.value = value;
			this.type = type;
			this.cde = cde;
		}
		
		public int compareTo(StructureField o)
		{
			return offset - o.offset;
		}
		
		public String toString()
		{
			StringBuilder builder = new StringBuilder();
			builder.append(type);
			builder.append(" ");
			builder.append(name);
			builder.append(" (");
			if(cde == null) {
				builder.append(Integer.toHexString(offset));	
			} else {
				builder.append("<FAULT: " + cde.getMessage() + ">");
			}
			builder.append(")");
			return builder.toString();
		}
	}
	
	@Override
	public String formatFullInteractive()
	{
		StringBuilder builder = new StringBuilder();
		
		builder.append(this.getTargetName());
		builder.append(" at ");
		builder.append("0x");
		builder.append(Long.toHexString(this.getAddress()));
		builder.append(" {");
		builder.append(nl);
				
		StructureField fields[] = getStructureFields();
		
		for (StructureField thisField : fields) {
			builder.append("\t0x");
			builder.append(Integer.toHexString(thisField.offset));
			builder.append(": ");
			builder.append(thisField.type);
			builder.append(" ");
			builder.append(thisField.name);
			builder.append(" = ");
			if (thisField.cde != null) {
				builder.append("<FAULT: " + thisField.cde.getMessage() + ">");
			} else if (thisField.value != null) {
				builder.append(thisField.value.formatShortInteractive());
			} else {
				builder.append("null");
			}
			
			builder.append(nl);
		}
		
		builder.append("}");
		builder.append("\n");
		return builder.toString();
	}
	
	public StructureField[] getStructureFields()
	{
		List<StructureField> fields = new LinkedList<StructureField>();
		
		Class<?> working = this.getClass();
		
		while (working != null) {
			GeneratedPointerClass classAnnotation = working.getAnnotation(GeneratedPointerClass.class);
			
			if (null == classAnnotation) {
				break;
			}
			
			for (Method thisMethod : working.getMethods()) {
				if (thisMethod.isAnnotationPresent(GeneratedFieldAccessor.class)) {
					GeneratedFieldAccessor fieldAnnotation = thisMethod.getAnnotation(GeneratedFieldAccessor.class);
					
					Field offsetField = null;
					try {
						offsetField = classAnnotation.structureClass().getField(fieldAnnotation.offsetFieldName());
					} catch (SecurityException e) {
						throw new Error("Unexpected security exception", e);
					} catch (NoSuchFieldException e) {
						//This will happen if we reach for a field that doesn't exist on this level
						continue;
					}
					
					int offset = -1;
					try {
						offset = offsetField.getInt(null);
					} catch (Exception e) {
						throw new Error(e);
					}
			
					DataType result = null;
					CorruptDataException cde = null;
					try {
						result = (DataType) thisMethod.invoke(this);
					} catch (IllegalArgumentException e) {
						throw new RuntimeException(e);
					} catch (IllegalAccessException e) {
						throw new RuntimeException(e);
					} catch (InvocationTargetException e) {
						Throwable cause = e.getCause();
						
						if (cause instanceof CorruptDataException) {
							cde = (CorruptDataException) cause;
						} else {
							throw new RuntimeException(e);
						}
					}
					fields.add(new StructureField(thisMethod.getName(), fieldAnnotation.declaredType(), offset, result, cde));
				}
			}
			
			working = working.getSuperclass();
		}
		
		Collections.sort(fields);
		
		StructureField[] result = new StructureField[fields.size()];
		fields.toArray(result);
		return result;
	}
	
}
