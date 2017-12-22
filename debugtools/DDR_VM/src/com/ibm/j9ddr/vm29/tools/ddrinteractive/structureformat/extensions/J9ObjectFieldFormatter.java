/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.extensions;

import static com.ibm.j9ddr.StructureTypeManager.TYPE_STRUCTURE_POINTER;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.StructureCommandUtil;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

/**
 * Field formatter that appends the class of !j9object fields
 */
public class J9ObjectFieldFormatter extends BaseFieldFormatter 
{

	@Override
	public FormatWalkResult postFormat(String name, String type, String declaredType,
			int typeCode, long address, PrintStream out,
			Context context, IStructureFormatter structureFormatter) throws CorruptDataException 
	{
		if (typeCode == TYPE_STRUCTURE_POINTER && StructureCommandUtil.typeToCommand(type).equals("!j9object")) {
		
			PointerPointer slotPtr = PointerPointer.cast(address);
			
			J9ObjectPointer object = J9ObjectPointer.cast(slotPtr.at(0));
			
			if (object.isNull()) {
				return FormatWalkResult.KEEP_WALKING;
			}
			
			J9ClassPointer clazz = J9ObjectHelper.clazz(object);
			
			out.print(" // ");
			
			if (J9ClassHelper.isArrayClass(clazz)) {
				out.print(J9ClassHelper.getArrayName(clazz));
			} else {
				out.print(J9ClassHelper.getName(clazz));
			}
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}
}
