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
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9JavaVM;

public class SetVMCommand extends Command
{

	/**
	 * Constructor. It adds the command info into !j9help menu
	 */
	public SetVMCommand()
	{
		addCommand("setvm", "<address>", "set the JavaVM address");
	}
	
	/**
	 * Prints the info about the usage of this command: !setVm
	 * It is being called if the there is none or more than one arguments passed to !setVM extension.
	 *  
	 * @param out PrintStream to print user msgs.
	 * @return void
	 */
	private void printHelp(PrintStream out) 
	{
		out.append("Usage: \n");
		out.append("  !setvm <address>\n");
	}

	/**
	 * Runs the !setvm command. 
	 * 1. Checks whether correct number of args passed which is 1. 
	 * 2. If condition 1 is successful, checks whether the passed arg is a valid integer, decimal or hexadecimal. 
	 * 3. If condition 2 is successful, checks whether the address (only arg) is a valid J9JavaVM address.
	 * 4. If condition 3 is unsuccessful, checks whether the address is a valid J9VMThread address.
	 * 
	 * If any of the condition 3 or 4 succeeds, then cachedVM is set to new address. 
	 * Otherwise, it prints error msg. 
	 * 
	 * @param command DDR extension command which is setVM for this extension.
	 * @param args arguments passed with the command
	 * @param context current context that DDR is running on. 	
	 * @param  out	PrintStream to print the user messages. 
	 * @return void
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (1 != args.length) {
			printHelp(out);
			return;
		}
		
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
		
		J9JavaVMPointer vmPtr = J9JavaVMPointer.cast(address);
		if (testJavaVMPtr(vmPtr)) {
			J9RASHelper.setCachedVM(vmPtr);
			out.println("VM set to " + vmPtr.getHexAddress());
			return;
		} else {
			/* hmm. It's not a J9JavaVM. Maybe it's a vmThread? */
			/* try to read the VM slot */
			J9VMThreadPointer threadPtr = J9VMThreadPointer.cast(address);
			try {
				vmPtr = threadPtr.javaVM();
				if (testJavaVMPtr(vmPtr)) {
					J9RASHelper.setCachedVM(vmPtr);
					out.println("VM set to " + vmPtr.getHexAddress());
					return;
				}
			} catch (CorruptDataException e) {
				/* Do Nothing */
			}
		}
		out.println("Error: Specified value (=" + address + ") is not a javaVM or vmThread pointer, VM not set");		
	}
	
	/**
	 * Tests whether the passed address is a valid J9JavaVM address by checking its reserved1 slot.
	 * @param vmPtr J9JavaVMPointer
	 * @return true, it vmPtr is a valid J9JavaVM address, false otherwise
	 */
	private boolean testJavaVMPtr(J9JavaVMPointer vmPtr) 
	{
		/* Jazz 101594: NoSuchFieldError occurs when verifying an old core dump 
		 * /bluebird/javatest/j9unversioned/dump4ddrext/Java727_GA/aix_ppc-64.727ga.core.dmp with DDR
		 * (TestDDRExtJunit_linux_x86-32_core_626sr7.NoOptions) in that only the old field name (reserved1) exists in there.
		 * So we need to check both the new field name and the old one.
		 */
		try {
			if (vmPtr.reserved1_identifier().longValue() == J9JavaVM.J9VM_IDENTIFIER) {
				return true;
			}
		} catch (NoSuchFieldError e) {
			try {
				Method method = vmPtr.getClass().getMethod("reserved1");
				VoidPointer pointer = (VoidPointer)method.invoke(vmPtr);
				Long fieldValue = pointer.longValue();
				
				if (fieldValue == J9JavaVM.J9VM_IDENTIFIER) {
					return true;
				}
			} catch (NoSuchMethodException e1) {
            } catch (IllegalAccessException e2) {
            } catch (IllegalArgumentException e3) {
            } catch (InvocationTargetException e4) {
            } catch (CorruptDataException e5) {
            }
			
			return false;
		} catch (CorruptDataException e) {
			/**
			 * If there is corrupt data exception, then the address to set the vm is not valid
			 * and can not be used to set the new VM.
			 * 
			 */
			return false;
		}
		return false;
	}
}

