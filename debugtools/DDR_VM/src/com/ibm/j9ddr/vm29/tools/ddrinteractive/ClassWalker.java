/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import java.util.HashMap;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.StructureCommandUtil;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.IClassWalkCallbacks.SlotType;

public abstract class ClassWalker {
	/**
	 * Walks every slots in an object and send the values to the classWalker
	 *
	 * @param classWalker
	 *            a class that will receive the slot and sections of the object
	 * @throws CorruptDataException
	 */
	public abstract void allSlotsInObjectDo(IClassWalkCallbacks classWalker)
			throws CorruptDataException;
	public abstract Context getContext();

	protected HashMap<String, String> debugExtMap = new HashMap<String, String>();
	protected StructurePointer clazz;
	protected IClassWalkCallbacks classWalkerCallback;

	protected void fillDebugExtMap() {
	}

	public StructurePointer getClazz() {
		return clazz;
	}

	public IClassWalkCallbacks getClassWalkerCallbacks() {
		return classWalkerCallback;
	}

	protected String getDebugExtForMethodName(String methodName) {
		return debugExtMap.get(methodName);
	}

	/**
	 * It walks through each field in this structure that is represented by methodClass and
	 * registers each field as a slot into ClassWalkerCallBack
	 *
	 * It uses the StructureDescriptor of J9DDR to get the FieldDescriptor of a structure.
	 * The name, type and address are found in the FieldDescriptor. The name, type and address
	 * are guaranteed to match the current core file, since they are taken from it.
	 *
	 * @param methodClass pointer class that is generated for the VM structure
	 * @param renameFields	list of names to rename the original field names in the structure
	 * @throws CorruptDataException
	 */
	protected void addObjectsAsSlot(StructurePointer methodClass, HashMap<String, String> renameFields) throws CorruptDataException {
		/* Get the structure name by removing "Pointer" suffix from the DDR generated class name*/
		String structureName = methodClass.getClass().getSimpleName().substring(0, methodClass.getClass().getSimpleName().indexOf("Pointer"));
		/* Get structure descriptor by using structure name */
		StructureDescriptor sd = StructureCommandUtil.getStructureDescriptor(structureName, getContext());
		/* Structure descriptor can not be null in normal circumstances,
		 * because StructurePointer "methodClass" is generated for an existing structure in the VM,
		 * so it should exist. If not, throw an exception.
		 */
		if (sd == null) {
			throw new CorruptDataException("Structure \"" + structureName + "\" can not be found.");
		}

		for ( FieldDescriptor fd : sd.getFields() ) {
			/* Get the name of the field from field descriptor */
			String outName = fd.getName();
			/* Get SlotType by using field type name */
			SlotType type = getTypeByFieldTypeName(fd.getType());

			/* Get the address of the field by adding the offset to the methodClass's address */
			AbstractPointer address = U8Pointer.cast(methodClass).addOffset(fd.getOffset());

			/* Rename fields if any defined. */
			if (null != renameFields) {
				if (renameFields.containsKey(outName)) {
					outName = renameFields.get(outName);
				}
			}

			/*add the field into classWalkerCallback by its name, type, address and debug extension method name */
			classWalkerCallback.addSlot(clazz, type, address, outName, getDebugExtForMethodName(outName));
		}
	}

	protected void addObjectsasSlot(StructurePointer methodClass) throws CorruptDataException {
		addObjectsAsSlot(methodClass, null);
	}

	/**
	 * Gets the SlotType value by using field's type name.
	 * Field's type name is identical to how it looks in VM structures unless its type is overwritten by DDR properties file.
	 * Example :
	 * 		struct  Test{
	 * 			J9SRP field1;
	 * 			J9SRP field2;
	 * 		}
	 *
	 * 		//overwrite field1's type in ddr .properties files
	 * 		ddrblob.typeoverride.Test.field1=J9SRP(struct J9UTF8)
	 *
	 * fieldTypeName values for:
	 * 		field1, it is "J9SRP(struct J9UTF8)"
	 * 		field2, it is "J9SRP"
	 *
	 *
	 * @param fieldTypeName
	 * @return SlotType
	 * @throws CorruptDataException if the passed fieldTypeName is not recognized by any if statements below.
	 */
	private static SlotType getTypeByFieldTypeName(String fieldTypeName) throws CorruptDataException {
		/*
		 * Remove any 'struct' or 'class' tags in the type name.
		 */
		String typeName = TypeTagPattern.matcher(fieldTypeName).replaceAll("");

		/*
		 * J9WSRP (possibly overridden by DDR properties files)
		 * J9WSRP field1
		 * J9WSRP(J9ROMClass) field2
		 * J9WSRP(U32) field3
		 */
		if (typeName.equals("J9WSRP") || typeName.startsWith("J9WSRP(")) {
			return SlotType.J9_WSRP;
		}

		/*
		 * J9SRP (possibly overridden by DDR properties files)
		 * J9SRP field1
		 * J9SRP(J9ROMClass) field2
		 * J9SRP(U32) field3
		 */
		if (typeName.equals("J9SRP") || typeName.startsWith("J9SRP(")) {
			return typeName.startsWith("J9SRP(J9UTF8") ? SlotType.J9_SRP_TO_STRING : SlotType.J9_SRP;
		}

		if (typeName.equals("J9UTF8*")) {
			return SlotType.J9_ROM_UTF8;
		}
		if (typeName.equals("U8")) {
			return SlotType.J9_U8;
		}
		if (typeName.equals("I8")) {
			return SlotType.J9_I8;
		}
		if (typeName.equals("U16")) {
			return SlotType.J9_U16;
		}
		if (typeName.equals("I16")) {
			return SlotType.J9_I16;
		}
		if (typeName.equals("U32")) {
			return SlotType.J9_U32;
		}
		if (typeName.equals("I32")) {
			return SlotType.J9_I32;
		}
		if (typeName.equals("U64")) {
			return SlotType.J9_U64;
		}
		if (typeName.equals("I64")) {
			return SlotType.J9_I64;
		}
		if (typeName.equals("UDATA")) {
			return SlotType.J9_UDATA;
		}
		if (typeName.equals("IDATA")) {
			return SlotType.J9_IDATA;
		}
		if (typeName.equals("J9ROMNameAndSignature")) {
			return SlotType.J9_NAS;
		}
		/* if type name ends with an asterisk, it is a pointer type */
		if (typeName.endsWith("*")) {
			return SlotType.J9_UDATA;
		}
		/* throw exception if a new field type name is added to VM which is not recognized by any if block above */
		throw new CorruptDataException("Field type name '" + fieldTypeName + "' is not recognized.");
	}

	private static final Pattern TypeTagPattern = Pattern.compile("\\s*\\b(class|struct)\\s+");

}
