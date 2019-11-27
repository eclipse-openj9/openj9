/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.PrintObjectFieldsHelper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;

/**
 *
 * FlatObjectCommand
 *
 * Displays all the fields of a j9object. If a field is nested, all of its fields are
 * displayed as well. Nested fields are indented by one tab space.
 *
 */
public class FlatObjectCommand extends Command {

	public FlatObjectCommand() {
		addCommand("flatobject", "<addressOfContainer> [<fieldName1>[.<fieldName2>]...]", "Display a flattened representation of a j9object given an address.");
	}

	private void printHelp(PrintStream out) {
		out.println("Usage:");
		out.println("  !flatobject <addressOfContainer> [<fieldName1>[.<fieldName2>]...]");
	}

	/**
	 * The method runs the flatobject command. If the first element of args is not empty
	 * this command will start printing fields at a nested member and not the container.
	 *
	 * @param command command name
	 * @param args string array of args.
	 * @param context ddrinteractive context
	 * @param out output stream
	 *
	 */
	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if (args.length == 0) {
			printHelp(out);
			return;
		}
		printFlatObject(command, args, context, out);
	}

	private void printFlatObject(String command, String[] args, Context context, PrintStream out) {
		J9ClassPointer clazz = null;
		J9ObjectPointer object = null;
		String[] argElements = args[0].split("\\.");

		if (!ValueTypeHelper.getValueTypeHelper().areValueTypesSupported()) {
			out.println("<this core file does not support flattened types>");
			return;
		}

		try {
			long address = CommandUtils.parsePointer(argElements[0], J9BuildFlags.env_data64);
			String nestingHeirarchy = null;

			if (1 < args.length) {
				nestingHeirarchy = args[1];
			}

			object = J9ObjectPointer.cast(address);
			clazz = J9ObjectHelper.clazz(object);
			U8Pointer dataStart =  U8Pointer.cast(object).add(ObjectModel.getHeaderSize(object));

			if (clazz.isNull()) {
				out.println("<can not read RAM class address>");
				return;
			}

			if (J9ClassHelper.isArrayClass(clazz)) {
				J9IndexableObjectPointer array = J9IndexableObjectPointer.cast(object);
				int length = J9IndexableObjectHelper.size(array).intValue();
				if (ValueTypeHelper.getValueTypeHelper().isJ9ClassIsFlattened(clazz)) {
					if (null == nestingHeirarchy) {
						for (int i = 0; i < length; i++) {
							String arrayIndex = "[" + i + "]";
							String[] newArgs = { argElements[0], arrayIndex };
							printFlatObject(command, newArgs, context, out);
						}
					} else {
						out.format("!flatobject %s %s {%n", object.getHexAddress(), nestingHeirarchy);
						PrintObjectFieldsHelper.printJ9ObjectFields(out, 1, J9ArrayClassPointer.cast(clazz).componentType(), dataStart, object, address, nestingHeirarchy.split("\\."), true);
						out.println("}");
					}
				} else {
					J9ObjectPointer[] data = (J9ObjectPointer[]) J9IndexableObjectHelper.getData(array);
					for (int i = 0; i < length; i++) {
						String[] newArgs = { data[i].getHexAddress() };
						printFlatObject(command, newArgs, context, out);
					}
				}
			} else {
				if (null != nestingHeirarchy) {
					out.format("!flatobject %s %s {%n", object.getHexAddress(), nestingHeirarchy);
				} else {
					out.format("!flatobject %s {%n", object.getHexAddress());
				}

				PrintObjectFieldsHelper.printJ9ObjectFields(out, 1, clazz, dataStart, object, address, nestingHeirarchy == null ? null : nestingHeirarchy.split("\\."), true);
				out.println("}");
			}
		} catch (MemoryFault ex2) {
			if ((object == null) || (clazz == null)) {
				out.format("Unable to read object with command !flatobject %s%n", argElements[0]);
			} else {
				out.format("Unable to read object clazz at %s (clazz = %s) with command !flatobject %s%n", object.getHexAddress(), clazz.getHexAddress(), argElements[0]);
			}
		} catch (CorruptDataException | DDRInteractiveCommandException ex) {
			out.println("Error for");
			ex.printStackTrace(out);
		}
	}
}
