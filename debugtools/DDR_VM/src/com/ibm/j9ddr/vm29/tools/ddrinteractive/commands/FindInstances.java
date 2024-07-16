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

import com.ibm.j9ddr.CorruptDataException;

import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Table;

import com.ibm.j9ddr.vm29.j9.DataType;

import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;

import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;

import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

import java.io.PrintStream;

public class FindInstances extends Command
{
	private String className;
	private J9ClassPointer classPointer;
	private Table data;
	private long corruptCount;
	private long objectsFound;

	public FindInstances() {
		addCommand("findinstances", "<classname>", "find all instances of the specific class and its subclasses");
	}

	private static void printUsage (PrintStream out) {
		out.println("findinstances <name> - find all instances of the specific class and its subclasses");
	}

	private boolean parseArgs(PrintStream out, String[] args) throws DDRInteractiveCommandException {
		if ((args == null) || (args.length != 1)) {
			out.println("Exactly one argument expected");
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

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if (!parseArgs(out, args)) {
			return;
		}

		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ClassSegmentIterator iterator = new ClassSegmentIterator(vm.classMemorySegments());

			out.format(
				"Searching for class named '%s' in VM=%s%n",
				className, Long.toHexString(vm.getAddress()));

			/* Reset classPointer for each invocation of the command. */
			classPointer = null;

			while (iterator.hasNext()) {
				J9ClassPointer clsPointer = (J9ClassPointer)iterator.next();
				String javaName = J9ClassHelper.getJavaName(clsPointer);
				if (className.equals(javaName)) {
					String hexString = clsPointer.getHexAddress();
					classPointer = clsPointer;
					out.format("Found !j9class %s named %s%n", hexString, javaName);
					break;
				}
			}

			if (classPointer == null) {
				out.format("No class named %s found%n", className);
				return;
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

		/* Allocate a new instance of Table for each invocation of the command. */
		data = new Table("Instances of " + className);
		data.row("Address", "Class Name");

		/* Reset counts for each invocation of the command. */
		corruptCount = 0;
		objectsFound = 0;

		scanHeap();

		out.format("Objects found: %d%n", objectsFound);
		out.format("Corruptions encountered: %d%n", corruptCount);

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
				if (objClass.notNull() && J9ClassHelper.isSameOrSuperClassOf(classPointer, objClass)) {
					data.row(object.getHexAddress(), J9ClassHelper.getJavaName(objClass));
					objectsFound += 1;
				}
			} catch (CorruptDataException e) {
				corruptCount += 1;
			}
		}
	}
}
