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
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.stackmap.StackMap;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

public class StackmapCommand extends Command 
{
	// CONSTANTS
	static final int  MAX_STACKSLOTS_COUNT = 65536;
	static final int  INT_SIZE_IN_BITS = 32;
	
	/**
	 * Constructor
	 * 
	 * Add the command into supported commands list.
	 */
	public StackmapCommand()
	{
		addCommand("stackmap", "<pc>", "calculate the stack slot map for the specified PC");
	}
	
	/**
	 * This method prints the usage of the !stackmap command 
	 * in the case user uses the this command wrong.
	 *  
	 * @param out Print stream to write the output. 
	 * 
	 * @return void
	 */
	private void printUsage(PrintStream out)
	{
		out.println("stackmap <pc> - calculate the stack slot map for the specified PC");
	}
	
	/**
	 *  Java representation of j9dbgext.c#dbgext_stackmap function.
	 *  
	 *  @param  command DDR extension command of stackmap. 
	 *                  It is supposed to be "stackmap" if this method is being called.
	 *  @param  args    Arguments passed to !stackmap DDR extension. 
	 *                  Only one argument is expected since !stackmap expects only the address.
	 *  @param  context Current context that DDR is running on.
	 *  @param  out     Print stream to write the output.       
	 *  
	 *  @return void      
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{			
		if(args.length != 1) {
			printUsage(out);
			return;			
		}
		
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (null == vm) {
				out.println("vm can not be found.");
			}	
		
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);

			U8Pointer pc = U8Pointer.cast(address);
			CommandUtils.dbgPrint(out, "Searching for PC=%d in VM=%s...\n", pc.longValue(), vm.getHexAddress());

			J9MethodPointer remoteMethod = J9JavaVMHelper.getMethodFromPC(vm, pc);
			if (remoteMethod.notNull()) {
				int[] stackMap = new int[MAX_STACKSLOTS_COUNT / INT_SIZE_IN_BITS];
				int leftMostBitIntMask = 1 << (INT_SIZE_IN_BITS - 1);
	
				CommandUtils.dbgPrint(out, "Found method %s !j9method %s\n", J9MethodHelper.getName(remoteMethod), remoteMethod.getHexAddress());

				UDATA offsetPC = new UDATA(pc.sub(U8Pointer.cast(remoteMethod.bytecodes())));
				CommandUtils.dbgPrint(out, "Relative PC = %d\n", offsetPC.longValue());
				J9ClassPointer localClass = J9_CLASS_FROM_CP(remoteMethod.constantPool());
				long methodIndex = new UDATA(remoteMethod.sub(localClass.ramMethods())).longValue();			
				CommandUtils.dbgPrint(out, "Method index is %d\n", methodIndex);
				
				J9ROMMethodPointer localROMMethod = J9ROMCLASS_ROMMETHODS(localClass.romClass());
				while (methodIndex != 0) {
					localROMMethod = ROMHelp.nextROMMethod(localROMMethod);
					--methodIndex;
				}	
			
				CommandUtils.dbgPrint(out, "Using ROM method %s\n", localROMMethod.getHexAddress());
				
				/*
				 * This call will return the depth of the stack or errorcode in case of a failure.
				 */
				int errorCode = StackMap.j9stackmap_StackBitsForPC(offsetPC, localClass.romClass(), localROMMethod, null, 0);
				
				if (0 > errorCode) {
					CommandUtils.dbgPrint(out, "Stack map failed, error code = %d\n", errorCode);
				} else {
					int stackMapIndex = 0;
					if (0 != errorCode) {
						/* This call to j9stackmap_StackBitsForPC will fill the stackMap */
						errorCode = StackMap.j9stackmap_StackBitsForPC(offsetPC, localClass.romClass(), localROMMethod, stackMap, errorCode);
						
						int currentDescription = stackMap[stackMapIndex];					
						int descriptionInt = 0;
						
						CommandUtils.dbgPrint(out, "Stack map (%d slots mapped): ", errorCode);
						long bitsRemaining = errorCode % INT_SIZE_IN_BITS;
						if (bitsRemaining != 0) {
							descriptionInt = currentDescription << (INT_SIZE_IN_BITS - bitsRemaining);
							currentDescription++;
						}
					
						while(errorCode != 0) {
							if (bitsRemaining == 0) {
								descriptionInt = currentDescription;
								currentDescription = stackMap[++stackMapIndex];
								bitsRemaining = INT_SIZE_IN_BITS;
							}
							CommandUtils.dbgPrint(out, "%d", (descriptionInt & (leftMostBitIntMask)) != 0 ? 1 : 0 );
							descriptionInt = descriptionInt << 1;
							--bitsRemaining;
							--errorCode;
						}
						CommandUtils.dbgPrint(out, "\n");
					} else {
						CommandUtils.dbgPrint(out, "Stack is empty\n");
					}
				}
			} else {
				CommandUtils.dbgPrint(out, "Not found\n");
			}		
		} catch (CorruptDataException e) {
			
		}
	}

	
	/**
	 * Java representation of the following macro in VM.
	 * #define J9ROMCLASS_ROMMETHODS(base) NNSRP_GET((base)->romMethods, struct J9ROMMethod*)
	 * 
	 * @param base A pointer to a ROM Class.
	 * 
	 * @return {@link J9ROMMethodPointer}
	 */
	private J9ROMMethodPointer J9ROMCLASS_ROMMETHODS(J9ROMClassPointer base) throws CorruptDataException 
	{
		return base.romMethods();		
	}
	
	
	/**
	 * Java representation of the following macro that is defined in j9cp.h file
	 * #define J9_CLASS_FROM_CP(cp)	(((J9ConstantPool *) (cp))->ramClass)
	 * 
	 * @param constantPool Pointer to a constant pool.
	 * 
	 * @return {@link J9ClassPointer}  A pointer to a class. 
	 * 
	 */
	private J9ClassPointer J9_CLASS_FROM_CP(J9ConstantPoolPointer constantPool) throws CorruptDataException 
	{
		return constantPool.ramClass();
	}
}
