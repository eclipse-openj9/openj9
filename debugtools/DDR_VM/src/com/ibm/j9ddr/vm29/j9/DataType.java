/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.InvalidDataTypeException;
import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.PackageNameType;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class DataType {
	
	// Fields
	protected static IProcess process;
	protected static byte SIZEOF_BOOL;
	protected static byte BITFIELD_FORMAT;
	static J9RASPointer J9RASPOINTER;
	protected static String pointerPackageName;
	protected static String structurePackageName;

	// Static methods
	
	// Invoked via reflection from VMDataFactory.getVMData(IProcess);
	public static void init(IProcess process, StructureReader structureReader) {
		DataType.process = process;
		SIZEOF_BOOL = structureReader.getSizeOfBool();
		BITFIELD_FORMAT = structureReader.getBitFieldFormat();
		pointerPackageName = structureReader.getPackageName(PackageNameType.POINTER_PACKAGE_DOT_NAME);
		structurePackageName = structureReader.getPackageName(PackageNameType.STRUCTURE_PACKAGE_DOT_NAME);
	}
	
	// Called via reflection in VMDataFactory.getVMDaata(IProcess)
	public static void setJ9RASPointer(J9RASPointer j9RASPoitner) {
		J9RASPOINTER = j9RASPoitner;
	}
	
	public static J9RASPointer getJ9RASPointer() {
		return J9RASPOINTER;
	}
	
	public static IProcess getProcess() {
		return process;
	}
			
	@SuppressWarnings("unchecked")
	public static DataType getStructure(String name, long address) 
	{	
		if(address == 0) {
			return null;
		}

		try {
			Class<? extends DataType> clazz;
			try {
				// Probably a structure pointer
				clazz = (Class<? extends StructurePointer>) Class.forName(pointerPackageName + "." + name);
			} catch(ClassNotFoundException notFound) {
				// Might be a built-in type
				try {
					clazz = (Class<? extends DataType>)Class.forName(UDATAPointer.class.getPackage().getName() + "." + name);
				} catch(ClassNotFoundException stillNotFound) {
					throw notFound;
				}
			}	
			return getStructure(clazz, address);
		} catch (Exception e) {
			throw new IllegalStateException("Cannot create the structure " + name, e);
		}
	}

	public static DataType getStructure(String name, UDATA udata) 
	{
		return getStructure(name, udata.longValue());
	}
	
	@SuppressWarnings("unchecked")
	public static <T> T getStructure(Class<T> clazz, long address) 
	{
		if(address == 0) {
			return null;
		}
		
		try {
			java.lang.reflect.Method method = clazz.getDeclaredMethod("cast", new Class[] { long.class });
			T result = (T) method.invoke(null, new Object[] { address});
			return result;
		} catch (Exception e) {
			throw new IllegalStateException("Cannot create the structure " + clazz.getSimpleName(), e);
		}
	}
	
	// Instance methods
	
	// Answer the long representation of the receiver.  This value may not have useful meaning in all contexts
	public abstract long longValue() throws CorruptDataException;
	
	/**
	 * @return Formats this type for DDR interactive, short version. E.g. u8: 0xFF (255)
	 */
	public String formatShortInteractive()
	{
		StringBuilder builder = new StringBuilder();
		
		builder.append(this.getClass().getSimpleName().toLowerCase());
		builder.append(": 0x");
		try {
			builder.append(Long.toHexString(this.longValue()));
		} catch (CorruptDataException e) {
			builder.append("FAULT");
		} catch (InvalidDataTypeException e) {
			builder.append("<FAULT : " + e.getMessage() + ">");
		}
		
		builder.append(" (");
		try {
			builder.append(this.longValue());
		} catch (CorruptDataException e) {
			builder.append("FAULT");
		}
		builder.append(")");
		
		return builder.toString();
	}
	
	public static String getPointerPackageName() 
	{
		return pointerPackageName;
	}
	
	public static String getStructurePackageName() 
	{
		return structurePackageName;
	}
}
