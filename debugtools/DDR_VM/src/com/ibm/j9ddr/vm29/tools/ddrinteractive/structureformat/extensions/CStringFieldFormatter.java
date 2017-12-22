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

import java.io.PrintStream;

import com.ibm.j9ddr.CTypeParser;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureTypeManager;
import com.ibm.j9ddr.tools.ddrinteractive.BaseFieldFormatter;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.FormatWalkResult;
import com.ibm.j9ddr.tools.ddrinteractive.IStructureFormatter;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;

/**
 * Field formatter that adds the value of any char * string fields
 *
 */
public class CStringFieldFormatter extends BaseFieldFormatter 
{

	public static final int MAXIMUM_LENGTH = 50;
	
	@Override
	public FormatWalkResult postFormat(String name, String type,
			String declaredType, int typeCode, long address, PrintStream out,
			Context context, IStructureFormatter structureFormatter)
			throws CorruptDataException 
	{
		if (typeCode == StructureTypeManager.TYPE_POINTER) {
			CTypeParser parser = new CTypeParser(declaredType);
			
			if (parser.getCoreType().equals("char") && parser.getSuffix().equals("*")) {
				U8Pointer ptr = U8Pointer.cast(PointerPointer.cast(address).at(0));
				
				if (ptr.isNull()) {
					return FormatWalkResult.KEEP_WALKING;
				}
				
				String str = ptr.getCStringAtOffset(0, MAXIMUM_LENGTH);
				
				if (str.length() > 0) {
					out.print(" // \"");
					out.print(str);
					if (str.length() >= MAXIMUM_LENGTH) {
						out.print("...");
					}
					out.print("\"");
				}
			}
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}
}
