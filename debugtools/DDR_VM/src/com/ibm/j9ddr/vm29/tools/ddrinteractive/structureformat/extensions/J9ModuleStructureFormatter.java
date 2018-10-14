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
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

/**
 * Structure Formatter that adds a suffix to the command "!j9module $moduleAddress$" output
 * 
 * Example output (suffix part):
 * Module name: java.instrument
 * To display all j9packages exported by a module, use !dumpmoduleexports 0x000001305F46DB38
 * To display all modules that the target module reads, use !dumpmodulereads 0x000001305F46DB38
 * To find all modules that read a module, use !findallreads 0x000001305F46DB38
 * To find all loaded classes in a module, use !dumpallclassesinmodule 0x000001305F46DB38
 * 
 */
public class J9ModuleStructureFormatter extends BaseStructureFormatter 
{

	@Override
	public FormatWalkResult postFormat(String type, long address,
			PrintStream out, Context context,
			List<IFieldFormatter> fieldFormatters, String[] extraArgs) 
	{
		if (type.equalsIgnoreCase("j9module") && address != 0) {
			J9ModulePointer modulePtr = J9ModulePointer.cast(address);
			String moduleAddress = modulePtr.getHexAddress();
			try {
				out.println("Module name: " + J9ObjectHelper.stringValue(modulePtr.moduleName()));
			} catch (CorruptDataException e) {
				// Do nothing
			}
			out.println("To display all j9packages exported by a module, use !dumpmoduleexports " + moduleAddress);
			out.println("To display all modules that the target module reads, use !dumpmodulereads " + moduleAddress);
			out.println("To find all modules that read a module, use !findallreads " + moduleAddress);
			out.println("To find all loaded classes in a module, use !dumpallclassesinmodule " + moduleAddress);
		}

		return FormatWalkResult.KEEP_WALKING;
	}

}
