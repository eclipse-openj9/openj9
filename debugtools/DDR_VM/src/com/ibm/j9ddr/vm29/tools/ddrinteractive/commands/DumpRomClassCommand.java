/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9BCTranslationData;

public class DumpRomClassCommand extends Command {

	public DumpRomClassCommand() {
		addCommand(
				"dumpromclass",
				"<address|name:<classname>> [maps]",
				"cfdump the specified J9ROMClass. Wild cards are allowed in class name. (maps is optional)");
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		try {
			long maps = 0;

			if (args.length != 1 && args.length != 2) {
				printUsage(out);
				return;
			}

			if (args.length == 2 && args[1].equals("maps")) {
				maps |= J9BCTranslationData.BCT_DumpMaps;
			}
			if (J9BuildFlags.env_littleEndian) {
				maps |= J9BCTranslationData.BCT_LittleEndianOutput;
			} else {
				maps |= J9BCTranslationData.BCT_BigEndianOutput;
			}

			/* check for name:<name> */
			if (args[0].startsWith("name:")) {
				long hitCount = 0;
				String searchClassName = args[0]
						.substring(args[0].indexOf(':') + 1);
				PatternString pattern = new PatternString(searchClassName);
				J9JavaVMPointer vm = J9RASHelper.getVM(DataType
						.getJ9RASPointer());
				ClassSegmentIterator iterator = new ClassSegmentIterator(
						vm.classMemorySegments());
				while (iterator.hasNext()) {
					J9ClassPointer classPointer = (J9ClassPointer) iterator
							.next();
					String javaName = J9ClassHelper.getJavaName(classPointer);
					if (pattern.isMatch(javaName)) {
						hitCount++;
						J9ROMClassPointer clazz = classPointer.romClass();
						String hexString = clazz.getHexAddress();
						out.println(String.format("ROMClass %1$s named %2$s\n",
								hexString, javaName));

						J9BCUtil.j9bcutil_dumpRomClass(out, clazz, maps);
					}
				}
				out.println(String.format(
						"Found %1$d class(es) with name %2$s\n", hitCount,
						searchClassName));
			} else {
				/* treat argument as address */
				long addr = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);

				J9ROMClassPointer clazz = J9ROMClassPointer.cast(addr);
				J9BCUtil.j9bcutil_dumpRomClass(out, clazz, maps);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * Prints the usage for the dumpromclass command.
	 * 
	 * @param out
	 *            the PrintStream the usage statement prints to
	 */
	private void printUsage(PrintStream out) {
		out.println("dumpromclass <address|name:<classname>> [map] - cfdump the specified J9ROMClass. Wild cards are allowed in class name. (maps is optional)");
	}

}
