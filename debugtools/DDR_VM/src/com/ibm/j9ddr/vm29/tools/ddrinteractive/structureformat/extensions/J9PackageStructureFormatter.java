/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

/**
 * Structure Formatter that adds a suffix like this:
 * 
 * Package name: jdk/internal/perf
 * To dump all modules that a package is exported to, use !dumpmoduleexports 0x00000130550DB338
 * 
 * When a !j9package is formatted
 */
public class J9PackageStructureFormatter extends BaseStructureFormatter 
{

	@Override
	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9package") && address != 0) {
			J9PackagePointer packagePtr = J9PackagePointer.cast(address);

			try {
				out.println("Package name: " + J9UTF8Helper.stringValue(packagePtr.packageName()));
			} catch (CorruptDataException e) {
				// Do nothing
			}
			out.println("To dump all modules that a package is exported to, use !dumpmoduleexports "
					+ packagePtr.getHexAddress());
		}
		
		return FormatWalkResult.KEEP_WALKING;
	}

	
}
