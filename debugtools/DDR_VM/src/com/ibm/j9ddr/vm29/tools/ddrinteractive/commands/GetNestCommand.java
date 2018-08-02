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
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassLoaderHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;
import com.ibm.j9ddr.vm29.types.U16;

/**
 * GetNest command Outputs the nest host and all the nest members of the class
 * 
 * Example: 
 * !getnest 0x0000000001919400
 *   
 * Example output: 
 * Nest host: 
 * ClassIsOwnNestHost !j9class 0x0000000001919400* 
 * Nest members:
 * 
 */
public class GetNestCommand extends Command 
{
	public GetNestCommand() 
	{
		addCommand("getnest", "<j9ClassAddress>", "Output the nest host and all the nest members of the class");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (args.length == 0) {
			printUsage(out);
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (JavaVersionHelper.ensureJava11AndUp(vm, out)) {
				String hexAddress = args[0];
				long address = CommandUtils.parsePointer(hexAddress, J9BuildFlags.env_data64);
				J9ClassPointer ramClass = J9ClassPointer.cast(address);
				J9ClassLoaderPointer classLoader = ramClass.classLoader();
				J9ROMClassPointer romClass = ramClass.romClass();
				String name = J9UTF8Helper.stringValue(romClass.className());
				long nestMemberCount = romClass.nestMemberCount().longValue();

				if (romClass.nestHost() == VoidPointer.NULL) {
					/* If the class has NULL nest host, it is its own nest host */
					out.printf("Nest host:%n" + "*%s !j9class %s*%nNest members:%n", name, hexAddress);
					printNestMembers(romClass, classLoader, name, nestMemberCount, out);
				} else {
					J9UTF8Pointer nestHost = J9UTF8Pointer.cast(romClass.nestHost());
					String nestHostName = J9UTF8Helper.stringValue(nestHost);
					String nestHostSignature = "L" + nestHostName + ";";
					J9ClassPointer hostClass = J9ClassLoaderHelper.findClass(classLoader, nestHostSignature);
					String hostClassAddr = "Class Not Loaded";
					/* The class cannot have both nest host and nest members */
					if (nestMemberCount != 0) {
						out.printf("Error: class has both nest host and nest members:%n*%s !j9class %s*%n", name,
								hexAddress);
						if (hostClass != null) {
							hostClassAddr = hostClass.getHexAddress();
						}
						out.printf("Nest host:%n" + "*%s !j9class %s*%nNest members:%n", nestHostName, hostClassAddr);
						printNestMembers(romClass, classLoader, name, nestMemberCount, out);
					} else if (nestHostName.matches(name)) {
						/* The class is its own nest host */
						out.printf("Nest host:%n" + "*%s !j9class %s*%nNest members:%nThere are no nest members.%n", name,
								hexAddress);
					} else {
						J9ROMClassPointer hostRomClass = null;
						long hostNestMemberCount = -1;
						if (hostClass != null) {
							hostClassAddr = hostClass.getHexAddress();
							hostRomClass = hostClass.romClass();
							hostNestMemberCount = hostRomClass.nestMemberCount().longValue();
						}
						out.printf("Nest host:%n" + " %s !j9class %s%nNest members:%n", nestHostName, hostClassAddr);
						printNestMembers(hostRomClass, classLoader, name, hostNestMemberCount, out);
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * Prints the usage for the GetNest command.
	 *
	 * @param out The PrintStream the usage statement prints to
	 */
	private void printUsage(PrintStream out) 
	{
		out.println("getnest <j9ClassAddress> - Output the nest host and all the nest members of the class");
	}
	
	/**
	 * Prints the Nest Members of a class.
	 *
	 * @param romClass The class that the method prints nest members from
	 * @param classLoader The class loader used to find member classes
	 * @param nestMemberCount The number of nest members to be printed
	 * @param out The PrintStream that the Nest Members prints to
	 */
	private void printNestMembers(J9ROMClassPointer romClass, J9ClassLoaderPointer classLoader, String name,
			long nestMemberCount, PrintStream out) throws DDRInteractiveCommandException {
		if (nestMemberCount == -1) {
			out.printf("Host class is not loaded, cannot access its nest members.%n");
		} else if (nestMemberCount == 0) {
			out.printf("There are no nest members.%n");
		} else {
			try {
				SelfRelativePointer nestMembers = SelfRelativePointer.cast((AbstractPointer) romClass.nestMembers());
				for (long i = 0; i < nestMemberCount; ++i) {
					J9UTF8Pointer nestMemberNamePtr = J9UTF8Pointer.cast(nestMembers.get());
					String nestMemberName = J9UTF8Helper.stringValue(nestMemberNamePtr);
					String nestMemberSignature = "L" + nestMemberName + ";";
					J9ClassPointer memberClass = J9ClassLoaderHelper.findClass(classLoader, nestMemberSignature);
					String memberClassAddr = "Class Not Loaded";
					if (memberClass != null) {
						memberClassAddr = memberClass.getHexAddress();
					}
					if (nestMemberName.matches(name)) {
						out.printf("*%s !j9class %s*%n", nestMemberName, memberClassAddr);
					} else {
						out.printf(" %s !j9class %s%n", nestMemberName, memberClassAddr);
					}
					nestMembers = nestMembers.add(1);
				}
			} catch (CorruptDataException e) {
				throw new DDRInteractiveCommandException(e);
			}
		}
	}
}
