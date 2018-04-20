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
import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.util.RuntimeTypeResolutionUtils;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.types.I8;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.I64;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;

/**
 * Root of the hierarchy for VM C structures.
 */
public abstract class StructurePointer extends AbstractPointer {

	private final static String nl = System.getProperty("line.separator");

	protected StructurePointer(long address) {
		super(address);
	}

	public DataType at(long count) {
		throw new UnsupportedOperationException("StructurePointers are implicitly dereferenced.  Use add(long count) instead.");
	}
	
	public DataType at(Scalar count) {
		throw new UnsupportedOperationException("StructurePointers are implicitly dereferenced.  Use add(Scalar count) instead.");
	}

	protected U8 getU8Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 8);
		long cell = getByteAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 8);

		return new U8(field);
	}

	protected U16 getU16Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 16);
		long cell = getShortAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 16);

		return new U16(field);
	}

	protected U32 getU32Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 32);
		long cell = getIntAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 32);

		return new U32(field);
	}

	protected U64 getU64Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 64);
		long cell = getLongAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 64);

		return new U64(field);
	}

	protected I8 getI8Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 8);
		long cell = getByteAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 8);

		return new I8(field);
	}

	protected I16 getI16Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 16);
		long cell = getShortAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 16);

		return new I16(field);
	}

	protected I32 getI32Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 32);
		long cell = getIntAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 32);

		return new I32(field);
	}

	protected I64 getI64Bitfield(int bitOffset, int bitWidth) throws CorruptDataException {
		long byteOffset = getByteOffset(bitOffset, 64);
		long cell = getLongAtOffset(byteOffset);
		long field = getBitfield(cell, bitOffset, bitWidth, 64);

		return new I64(field);
	}

	private static long getByteOffset(int bitOffset, int cellSize) {
		long cellIndex = bitOffset / cellSize;

		return cellIndex * (cellSize / Byte.SIZE);
	}

	private static long getBitfield(long cell, int bitOffset, int bitWidth, int cellSize) {
		int shiftAmount;

		switch (BITFIELD_FORMAT) {
		case StructureReader.BIT_FIELD_FORMAT_LITTLE_ENDIAN:
			shiftAmount = bitOffset % cellSize;
			break;
		case StructureReader.BIT_FIELD_FORMAT_BIG_ENDIAN:
			// We must ensure shiftAmount is non-negative.
			shiftAmount = (bitOffset + bitWidth) % cellSize;
			if (shiftAmount != 0) {
				shiftAmount = cellSize - shiftAmount;
			}
			break;
		default:
			throw new IllegalArgumentException("Unsupported bitfield format");
		}

		return (cell >>> shiftAmount) & ((1L << bitWidth) - 1);
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
