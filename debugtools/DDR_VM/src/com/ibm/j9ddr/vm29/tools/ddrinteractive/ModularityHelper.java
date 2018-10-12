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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.ModuleHashTable;
import com.ibm.j9ddr.vm29.j9.PackageHashTable;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;


public class ModularityHelper {
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

	public static class ModuleFilterMatchAll implements ModuleIteratorFilter {
		/**
		 * Matches all modules. Fits the
		 * ModuleIteratorFilter functional interface.
		 * 
		 * @param    modulePtr  unused
		 * @param    arg        unused
		 * @return   true
		 */
		public boolean filter(J9ModulePointer modulePtr, String arg) throws CorruptDataException {
			return true;
		}
	}

	public static class PrintJ9Module implements ModuleOutput {
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
		public void print(J9ModulePointer modulePtr, PrintStream out) throws CorruptDataException {
			String moduleName = J9ObjectHelper.stringValue(modulePtr.moduleName());
			String hexAddress = modulePtr.getHexAddress();
			out.printf("%-30s !j9module %s%n", moduleName, hexAddress);
		}
	}


	public static class PrintPackageJ9Module implements PackageOutput {
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
		public void print(J9PackagePointer packagePtr, PrintStream out) throws CorruptDataException {
			J9ModulePointer modulePtr = packagePtr.module();
			new PrintJ9Module().print(modulePtr, out);
		}
	}
	/**
	 * Traverses through all loaded modules. Uses
	 * outtputter to print details about modules
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
	public static void iterateModules(PrintStream out, ModuleIteratorFilter filter, ModuleOutput outputter, String filterArg) throws CorruptDataException {
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
						outputter.print(modulePtr, out);
					}
				}
			}
		}
	}


	/**
	 * Traverses through all loaded modules.
	 * Uses outtputter to print details about modules
	 * matched by filter.
	 * 
	 * @param    out        The printstream that will be provided to outputter
	 * @param    filter     Used to determine whether a module will be output.
	 *                      Will be run on all loaded modules.
	 * @param   outputter   Used to output details about a module that was matched
	 *                      by the filter. Will be run on any module that filter
	 *                      returns true for.
	 * @param   filterArg   A string that is to be used as additional data for filter.
	 *                      Will be passed to filter everytime filter is called.
	 */
	public static void iteratePackages(PrintStream out, PackageIteratorFilter filter, PackageOutput outputter, String filterArg) throws CorruptDataException {
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
			GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
			while (iterator.hasNext()) {
				J9ClassLoaderPointer classLoaderPointer = iterator.next();
				HashTable<J9PackagePointer> packageHashTable = PackageHashTable
						.fromJ9HashTable(classLoaderPointer.packageHashTable());
				SlotIterator<J9PackagePointer> slotIterator = packageHashTable.iterator();
				while (slotIterator.hasNext()) {
					J9PackagePointer packagePtr = slotIterator.next();
					if (filter.filter(packagePtr, filterArg)) {
						outputter.print(packagePtr, out);
					}
				}
			}
		}
	}
}
