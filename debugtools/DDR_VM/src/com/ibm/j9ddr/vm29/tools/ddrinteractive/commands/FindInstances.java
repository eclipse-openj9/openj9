/*
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Table;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;

public class FindInstances extends Command
{
	private static final String nl = System.getProperty("line.separator");

	private String className;
	private J9ClassPointer classPointer;
	private Table data;
	private long corruptCount = 0;
	private long objectsFound = 0;

	public FindInstances() {
		addCommand("findinstances", "<classname>", "find all instances of the specific class and its subclasses");
	}

	private void printUsage (PrintStream out) {
		out.println("findinstances <name> - find all instances of the specific class and its subclasses");
	}

	private boolean parseArgs(PrintStream out, String[] args) throws DDRInteractiveCommandException {
		if (args.length != 1) {
			out.append("Invalid number of arguments" + nl);
			return false;
		}
		String firstArg = args[0];
		if (firstArg.equals("help")) {
			printUsage(out);
			return false;
		}
		className = firstArg;
		return true;
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args != null) {
			if (!parseArgs(out, args)) {
				return;
			}
		}

		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ClassSegmentIterator iterator = new ClassSegmentIterator(vm.classMemorySegments());

			out.println(
				String.format(
					"Searching for class named '%1$s' in VM=%2$s",
					className, Long.toHexString(vm.getAddress())));

			/* Reset classPointer for each invocation of the command. */
			classPointer = null;

			while (iterator.hasNext()) {
				J9ClassPointer clsPointer = (J9ClassPointer)iterator.next();
				String javaName = J9ClassHelper.getJavaName(clsPointer);
				if (className.equals(javaName)) {
					String hexString = clsPointer.getHexAddress();
					classPointer = clsPointer;
					out.println(String.format("Found !j9class %1$s named %2$s", hexString, javaName));
					break;
				}
			}

			if (classPointer == null) {
				out.println(String.format("No class named %1$s found", className));
				return;
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

		/* Allocate a new instance of Table for each invocation of the command. */
		data = new Table("Instances of " + className);
		if (data == null) {
			out.append("Failed to allocate an instance of Table" + nl);
			return;
		}

		data.row("Address", "Class Name");

		/* Reset counts for each invocation of the command. */
		corruptCount = 0;
		objectsFound = 0;

		scanHeap();

		out.println(String.format("Objects found: %1$d", objectsFound));
		out.println(String.format("Corruptions encountered: %1$d", corruptCount));

		data.render(out);
	}

	private void scanHeap() {
		try {
			GCHeapRegionIterator regions = GCHeapRegionIterator.from();
			while (regions.hasNext()) {
				GCHeapRegionDescriptor region = regions.next();
				scanObjects(region);
			}
		} catch (CorruptDataException e) {
			e.printStackTrace();
		}

	}

	private void scanObjects(GCHeapRegionDescriptor region) throws CorruptDataException	{
		GCObjectHeapIterator heapIterator = GCObjectHeapIterator.fromHeapRegionDescriptor(region, true, true);
		while (heapIterator.hasNext()) {
			J9ObjectPointer object = heapIterator.next();
			try {
				J9ClassPointer objClass = J9ObjectHelper.clazz(object);
				if (!objClass.isNull() && J9ClassHelper.isSameOrSuperClassOf(classPointer, objClass)) {
					data.row(object.getHexAddress(), J9ClassHelper.getJavaName(objClass));
					objectsFound++;
				}
			} catch (CorruptDataException e) { corruptCount++; }
		}
	}
}
