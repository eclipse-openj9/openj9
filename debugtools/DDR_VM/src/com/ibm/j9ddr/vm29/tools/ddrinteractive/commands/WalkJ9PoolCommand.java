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
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;

/**
 * 
 * @author fkaraman
 * 
 * Implementation of DDR extension !walkj9pool
 *
 */
public class WalkJ9PoolCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");
	
	/**
	 * Constructor
	 */
	public WalkJ9PoolCommand()
	{
		addCommand("walkj9pool", "<address>", "Walks the elements of J9Pool");
	}
	
	 /**
     * Prints the usage for the walkj9pool command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println("walkj9pool <address> - Walks the elements of J9Pool");
	}
	
	/**
	 * Run method for !walkj9pool extension.
	 * 
	 * @param command  !walkj9pool
	 * @param args	args passed by !walkj9pool extension.
	 * @param context Context
	 * @param out PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if ((0 == args.length) ||
				(1 < args.length)) {
			printUsage(out);
		}
		
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
		out.append("J9Pool at 0x" 
				+ CommandUtils.longToBigInteger(address).toString(CommandUtils.RADIX_HEXADECIMAL) 
				+ "\n{\n");		
		
		walkJ9Pool(address, out);
		
		out.append("}\n");

	}
	
	/**
	 * This method walks through each element in the pool and prints each elements' address.
	 * Elements can be in the same puddle or different, and this method do not print puddle information.
	 *   
	 * @param address address of the pool
	 * @param out print stream
	 * @throws DDRInteractiveCommandException
	 */
	private void walkJ9Pool(long address, PrintStream out) throws DDRInteractiveCommandException {
		try {
			J9PoolPointer j9pool = J9PoolPointer.cast(address);
			Pool pool = Pool.fromJ9Pool(j9pool, VoidPointer.class);
			
			SlotIterator<VoidPointer> poolIterator = pool.iterator();
			VoidPointer currentElement;
			int i = 0;
			while (poolIterator.hasNext()) {
				currentElement = poolIterator.next();
				out.println(String.format("\t[%d]\t=\t%s", i++, currentElement.getHexAddress()));
				
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException("Either address is not a valid pool address or pool itself is corrupted.");
		}
	}
}
