/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.BaseStructureFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IFieldFormatter;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;


public class J9ROMMethodStructureFormatter extends BaseStructureFormatter {

	@Override
	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9rommethod")) {
			J9ROMMethodPointer method = J9ROMMethodPointer.cast(address);
			
			if (! method.isNull()) {
				writeMethodName(method, out);
				writeNextROMMethodAddress(method, out);
			}
			
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}

	private void writeNextROMMethodAddress(J9ROMMethodPointer romMethod, PrintStream out) {
		out.print("Next ROM Method: !j9rommethod ");
		try {
			out.print("0x" + Long.toHexString(ROMHelp.nextROMMethod(romMethod).getAddress()));
		} catch (CorruptDataException ex) {
			out.print("<FAULT>");
		} finally {
			out.println();
		}
	}

	private void writeMethodName(J9ROMMethodPointer romMethod, PrintStream out) {
		out.print("Signature: ");
		try {
			out.print(J9ROMMethodHelper.getName(romMethod));
			out.print(J9ROMMethodHelper.getSignature(romMethod));
		} catch (CorruptDataException ex) {
			out.print("<FAULT>");
		} finally {
			out.println();
		}
	}
	
}
