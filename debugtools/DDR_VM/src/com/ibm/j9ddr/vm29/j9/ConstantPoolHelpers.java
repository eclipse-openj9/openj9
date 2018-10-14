/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STARTPC_STATUS;

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTION_MASK;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/* This class takes the place of oti/j9cp.h */
public class ConstantPoolHelpers
{
	
	private static J9ObjectFieldOffset vmRefOffset = null;
	
	private ConstantPoolHelpers() {}
	
	public static UDATAPointer J9_AFTER_CLASS(J9ClassPointer clazz)
	{
		return UDATAPointer.cast(clazz.add(1));
	}
	
	public static J9ConstantPoolPointer J9_CP_FROM_METHOD(J9MethodPointer method) throws CorruptDataException
	{
		return J9ConstantPoolPointer.cast(UDATA.cast(method.constantPool())).untag(J9_STARTPC_STATUS);
	}
	
	/**
	 *  use J9ClassPointer.ramConstantPool() instead? 
	 */
	public static J9ConstantPoolPointer J9_CP_FROM_CLASS(J9ClassPointer clazz) throws CorruptDataException
	{
		return J9ConstantPoolPointer.cast(clazz.ramMethods().add(clazz.romClass().romMethodCount()));
	}
	
	/**
	 *  use J9ConstantPoolPointer.ramClass() instead? 
	 */
	public static J9ClassPointer J9_CLASS_FROM_CP(J9ConstantPoolPointer cp) throws CorruptDataException
	{
		return cp.ramClass();
	}
	
	public static J9ClassPointer J9_CLASS_FROM_METHOD(J9MethodPointer method) throws CorruptDataException
	{
		return J9_CLASS_FROM_CP(J9_CP_FROM_METHOD(method));
	}
	
	/**
	 *  use J9ConstantPoolPointer.romConstantPool() instead? 
	 */
	public static J9ROMConstantPoolItemPointer J9_ROM_CP_FROM_CP(J9ConstantPoolPointer cp) throws CorruptDataException
	{
		return cp.romConstantPool();
	}
	
	/**
	 *  use J9ROMClassPointer.romConstantPool() instead? 
	 */
	public static J9ROMConstantPoolItemPointer J9_ROM_CP_FROM_ROM_CLASS(J9ROMClassPointer romClass) throws CorruptDataException
	{
		return J9ROMConstantPoolItemPointer.cast(romClass.add(1));
	}
	
	public static boolean J9_IS_CLASS_OBSOLETE(J9ClassPointer clazz) throws CorruptDataException
	{
		if(J9BuildFlags.interp_hotCodeReplacement) {
			return J9ClassHelper.isSwappedOut(clazz);
		} else {
			return false;
		}
	}
	
	public static J9ClassPointer J9_CURRENT_CLASS(J9ClassPointer clazz) throws CorruptDataException
	{
		if(J9_IS_CLASS_OBSOLETE(clazz)) {
			return clazz.arrayClass();
		} else {
			return clazz;
		}
	}
	
	public static J9ClassPointer J9VM_J9CLASS_FROM_HEAPCLASS(J9ObjectPointer clazzObject) throws CorruptDataException
	{
		if(clazzObject.isNull()) {
			return J9ClassPointer.NULL;
		}
		if( vmRefOffset == null ) {
			Iterator fieldIterator = J9ClassHelper.getFieldOffsets(J9ObjectHelper.clazz(clazzObject));
			while( fieldIterator.hasNext() ) {
				Object nextField = fieldIterator.next();
				if( nextField instanceof J9ObjectFieldOffset ) {
					J9ObjectFieldOffset offset = (J9ObjectFieldOffset) nextField;
					if( "vmRef".equals(offset.getName())) {
						vmRefOffset = offset;
						break;
					}
				}
			}
		}
		if( vmRefOffset != null ) {
			long address = J9ObjectHelper.getLongField(clazzObject, vmRefOffset);
			return J9ClassPointer.cast(address);
		} else {
			throw new CorruptDataException("Unable to find field offset for the 'vmRef' field");
		}
	}
	
	public static J9ObjectPointer J9VM_J9CLASS_TO_HEAPCLASS(J9ClassPointer clazz) throws CorruptDataException
	{
		if(clazz.isNull()) {
			return J9ObjectPointer.NULL;
		}
		return J9ObjectPointer.cast(clazz.classObject());
	}	
	
	/**
	 * This method is Java implementation of the define J9_CP_TYPE in j9cp.h in VM. 
	 * It basically find out the type of the constant pool entry at the given index.  
	 * 
	 * #define J9_CP_TYPE(cpShapeDescription, index) \
	 *	(((cpShapeDescription)[(index) / J9_CP_DESCRIPTIONS_PER_U32] >> \
	 *	(((index) % J9_CP_DESCRIPTIONS_PER_U32) * J9_CP_BITS_PER_DESCRIPTION)) & J9_CP_DESCRIPTION_MASK)
	 *
	 * @param cpShapeDescription Description of the constantPool
	 * @param index Type index
	 * @return Type
	 * @throws CorruptDataException
	 */
	public static long J9_CP_TYPE(U32Pointer cpShapeDescription, int index) throws CorruptDataException 
	{
		U32 cpDescription = cpShapeDescription.at(index/J9_CP_DESCRIPTIONS_PER_U32);
		long shapeDesc = cpDescription.rightShift((int)((index%J9_CP_DESCRIPTIONS_PER_U32)*J9_CP_BITS_PER_DESCRIPTION)).bitAnd(J9_CP_DESCRIPTION_MASK).longValue();
		return shapeDesc;
	}
}
