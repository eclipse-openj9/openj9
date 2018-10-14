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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;

/**
 * DumpModuleExports command dumps all modules that the package is exported to
 * 
 * Example:
 *    !dumpallclassesinmodule 0x000001305F471528
 * Example output: 
 *    com/ibm/jvm/Trace    !j9class 0x000000000015A800
 *    com/ibm/jvm/TracePermission    !j9class 0x000000000015AA00
 *    Found 2 loaded classes in !j9module 0x000001305F471528
 */
public class DumpAllClassesInModuleCommand extends Command
{

	public DumpAllClassesInModuleCommand()
	{
		addCommand("dumpallclassesinmodule", "<moduleAddress>", "dump all loaded classes in the target module");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length != 1) {
			CommandUtils.dbgPrint(out, "Usage: !dumpallclassesinmodule <moduleAddress>\n");
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
				String targetModuleAddress = args[0];
				J9ModulePointer modulePtr = J9ModulePointer.cast(Long.decode(targetModuleAddress));
				J9ClassLoaderPointer classLoaderPtr = modulePtr.classLoader();
				Iterator<J9ClassPointer> iterator = ClassIterator.fromJ9Classloader(classLoaderPtr);
				J9ClassPointer classPtr = null;

				int hitCount = 0;
				PatternString pattern = new PatternString(targetModuleAddress);

				while (iterator.hasNext()) {
					classPtr = iterator.next();
					String moduleAddr = classPtr.module().getHexAddress();
					if (pattern.isMatch(moduleAddr)) {
						String className = J9ClassHelper.getName(classPtr);
						String classAddress = classPtr.getHexAddress();
						hitCount++;
						out.printf("%-30s !j9class %s%n", className, classAddress);
					}
				}

				out.printf("Found %d loaded classes in !j9module %s\n", hitCount, targetModuleAddress);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
