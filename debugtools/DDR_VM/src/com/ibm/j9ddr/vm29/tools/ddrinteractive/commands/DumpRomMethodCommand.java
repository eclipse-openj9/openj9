/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_BigEndianOutput;
import static com.ibm.j9ddr.vm29.structure.J9BCTranslationData.BCT_LittleEndianOutput;

import java.io.PrintStream;
import java.util.Vector;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.J9ROMClassAndMethod;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.FilteredROMMethodsIterator;

public class DumpRomMethodCommand extends Command {

	private static final String DUMPROMMETHOD = "dumprommethod";
	private static final String COMMAND_SYNTAX = "-a <ram method addr>|-o <rom method addr>|<name>";
	private static final String COMMAND_DESCRIPTION = "dump all the rommethods corresponding to RAM method address <addr>, or to <name> (with wildcards)";
	private final long dumpFlags;
	public DumpRomMethodCommand() {
		dumpFlags = J9BuildFlags.env_littleEndian ?  BCT_LittleEndianOutput: BCT_BigEndianOutput;
		addCommand(DUMPROMMETHOD, COMMAND_SYNTAX, COMMAND_DESCRIPTION);
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {

		try {
			Iterable<J9ROMClassAndMethod> methodIterator = null;
			if (args.length < 1)  {
				printUsage(out);
				return;
			}
			String selector = args[0];
			if (selector.equals("-a")) {
				if (args.length != 2) {
					printUsage(out);
					return;
				}
				/* decimal or hexadecimal */

				long methodAddress = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
				J9MethodPointer ramMethod =  J9MethodPointer.cast(methodAddress);
				if (ramMethod.isNull()) {
					CommandUtils.dbgPrint(out, "bad ram method addr\n");
					return;
				}
				methodIterator = romMethodIteratorFromRamMethod(ramMethod);			

			} else if (selector.equals("-o")) {
				if (args.length != 2) {
					printUsage(out);
					return;
				}
				long methodAddress = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
				J9ROMMethodPointer romMethod =  J9ROMMethodPointer.cast(methodAddress);
				if (romMethod.isNull()) {
					CommandUtils.dbgPrint(out, "bad rom method addr\n");
					return;
				}
				
				J9ROMMethodPointer bytecodesStart = romMethod.add(1); /* bytecodes immediately follow the J9ROMMethod struct */
				U8Pointer pc = U8Pointer.cast(bytecodesStart.getAddress());
				J9MethodPointer ramMethod = J9JavaVMHelper.getMethodFromPC(J9RASHelper.getVM(DataType.getJ9RASPointer()), pc);
				methodIterator = romMethodIteratorFromRamMethod(ramMethod);			
			} else {
				try {
					methodIterator = new FilteredROMMethodsIterator(out, context, selector);
				} catch (CorruptDataException e) {
					throw new DDRInteractiveCommandException(e);
				}
			}
			for (J9ROMClassAndMethod mi: methodIterator) {
				out.println(String.format("Class: %s",  J9UTF8Helper.stringValue(mi.romClass.className())));
				J9BCUtil.j9bcutil_dumpRomMethod(out, mi.romMethod, mi.romClass, dumpFlags, J9BCUtil.BCUtil_DumpAnnotations);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		} catch (NullPointerException e) {
			e.printStackTrace();
			throw e;
		}
	}

	private Iterable<J9ROMClassAndMethod> romMethodIteratorFromRamMethod(J9MethodPointer ramMethod)
			throws CorruptDataException {
		Iterable<J9ROMClassAndMethod> methodIterator;
		J9ClassPointer ramClass = ConstantPoolHelpers.J9_CLASS_FROM_METHOD(ramMethod);
		J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(ramMethod);
		J9ROMClassPointer romClass = ramClass.romClass();

		Vector<J9ROMClassAndMethod> methodInfoVector = new Vector<J9ROMClassAndMethod>(1);
		methodInfoVector.add(new J9ROMClassAndMethod(romMethod, romClass));
		methodIterator = methodInfoVector;
		return methodIterator;
	}
	 /**
     * Prints the usage for the dumprommethod command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println(DUMPROMMETHOD+" "+COMMAND_SYNTAX+": "+COMMAND_DESCRIPTION);
		out.println("<name> is classname.methodname(signature.  package names are separated by '/'. Signature is optional ");
	}

}
