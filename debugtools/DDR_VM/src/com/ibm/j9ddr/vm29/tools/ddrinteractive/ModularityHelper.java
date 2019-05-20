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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
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
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

public class ModularityHelper {
	@FunctionalInterface
	public interface ClassIteratorFilter {
		boolean filter(J9ClassPointer classPtr, String arg) throws CorruptDataException;
	}

	@FunctionalInterface
	public interface ClassOutput {
		void print(J9ClassPointer classPtr, PrintStream out) throws CorruptDataException;
	}

	@FunctionalInterface
	public interface ModuleIteratorFilter {
		boolean filter(J9ModulePointer modulePtr, String arg) throws CorruptDataException;
	}

	@FunctionalInterface
	public interface ModuleOutput {
		void print(J9ModulePointer modulePtr, PrintStream out) throws CorruptDataException;
	}

	@FunctionalInterface
	public interface PackageIteratorFilter {
		boolean filter(J9PackagePointer packagePtr, String arg) throws CorruptDataException;
	}

	@FunctionalInterface
	public interface PackageOutput {
		void print(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException;
	}

	/**
	 * Matches all modules. Fits the
	 * ModuleIteratorFilter functional interface.
	 *
	 * @param    modulePtr  unused
	 * @param    arg        unused
	 * @return   true
	 */
	public static boolean moduleFilterMatchAll(J9ModulePointer modulePtr, String arg) {
		return true;
	}

	/**
	 * Prints the name and hex address of a J9Module.
	 * Example:
	 * java.base    !j9module 0x00007FAC2008EAC8
	 *
	 * @param    modulePtr  The module which is to have its details
	 *                      printed.
	 * @param    out        The PrintStream that the details will
	 *                      be outputted to.
	 */
	public static void printJ9Module(J9ModulePointer modulePtr, PrintStream out) throws CorruptDataException {
		String moduleName = J9ObjectHelper.stringValue(modulePtr.moduleName());
		String hexAddress = modulePtr.getHexAddress();
		out.printf("%-30s !j9module %s%n", moduleName, hexAddress);
	}

	/**
	 * Prints the details of the J9Module that owns a
	 * J9Package. Uses printJ9Module to output the
	 * module details.
	 *
	 * @param    packagePtr The package which is to have its
	 *                      owner's details printed.
	 * @param    out        The PrintStream that the details will
	 *                      be outputted to.
	 *
	 * @see      printJ9Module(J9ModulePointer, PrintStream)
	 */
	public static void printPackageJ9Module(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException {
		J9ModulePointer modulePtr = packagePtr.module();
		printJ9Module(modulePtr, out);
	}

	/**
	 * Prints the name and hex address of a J9Package.
	 * Example:
	 * moduleB/packageB    !j9package 0x00007F84903B5860
	 *
	 * @param    packagePtr The package which is to have its details
	 *                      printed.
	 * @param    out        The PrintStream that the details will
	 *                      be outputted to.
	 */
	public static void printJ9Package(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException {
		String packageName = J9UTF8Helper.stringValue(packagePtr.packageName());
		String hexAddress = packagePtr.getHexAddress();
		out.printf("%-45s !j9package %s%n", packageName, hexAddress);
	}

	/**
	 * Prints the name and hex address of a J9Class.
	 * Example:
	 * moduleA/packageA/classA        !j9class 0x00000000022CCD00
	 *
	 * @param    ClassPtr   The class which is to have its details
	 *                      printed.
	 * @param    out        The PrintStream that the details will
	 *                      be outputted to.
	 */
	public static void printJ9Class(J9ClassPointer classPtr, PrintStream out) throws CorruptDataException {
		String className = J9ClassHelper.getName(classPtr);
		String classAddress = classPtr.getHexAddress();
		out.printf("%-30s !j9class %s%n", className, classAddress);
	}

	/**
	 * Prints the name and hex address of all
	 * J9Modules the provided package is exported to.
	 * Output is formated similar to the format of
	 * module exports within a module-info.java file.
	 * Example:
	 * Exports moduleA/packageA        !j9module 0x00000000022CCD00
	 *      to moduleB    !j9module 0x00007FAC2008EAC8
	 *
	 * @param    packagePtr The package which is to have the
	 *                      modules it is exported to printed.
	 * @param    out        The PrintStream that the result will
	 *                      be output to.
	 */
	public static void printPackageExportVerbose(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException {
		out.print("Exports ");
		ModularityHelper.printJ9Package(packagePtr, out);
		if (packagePtr.exportToAll().isZero()) {
			printPackageExportTo(packagePtr, out, "     to ");
		}
	}

	/**
	 * Prints the name and hex address of all
	 * J9Modules the provided package is exported to.
	 * Example:
	 * moduleB    !j9module 0x00007FAC2008EAC8
	 *
	 * @param    packagePtr The package which is to have the
	 *                      modules it is exported to printed.
	 * @param    out        The PrintStream that the result will
	 *                      be output to.
	 */
	public static void printPackageExports(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException {
		if (packagePtr.exportToAll().isZero()) {
			int exportCount = printPackageExportTo(packagePtr, out, "");
			if (0 == exportCount) {
				out.println("Package is not exported");
			}
		} else {
			out.println("Package is exported to all");
		}
	}

	private static int printPackageExportTo(J9PackagePointer packagePtr, PrintStream out, String linePrefix) throws CorruptDataException {
		int count = 0;
		J9HashTablePointer exportsHashTable = packagePtr.exportsHashTable();
		HashTable<J9ModulePointer> exportsModuleHashTable = ModuleHashTable.fromJ9HashTable(exportsHashTable);
		SlotIterator<J9ModulePointer> exportsSlotIterator = exportsModuleHashTable.iterator();
		while (exportsSlotIterator.hasNext()) {
			count++;
			J9ModulePointer modulePtr = exportsSlotIterator.next();
			out.print(linePrefix);
			ModularityHelper.printJ9Module(modulePtr, out);
		}
		if (!packagePtr.exportToAllUnnamed().isZero()) {
			count++;
			out.printf("%sALL-UNNAMED%n", linePrefix);
		}
		return count;
	}

	/**
	 * Traverses through all loaded modules. Uses
	 * outputter to print details about modules
	 * matched by filter.
	 *
	 * @param    out        The printstream that will be provided to outputter.
	 * @param    filter     Used to determine whether a module will be output.
	 *                      Will be run on all loaded modules.
	 * @param    outputter  Used to output details about a module that was
	 *                      matched by the filter. Will be run on any module that
	 *                      filter returns true for.
	 * @param    filterArg  A string that is to be used as additional data for filter.
	 *                      Will be passed to filter everytime filter is called.
	 */
	public static int iterateModules(PrintStream out, ModuleIteratorFilter filter, ModuleOutput outputter, String filterArg) throws CorruptDataException {
		int count = 0;
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
			GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
			while (iterator.hasNext()) {
				J9ClassLoaderPointer classLoaderPointer = iterator.next();
				HashTable<J9ModulePointer> moduleHashTable = ModuleHashTable
						.fromJ9HashTable(classLoaderPointer.moduleHashTable());
				SlotIterator<J9ModulePointer> slotIterator = moduleHashTable.iterator();
				while (slotIterator.hasNext()) {
					J9ModulePointer modulePtr = slotIterator.next();
					if (filter.filter(modulePtr, filterArg)) {
						count++;
						outputter.print(modulePtr, out);
					}
				}
			}
		}
		return count;
	}

	/**
	 * Traverses through all loaded packages.
	 * Uses outputter to print details about
	 * packages matched by filter.
	 *
	 * @param    out        The printstream that will be provided to outputter
	 * @param    filter     Used to determine whether a package will be output.
	 *                      Will be run on all loaded packages.
	 * @param   outputter   Used to output details about a package that was matched
	 *                      by the filter. Will be run on any module that filter
	 *                      returns true for.
	 * @param   filterArg   A string that is to be used as additional data for filter.
	 *                      Will be passed to filter everytime filter is called.
	 */
	public static int iteratePackages(PrintStream out, PackageIteratorFilter filter, PackageOutput outputter, String filterArg) throws CorruptDataException {
		int count = 0;
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
			GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
			while (iterator.hasNext()) {
				count += iterateClassLoaderPackages(out, filter, outputter, filterArg, iterator.next());
			}
		}
		return count;
	}

	/**
	 * Traverses through all loaded packages that are
	 * owned by the given class loader. Uses
	 * outputter to print details about packages
	 * matched by filter.
	 *
	 * @param    out        The printstream that will be provided to outputter
	 * @param    filter     Used to determine whether a packages will be output. Will
	 *                      be run on all loaded packages owned by the class loader.
	 * @param   outputter   Used to output details about a packages that was matched
	 *                      by the filter. Will be run on any packages that filter
	 *                      returns true for.
	 * @param   filterArg   A string that is to be used as additional data for filter.
	 *                      Will be passed to filter everytime filter is called.
	 */
	public static int iterateClassLoaderPackages(PrintStream out, PackageIteratorFilter filter, PackageOutput outputter, String filterArg, J9ClassLoaderPointer classLoaderPtr) throws CorruptDataException {
		int count = 0;
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
			HashTable<J9PackagePointer> packageHashTable = PackageHashTable
					.fromJ9HashTable(classLoaderPtr.packageHashTable());
			SlotIterator<J9PackagePointer> slotIterator = packageHashTable.iterator();
			while (slotIterator.hasNext()) {
				J9PackagePointer packagePtr = slotIterator.next();
				if (filter.filter(packagePtr, filterArg)) {
					count++;
					outputter.print(packagePtr, out);
				}
			}
		}
		return count;
	}

	/**
	 * Traverses through all loaded classes in a
	 * classloader. Uses outputter to print details
	 * about classes within the classloader that are
	 * matched by filter.
	 *
	 * @param    out            The printstream that will be provided to outputter
	 * @param    filter         Used to determine whether a class will be output.
	 *                          Will be run on all loaded classes owned by the module.
	 * @param    outputter      Used to output details about a class that was matched
	 *                          by the filter. Will be run on any class that filter
	 *                          returns true for.
	 * @param    filterArg      A string that is to be used as additional data for filter.
	 *                          Will be passed to filter everytime filter is called.
	 * @param    classLoaderPtr The classloader which will have it's classes traversed.
	 */
	public static int iterateClassLoaderClasses(PrintStream out, ClassIteratorFilter filter, ClassOutput outputter, String filterArg, J9ClassLoaderPointer classLoaderPtr) throws CorruptDataException {
		int count = 0;
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
			Iterator<J9ClassPointer> classIterator = ClassIterator.fromJ9Classloader(classLoaderPtr);
			while (classIterator.hasNext()) {
				J9ClassPointer classPtr = classIterator.next();
				if (filter.filter(classPtr, filterArg)) {
					count++;
					outputter.print(classPtr, out);
				}
			}
		}
		return count;
	}

}
