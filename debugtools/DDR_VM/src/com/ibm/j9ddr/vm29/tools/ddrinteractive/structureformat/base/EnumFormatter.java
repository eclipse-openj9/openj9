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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.structureformat.base;

import static com.ibm.j9ddr.StructureTypeManager.TYPE_ENUM;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.StructureCommandUtil;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;

public class EnumFormatter extends BaseFieldFormatter
{
	@Override
	public FormatWalkResult format(String name, String type, String declaredType,
			int typeCode, long address, PrintStream out,
			Context context, IStructureFormatter structureFormatter) throws CorruptDataException 
	{
		if (typeCode != TYPE_ENUM) {
			return FormatWalkResult.KEEP_WALKING;
		}
		
		//Get a handle on the descriptor for the enum - to find out how big it is
		type = type.replace("enum", "");
		type = type.trim();
		
		StructureDescriptor desc = StructureCommandUtil.getStructureDescriptor(type, context);
		
		if (null == desc) {
			out.print("<<Missing description of " + type + ">>");
			return FormatWalkResult.STOP_WALKING;
		}
		
		long value;
		
		switch (desc.getSizeOf()) {
		case 1:
			value = U8Pointer.cast(address).at(0).longValue();
			break;
		case 2:
			value = U16Pointer.cast(address).at(0).longValue();
			break;
		case 4:
			value = U32Pointer.cast(address).at(0).longValue();
			break;
		case 8:
			value = U64Pointer.cast(address).at(0).longValue();
			break;
		default:
			out.print("<<Unhandled enum size: " + desc.getSizeOf() + ">>");
			return FormatWalkResult.STOP_WALKING;
		}
		
		out.print("0x" + Long.toHexString(value));
		out.print(" (");
		out.print(value);
		out.print(")");
		
		boolean mnemonicWritten = false;
		for (ConstantDescriptor constant : desc.getConstants()) {
			if (constant.getValue() == value) {
				out.print(" //");
				out.print(constant.getName());
				mnemonicWritten = true;
				break;
			}
		}
		
		if (! mnemonicWritten) {
			out.print(" <<Not matched to enum constant>>");
		}
		
		return FormatWalkResult.STOP_WALKING;
	}
}
