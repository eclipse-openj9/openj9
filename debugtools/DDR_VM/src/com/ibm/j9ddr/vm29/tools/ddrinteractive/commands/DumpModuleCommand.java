/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.ModuleHashTable;
import com.ibm.j9ddr.vm29.j9.PackageHashTable;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ClassIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ClassOutput;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ModuleIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ModuleOutput;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.PackageIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.PackageOutput;

public class DumpModuleCommand extends Command 
{
	enum Subcommand {
		CLASS, MODULE, PACKAGE, ALL, HELP, INVALID
	}

	public DumpModuleCommand()
	{
		addCommand("dumpmodule", "[all|requires|exports|classes|packages] <moduleAddress>|help", "List details about a module");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		final Subcommand subcommand;
		ClassIteratorFilter classFilter = null;
		ClassOutput classOutput = null;
		ModuleIteratorFilter moduleFilter = null;
		ModuleOutput moduleOutputter = null;
		PackageIteratorFilter packageFilter = null;
		PackageOutput packageOutputter = null;
		String filterArg = null;
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
			case "requires":
				moduleFilter = DumpModuleCommand::filterModuleRequired;
				moduleOutputter = ModularityHelper::printJ9Module;
				subcommand = Subcommand.MODULE;
				break;
			case "exports":
				packageFilter = DumpModuleCommand::filterPackageModuleAndExport;
				packageOutputter = ModularityHelper::printPackageExportVerbose;
				subcommand = Subcommand.PACKAGE;
				break;
			case "classes":
				classFilter = DumpModuleCommand::filterClassByModule;
				classOutput = ModularityHelper::printJ9Class;
				subcommand = Subcommand.CLASS;
				break;
			case "packages":
				packageFilter = DumpModuleCommand::filterPackageModule;
				packageOutputter = ModularityHelper::printJ9Package;
				subcommand = Subcommand.PACKAGE;
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
			int result;
			J9ModulePointer modulePtr = null;
			J9ClassLoaderPointer classLoaderPtr = null;
			if (null != filterArg) {
				try {
					modulePtr = J9ModulePointer.cast(Long.decode(filterArg));
					classLoaderPtr = modulePtr.classLoader();
				} catch (NumberFormatException e) {
					throw new DDRInteractiveCommandException("The argument \"" + filterArg + "\" is not a valid number. It should be the address of a J9Module.");
				} catch (NullPointerException e) {
					throw new DDRInteractiveCommandException("The argument \"" + filterArg + "\" is not the address of a valid J9Module.");
				} catch (MemoryFault e) {
					System.out.println(e.getMessage());
					throw new DDRInteractiveCommandException("The argument \"" + filterArg + "\" is not the address of a valid J9Module.");
				}
			}
			switch (subcommand) {
			case CLASS:
				result = ModularityHelper.iterateClassLoaderClasses(out, classFilter, classOutput, filterArg, classLoaderPtr);
				System.out.printf("Found %d class%s%n", result, (1 == result ? "" : "es"));
				break;
			case MODULE:
				result = ModularityHelper.iterateModules(out, moduleFilter, moduleOutputter, filterArg);
				System.out.printf("Found %d module%s%n", result, (1 == result ? "" : "s"));
				break;
			case PACKAGE:
				result = ModularityHelper.iterateClassLoaderPackages(out, packageFilter, packageOutputter, filterArg, classLoaderPtr);
				System.out.printf("Found %d package%s%n", result, (1 == result ? "" : "s"));
				break;
			case ALL:
				out.println("Module:");
				ModularityHelper.printJ9Module(modulePtr, out);
				/* Run requires */
				out.println("Requires:");
				moduleFilter = DumpModuleCommand::filterModuleRequired;
				moduleOutputter = ModularityHelper::printJ9Module;
				result = ModularityHelper.iterateModules(out, moduleFilter, moduleOutputter, filterArg);
				System.out.printf("Found %d required module%s%n", result, (1 == result ? "" : "s"));
				/* Run exports */
				out.println("Exports:");
				packageFilter = DumpModuleCommand::filterPackageModuleAndExport;
				packageOutputter = ModularityHelper::printPackageExportVerbose;
				result = ModularityHelper.iteratePackages(out, packageFilter, packageOutputter, filterArg);
				System.out.printf("Found %d exported package%s%n", result, (1 == result ? "" : "s"));
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

	private static boolean filterClassByModule(J9ClassPointer classPtr, String targetModuleAddress) throws CorruptDataException {
		String moduleAddress = classPtr.module().getHexAddress();
		return targetModuleAddress.equalsIgnoreCase(moduleAddress);
	}

	private static boolean filterModuleRequired(J9ModulePointer modulePtr, String targetModuleAddress) throws CorruptDataException {
		boolean result = false;
		J9HashTablePointer readTable = modulePtr.readAccessHashTable();
		HashTable<J9ModulePointer> readModuleHashTable = ModuleHashTable.fromJ9HashTable(readTable);
		SlotIterator<J9ModulePointer> readSlotIterator = readModuleHashTable.iterator();
		while (readSlotIterator.hasNext()) {
			J9ModulePointer readModulePtr = readSlotIterator.next();
			String readHexAddress = readModulePtr.getHexAddress();
			if (targetModuleAddress.equals(readHexAddress)) {
				result = true;
				break;
			}
		}
		return result;
	}

	/**
	 * Check whether a given package is exported. Both
	 * `export <package>` and `export <package> to
	 * <module>` are matched.
	 * 
	 * @param    packagePtr The package that is to be filtered.
	 * @param    arg        Unused.
	 * @return   true if the package is globally exported or exported to a
	 *           specific module.
	 */
	private static boolean filterPackageExported(J9PackagePointer packagePtr, String arg) throws CorruptDataException {
		boolean result = false;
		if (packagePtr.exportToAll().isZero() && packagePtr.exportToAllUnnamed().isZero()) {
			J9HashTablePointer exportsHashTable = packagePtr.exportsHashTable();
			HashTable<J9ModulePointer> exportsModuleHashTable = ModuleHashTable.fromJ9HashTable(exportsHashTable);
			SlotIterator<J9ModulePointer> exportsSlotIterator = exportsModuleHashTable.iterator();
			if (exportsSlotIterator.hasNext()) {
				result = true;
			}
		} else {
			result = true;
		}
		return result;
	}

	private static boolean filterPackageModule(J9PackagePointer packagePtr, String targetModuleAddress) throws CorruptDataException {
		return packagePtr.module().getHexAddress().equals(targetModuleAddress);
	}

	/**
	 * Run both filterPackageModule and filterPackageExported
	 * 
	 * @param    packagePtr             The package to be passed to both filters.
	 * @param    targetModuleAddress    The module address to be passed to both filters.
	 * @return   (filterPackageModule result && filterPackageExported result)
	 */
	private static boolean filterPackageModuleAndExport(J9PackagePointer packagePtr, String targetModuleAddress) throws CorruptDataException {
		return filterPackageModule(packagePtr, targetModuleAddress) && filterPackageExported(packagePtr, targetModuleAddress);
	}

	private static void printHelp(PrintStream out) {
		out.println("Usage:");
		out.println("  !dumpmodule <moduleAddress>");
		out.println("      Lists !dumpmodule all <moduleAddress>");
		out.println("  !dumpmodule all <moduleAddress>");
		out.println("      Lists the requires and exports of the target module");
		out.println("  !dumpmodule requires <moduleAddress>");
		out.println("      Lists all modules required by the target module");
		out.println("  !dumpmodule exports <moduleAddress>");
		out.println("      Lists all packages exported by the target module");
		out.println("  !dumpmodule classes <moduleAddress>");
		out.println("      Lists all loaded classes in the target module");
		out.println("  !dumpmodule packages <moduleAddress>");
		out.println("      Lists all packages in the target module");
	}
}
