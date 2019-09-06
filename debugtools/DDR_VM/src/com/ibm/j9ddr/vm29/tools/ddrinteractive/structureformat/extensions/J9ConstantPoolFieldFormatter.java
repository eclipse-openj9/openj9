/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import java.io.PrintStream;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureTypeManager;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.vm29.pointer.Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.structure.J9Consts;

/**
 * Formats fields which are J9ConstantPool pointers,
 * which require special handling of the low-order bits.
 */
public final class J9ConstantPoolFieldFormatter extends BaseFieldFormatter {

	private static final Pattern TypePattern = Pattern.compile("(struct\\s+)?J9ConstantPool\\s*\\*");

	public FormatWalkResult format(String name, String type, String declaredType, int typeCode, long address,
			PrintStream out, Context context, IStructureFormatter structureFormatter) throws CorruptDataException {
		if (StructureTypeManager.TYPE_STRUCTURE_POINTER == typeCode && TypePattern.matcher(type).matches()) {
			Pointer ptr = PointerPointer.cast(address).at(0);
			J9ConstantPoolPointer cpPtr = J9ConstantPoolPointer.cast(ptr).untag(J9Consts.J9_STARTPC_STATUS);
			long flags = ptr.getAddress() & J9Consts.J9_STARTPC_STATUS;

			out.print("!j9constantpool ");
			out.print(cpPtr.getHexAddress());
			out.print(" (flags = 0x");
			out.print(Long.toHexString(flags).toUpperCase());
			out.print(")");

			return FormatWalkResult.STOP_WALKING;
		}

		return FormatWalkResult.KEEP_WALKING;
	}

}
