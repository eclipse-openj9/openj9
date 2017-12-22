/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;

/**
 * Structure formatter that adds the signature and j9rommethod pointer as a suffix
 * after a !j9method structure is formatted.
 *
 */
public class J9MethodStructureFormatter extends BaseStructureFormatter 
{

	@Override
	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9method")) {
			J9MethodPointer method = J9MethodPointer.cast(address);
			
			if (! method.isNull()) {
				writeMethodName(method, out);
				writeJ9ROMClassAddress(method, out);
				writeNextMethodAddress(method, out);
			}
			
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}

	private static void writeJ9ROMClassAddress(J9MethodPointer method, PrintStream out) 
	{
		out.print("ROM Method: ");
		try {
			J9ROMMethodPointer j9romMethod = J9MethodHelper.romMethod(method);
			
			out.print("!j9rommethod ");
			out.print(j9romMethod.getHexAddress());
		} catch (CorruptDataException e) {
			out.print("<FAULT>");
		} finally {
			out.println();
		}
	}

	private static void writeMethodName(J9MethodPointer method, PrintStream out) 
	{
		out.print("Signature: ");
		try {
			J9ROMMethodPointer j9romMethod = J9MethodHelper.romMethod(method);
			
			J9ClassPointer clazz = ConstantPoolHelpers.J9_CLASS_FROM_METHOD(method);
			
			
			out.print(J9ClassHelper.getName(clazz));
			out.print(".");
			out.print(J9ROMMethodHelper.getName(j9romMethod));
			out.print(J9ROMMethodHelper.getSignature(j9romMethod));
			
			out.print(" !bytecodes ");
			out.print(method.getHexAddress());
		} catch (CorruptDataException ex) {
			out.print("<FAULT>");
		} finally {
			out.println();
		}
	}
	
	private static void writeNextMethodAddress(J9MethodPointer method, PrintStream out) 
	{
		out.print("Next Method: !j9method ");
		try {
			out.print(J9MethodHelper.nextMethod(method).getHexAddress());
		} catch (CorruptDataException ex) {
			out.print("<FAULT>");
		} finally {
			out.println();
		}
	}
	
}
