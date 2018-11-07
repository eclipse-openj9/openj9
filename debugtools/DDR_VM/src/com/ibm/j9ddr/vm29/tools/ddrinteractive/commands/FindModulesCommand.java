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
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ModuleIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.ModuleOutput;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.PackageIteratorFilter;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ModularityHelper.PackageOutput;

public class FindModulesCommand extends Command 
{
	private enum Subcommand {
		MODULE, PACKAGE, HELP, INVALID
	}
	
	public FindModulesCommand() 
	{
		addCommand("findmodules", "[all|name <moduleName>|requires <moduleName>|package <packageName>|help]", "Find a module or set of modules that are loaded by the runtime.");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		final Subcommand subcommand;
		String filterArg = null;
		ModuleIteratorFilter moduleFilter = null;
		ModuleOutput moduleOutput = ModularityHelper::printJ9Module;
		PackageIteratorFilter packageFilter = null;
		switch (args.length) {
		case 0:
			moduleFilter = ModularityHelper::moduleFilterMatchAll;
			subcommand = Subcommand.MODULE;
			break;
		case 1:
			switch (args[0]) {
			case "all":
				moduleFilter = ModularityHelper::moduleFilterMatchAll;
				subcommand = Subcommand.MODULE;
				break;
			case "help":
				subcommand = Subcommand.HELP;
				break;
			default:
				subcommand = Subcommand.INVALID;
				break;
			}
			break;
		case 2:
			filterArg = args[1];
			switch (args[0]) {
			case "name":
				moduleFilter = FindModulesCommand::filterModuleName;
				subcommand = Subcommand.MODULE;
				break;
			case "requires":
				moduleFilter = FindModulesCommand::filterModuleName;
				moduleOutput = FindModulesCommand::printModuleReads;
				subcommand = Subcommand.MODULE;
				break;
			case "package":
				packageFilter = FindModulesCommand::filterPackageName;
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
			switch (subcommand) {
			case MODULE:
				result = ModularityHelper.iterateModules(out, moduleFilter, moduleOutput, filterArg);
				out.printf("Found %d module%s%n", result, (1 == result ? "": "s"));
				break;
			case PACKAGE:
				/* Output the module(s) owning all packages matched by packageFilter */
				ModularityHelper.PackageOutput packageOutput = ModularityHelper::printPackageJ9Module;
				result = ModularityHelper.iteratePackages(out, packageFilter, packageOutput, filterArg);
				out.printf("Found %d module%s%n", result, (1 == result ? "": "s"));
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

	private static void printModuleReads(J9ModulePointer modulePtr, PrintStream out) throws CorruptDataException {
		J9HashTablePointer readTable = modulePtr.readAccessHashTable();
		HashTable<J9ModulePointer> moduleHashTable = ModuleHashTable.fromJ9HashTable(readTable);
		SlotIterator<J9ModulePointer> slotIterator = moduleHashTable.iterator();
		while (slotIterator.hasNext()) {
			J9ModulePointer readModulePtr = slotIterator.next();
			ModularityHelper.printJ9Module(readModulePtr, out);
		}
	}
	
	/**
	 * Matches all modules with the provided name.
	 * Fits the ModuleIteratorFilter functional
	 * interface. Case sensitive.
	 * 
	 * @param    modulePtr  The module to check the name of.
	 * @param    targetName The name that should be matched.
	 * @return   true if the names are equal, false otherwise.
	 */
	private static boolean filterModuleName(J9ModulePointer modulePtr, String targetName) throws CorruptDataException {
		return J9ObjectHelper.stringValue(modulePtr.moduleName()).equals(targetName);
	}


	private static boolean filterRequiresTargetModule(J9ModulePointer modulePtr, String targetModule) throws CorruptDataException {
		boolean result = false;
		J9HashTablePointer readTable = modulePtr.readAccessHashTable();
		HashTable<J9ModulePointer> moduleHashTable = ModuleHashTable.fromJ9HashTable(readTable);
		SlotIterator<J9ModulePointer> slotIterator = moduleHashTable.iterator();
		while (slotIterator.hasNext()) {
			J9ModulePointer readModulePtr = slotIterator.next();
			if (J9ObjectHelper.stringValue(readModulePtr.moduleName()).equals(targetModule)) {
				result = true;
				break;
			}
		}
		return result;
	}

	private static boolean filterPackageName(J9PackagePointer packagePtr, String targetName) throws CorruptDataException  {
		return J9UTF8Helper.stringValue(packagePtr.packageName()).equals(targetName);
	}

	private static void printHelp(PrintStream out) {
		out.println("Usage:");
		out.println("  !findmodules");
		out.println("      Returns !findmodules all");
		out.println("  !findmodules all");
		out.println("      Returns all loaded modules");
		out.println("  !findmodules name <moduleName>");
		out.println("      Returns all loaded modules with the same name as provided");
		out.println("  !findmodules requires <moduleName>");
		out.println("      Returns all loaded modules which require the provided module");
		out.println("  !findmodules package <packageName>");
		out.println("      Returns the loaded module which owns the provided package");
		out.println("Output Format:");
		out.println("  <module name>                  !j9module <module hexaddress>");
		out.println("Output Example:");
		out.println("  java.base                      !j9module 0x00007FAC2008EAC8");
	}
}
