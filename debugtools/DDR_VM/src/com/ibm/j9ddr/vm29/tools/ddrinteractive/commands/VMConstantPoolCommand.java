/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_DOUBLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FLOAT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_HANDLE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INSTANCE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_LONG;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STATIC_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STRING;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED8;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTION_MASK;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.structure.J9ROMConstantPoolItem;

public class VMConstantPoolCommand extends Command {

	static final String nl = System.getProperty("line.separator");
	
	/**
	 * Constructor
	 */
	public VMConstantPoolCommand()
	{
		addCommand("vmconstantpool", "[j9javavmAddress]", "Dump the jclconstantpool entries starting at the top and continuing through the end");
	}
		
	/**
     * Prints the usage for the vmconstantpool command.
     *
     * @param out PrintStream to print the output to the console.
     */
	private void printUsage (PrintStream out) {
		out.println("vmconstantpool [j9javavmAddress] - Dump the jclconstantpool entries starting at the top and continuing through the end");
	}
	
	/**
	 * Run method for !vmconstantpool extension.
	 * 
	 * @param command  !vmconstantpool
	 * @param args	args passed by !vmconstantpool extension.
	 * @param context Context
	 * @param out PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			J9JavaVMPointer javaVM;
			javaVM = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (args.length == 1) {
				long vmaddress = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
				if (vmaddress != javaVM.getAddress()) {
					out.println(args[0] + " is not a valid j9javavm address. Run !findvm to find out the j9javavm address of the current context");
					return;
				}
			} else if (args.length > 1) {
				printUsage(out);
			}
			
			J9ConstantPoolPointer jclConstantPool = J9ConstantPoolPointer.cast(javaVM.jclConstantPoolEA());
			J9ROMClassPointer romClass = jclConstantPool.ramClass().romClass();

			U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);
			long cpDescription = cpShapeDescription.at(0).longValue();
			int constPoolCount = romClass.romConstantPoolCount().intValue();
			PointerPointer cpEntry = PointerPointer.cast(J9ROMClassHelper.constantPool(romClass));
			long cpDescriptionIndex = 0;

			for (int index = 0; index < constPoolCount; index++) {
				if (0 == cpDescriptionIndex) {
					// Load a new description word
					cpDescription = cpShapeDescription.at(0).longValue();
					cpShapeDescription = cpShapeDescription.add(1);
					cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32;
				}
				
				long shapeDesc = cpDescription & J9_CP_DESCRIPTION_MASK;
				AbstractPointer ref = PointerPointer.NULL;
				if (shapeDesc == J9CPTYPE_CLASS) {
					ref = J9ROMClassRefPointer.cast(cpEntry);
				} else if (shapeDesc == J9CPTYPE_STRING) {
					ref = J9ROMStringRefPointer.cast(cpEntry);
				} else if ((shapeDesc == J9CPTYPE_INT) || 
						(shapeDesc == J9CPTYPE_FLOAT)) {
					ref = J9ROMConstantPoolItemPointer.cast(cpEntry);
				} else if (shapeDesc == J9CPTYPE_LONG) {
					U64Pointer longPointer = U64Pointer.cast(cpEntry);
					out.println("Long at " + longPointer.getHexAddress() + " {\n\t0x0: U64:" + longPointer.getHexValue() + "\n}");
				} else if (shapeDesc == J9CPTYPE_DOUBLE) {
					U64Pointer longPointer = U64Pointer.cast(cpEntry);
					out.println("Double at " + longPointer.getHexAddress() + " {\n\t0x0: U64:" + longPointer.at(0).longValue() + "\n}");
				} else if ((shapeDesc == J9CPTYPE_INSTANCE_METHOD) 
							|| (shapeDesc == J9CPTYPE_STATIC_METHOD) 
							|| (shapeDesc == J9CPTYPE_INTERFACE_METHOD)
							|| (shapeDesc == J9CPTYPE_HANDLE_METHOD)
							|| (shapeDesc == J9CPTYPE_FIELD)) {
					long classRefCPIndex;
					
					if (shapeDesc == J9CPTYPE_FIELD) {
						ref = J9ROMFieldRefPointer.cast(cpEntry);
						/* gets the classRefCPIndex to obtain a pointer to the j9romclassref */
						classRefCPIndex = J9ROMFieldRefPointer.cast(ref).classRefCPIndex().longValue();
					} else {
						ref = J9ROMFieldRefPointer.cast(cpEntry);
						classRefCPIndex = J9ROMMethodRefPointer.cast(ref).classRefCPIndex().longValue();
					}
					
					PointerPointer classRefCPEntry = PointerPointer.cast(J9ROMClassHelper.constantPool(romClass)).addOffset(J9ROMConstantPoolItem.SIZEOF * classRefCPIndex);
					
					/* gets the DDR output of the item */
					String outString = ref.formatFullInteractive();
					String[] parts = outString.split(nl);
					/* add a debug extension(!j9romclassref) on the second line of the output */
					parts[1] += "(!j9romclassref " + classRefCPEntry.getHexAddress() + ")";
					out.print(join(nl, parts));
				} else if ((shapeDesc == J9CPTYPE_UNUSED) || (shapeDesc == J9CPTYPE_UNUSED8)) {
					U64Pointer longPointer = U64Pointer.cast(cpEntry);
					out.println("Unused at " + longPointer.getHexAddress() + " {\n\t0x0: U64:" + longPointer.at(0).longValue() + "\n}");
				} else if (ref.notNull()) {
					out.println(ref.formatFullInteractive());
				}

				cpEntry = cpEntry.addOffset(J9ROMConstantPoolItem.SIZEOF);
				cpDescription >>= J9_CP_BITS_PER_DESCRIPTION;
				cpDescriptionIndex -= 1;
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	private static String join( String separator, String[] strings )
    {
		StringBuffer sb = new StringBuffer();
		for (String string : strings) {
			sb.append(string).append(separator);
		}
		return sb.toString();
    }

}
