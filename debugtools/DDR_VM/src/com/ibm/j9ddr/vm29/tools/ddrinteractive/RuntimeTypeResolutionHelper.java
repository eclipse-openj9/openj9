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
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.StructureCommandUtil;
import com.ibm.j9ddr.util.RuntimeTypeResolutionUtils;
import com.ibm.j9ddr.vm29.pointer.Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;

public class RuntimeTypeResolutionHelper
{
	
	// Design 42819
	// Discover runtime type based on identifier field.
	public static String findRuntimeType(String type, Pointer ptr, Context context)
	{	
		StructureDescriptor fieldOwner = null;
		FieldDescriptor typeIdField = null;
		String classType = null;	
		
		// If it *is* a class or structure (vs. for example simple char*, etc.)
		// n.b. The magic "_typeId" field may be in any of the superclasses,
		// not just as the top of the hierarchy, so we have to look at each level.
		try {
			if (ptr.notNull()) {
				classType = StructureCommandUtil.typeToCommand(type).substring(1); // Skip the "!"
				do {
					fieldOwner = StructureCommandUtil.getStructureDescriptor(classType, context);
					if (null != fieldOwner) {	
						for (FieldDescriptor aField : fieldOwner.getFields()) {
							if (aField.getDeclaredName().equals("_typeId")) {
								typeIdField = aField;
								break;
							}
						}
						if (null == typeIdField) {
							classType = fieldOwner.getSuperName();
						}
					}
				} while (
						(null == typeIdField)
						&& (null != classType) 
						&& (null != fieldOwner)
						&& (classType.length() > 0));
			}
		
			if (null != typeIdField) {
				VoidPointer untypedStrPtr = PointerPointer.cast(ptr).addOffset(typeIdField.getOffset()).at(0);
				if (untypedStrPtr.notNull()) {
					U8Pointer typeStrPtr = U8Pointer.cast(untypedStrPtr);
					type = typeStrPtr.getCStringAtOffset(0).toLowerCase();
				}
			}

		} catch (CorruptDataException e) {
			// Do nothing.
		}
		return RuntimeTypeResolutionUtils.cleanTypeStr(type);
	}
	
}
