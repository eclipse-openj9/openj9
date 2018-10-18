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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ClassIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ClassOutput;

public class DumpPackageCommand extends Command 
{
	enum Subcommand {
		CLASS, ALL, PRINT_PACKAGE, HELP, INVALID
	}

	public DumpPackageCommand() 
	{
		addCommand("dumppackage", "[all|exportsTo|classes] <packageAddress>|help", "List details about a package");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		final Subcommand subcommand;
		String filterArg = null;
		ClassOutput classOutputter = null;
		ClassIteratorFilter classFilter = null;
		switch (args.length) {
		case 1:
			switch (args[0]) {
			case "help":
				subcommand = Subcommand.HELP;
				break;
			default:
				filterArg = args[0];
				subcommand = Subcommand.ALL;
				break;
			}
			break;
		case 2:
			filterArg = args[1];
			switch (args[0]) {
			case "all":
				subcommand = Subcommand.ALL;
				break;
			case "exportsTo":
				subcommand = Subcommand.PRINT_PACKAGE;
				break;
			case "classes":
				classOutputter = ModularityHelper::printJ9Class;
				classFilter = DumpPackageCommand::filterClassByPackage;
				subcommand = Subcommand.CLASS;
				break;
			default:
				subcommand = Subcommand.INVALID;
				break;
			}
			break;
		default:
			subcommand = Subcommand.INVALID;
			break;
		}
		try {
			J9PackagePointer packagePtr = null;
			if (null != filterArg) {
				try {
					packagePtr = J9PackagePointer.cast(Long.decode(filterArg));
				} catch (NumberFormatException e) {
					throw new DDRInteractiveCommandException("The argument \"" + filterArg + "\" is not a valid number. It should be the address of a J9Package.");
				}
			}
			int result = 0;
			switch (subcommand) {
			case ALL:
				out.println("Exported to:");
				ModularityHelper.printPackageExports(packagePtr, out);
				break;
			case PRINT_PACKAGE:
				ModularityHelper.printPackageExports(packagePtr, out);
				break;
			case CLASS:
				J9ClassLoaderPointer classLoaderPtr = packagePtr.classLoader();
				result = ModularityHelper.iterateClassLoaderClasses(out, classFilter, classOutputter, filterArg, classLoaderPtr);
				out.printf("Found %d class%s%n", result, (1 == result ? "": "es"));
				break;
			case HELP:
				printHelp(out);
				break;
			default:
				out.println("Argument failed to parse or was parsed to an unhandled subcommand.");
				printHelp(out);
				break;
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	private static boolean filterClassByPackage(J9ClassPointer classPtr, String targetModuleAddress) throws CorruptDataException {
		boolean result = false;
		String className = J9ClassHelper.getJavaName(classPtr);
		int index = className.lastIndexOf('/');
		if (index >= 0) {
			String classPackage = className.substring(0, index);
			J9PackagePointer packagePtr = J9PackagePointer.cast(Long.decode(targetModuleAddress));
			String packageName = J9UTF8Helper.stringValue(packagePtr.packageName());
			result = classPackage.equals(packageName);
		}
		return result;
	}

	void printHelp(PrintStream out) {
		out.println("Usage:");
		out.println("  !dumppackage <packageAddress>");
		out.println("      Lists !dumppackage all <packageAddress>");
		out.println("  !dumppackage all <packageAddress>");
		out.println("      Lists !dumppackage exportsTo <packageAddress>");
		out.println("  !dumppackage exportsTo <packageAddress>");
		out.println("      Lists all modules that the given package is exported to");
		out.println("  !dumppackage classes <packageAddress>");
		out.println("      Lists all loaded classes in the given package");
	}
}
