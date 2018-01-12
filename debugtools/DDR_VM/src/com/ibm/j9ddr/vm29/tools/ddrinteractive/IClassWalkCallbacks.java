/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I64;
import com.ibm.j9ddr.vm29.types.I8;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.UDATA;

public interface IClassWalkCallbacks {
	public enum SlotType { 
		J9_U8(U8.SIZEOF),
		J9_I8(I8.SIZEOF), 
		J9_I16(I16.SIZEOF), 
		J9_U16(U16.SIZEOF), 
		J9_U32(U32.SIZEOF), 
		J9_I32(I32.SIZEOF), 
		J9_U64(U64.SIZEOF),
		J9_I64(I64.SIZEOF),
		J9_RAM_UTF8(UDATA.SIZEOF),
		J9_ROM_UTF8(U32.SIZEOF),
		J9_UTF8(U32.SIZEOF),
		J9_NAS(I32.SIZEOF), /* Prints the name and signature */
		J9_SRPNAS(I32.SIZEOF),
		J9_UDATA(UDATA.SIZEOF),
		J9_IDATA(IDATA.SIZEOF),
		J9_SRP(I32.SIZEOF), /* Prints the value where a SRP points to */
		J9_SRP_TO_STRING(I32.SIZEOF), /* It is an SRP, but prints the String it points to */
		J9_WSRP(UDATA.SIZEOF),
		J9_iTableMethod(UDATA.SIZEOF),
		J9_IntermediateClassData(0),
		/* New SlotTypes needs to be added before this line to be properly nested */
		J9_SECTION_START(UDATA.SIZEOF),
		J9_SECTION_END(UDATA.SIZEOF),
		J9_Padding(0);
		
		private final int size;
		SlotType(int size) {
			this.size = size;
		}
		public int getSize() {
			return size;
		}
	}
	
	/**
	 * The information of one slot is sent back.
	 * 
	 * @param clazz
	 *            the object walked
	 * @param type
	 *            the type of the slot, it is a
	 *            com.ibm.j9ddr.vm29.tools.ddrinteractive.Type
	 * @param slotPtr
	 *            an AbstractPointer of the slot currently walked
	 * @param slotName
	 * @param additionalInfo
	 * @throws CorruptDataException
	 */
	public void addSlot(StructurePointer clazz, SlotType type, AbstractPointer slotPtr, String slotName, String additionalInfo) throws CorruptDataException;

	/**
	 * The information of one slot is sent back.
	 * 
	 * @param clazz
	 *            the object walked
	 * @param type
	 *            the type of the slot, it is a
	 *            com.ibm.j9ddr.vm29.tools.ddrinteractive.Type
	 * @param slotPtr
	 *            an AbstractPointer of the slot currently walked
	 * @param slotName
	 * @throws CorruptDataException
	 */
	public void addSlot(StructurePointer clazz, SlotType type, AbstractPointer slotPtr, String slotName) throws CorruptDataException;

	/**
	 * It is a high level section of an object walked. If two sections have the
	 * same address, they will be nested
	 * 
	 * @param clazz
	 *            the object walked
	 * @param address
	 *            starting address of the section
	 * @param length
	 *            length of the section
	 * @param name
	 * @param computePadding
	 *            If true, compute the padding before and after this section
	 * @throws CorruptDataException
	 */
	public void addSection(StructurePointer clazz, AbstractPointer address, long length, String name, boolean computePadding) throws CorruptDataException;
}
