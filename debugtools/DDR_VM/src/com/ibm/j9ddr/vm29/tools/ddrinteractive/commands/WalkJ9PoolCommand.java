/*
 * Copyright IBM Corp. and others 2001
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
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;

/**
 * Implementation of DDR extension !walkj9pool.
 *
 * This extension takes a J9Pool address and an optional type argument,
 * and prints the address of each element in the pool as a runnable DDR command.
 * The type argument is used to derive the DDR command prefix to prepend to each
 * element's address. If not provided, the default DDR command prefix is "!j9x".
 */
public class WalkJ9PoolCommand extends Command
{
	private static final String POINTER_MARKER = "*";

	/**
	 * Constructor
	 */
	public WalkJ9PoolCommand()
	{
		addCommand("walkj9pool", "<address> [<type>]", "Walks the elements of a J9Pool");
	}
	
	/**
	 * Prints the usage for the walkj9pool command.
	 *
	 * @param out  the PrintStream the usage statement prints to
	 */
	private static void printUsage(PrintStream out)
	{
		out.println("walkj9pool <address> [<type>] - Walks the elements of a J9Pool");
	}

	/**
	 * Run method for !walkj9pool extension.
	 *
	 * @param command  !walkj9pool
	 * @param args     args passed by !walkj9pool extension
	 * @param context  Context
	 * @param out      PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if ((0 == args.length) || (2 < args.length)) {
			printUsage(out);
			return;
		}

		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.J9VM_ENV_DATA64);

		/* Set the default values. */
		boolean isInline = true;
		String ddrCommand = "!j9x";

		if (args.length >= 2) {
			/* Parse the supplied type argument. */
			String typeName = args[1].trim();
			String baseTypeName;
			if (typeName.endsWith(POINTER_MARKER)) {
				isInline = false;
				baseTypeName = typeName.substring(0, typeName.length() - POINTER_MARKER.length()).trim();
			} else {
				baseTypeName = typeName;
			}

			baseTypeName = baseTypeName.toLowerCase();

			/*
			 * Validate: check that the derived DDR command is actually registered in
			 * this context. This catches typos (e.g. "j9claas*") early with a clear
			 * error message.
			 */
			if (!context.getCommandNames().contains(baseTypeName)) {
				throw new DDRInteractiveCommandException(
						"Unrecognized type '" + typeName + "'");
			}

			ddrCommand = "!" + baseTypeName;
		}

		out.format("J9Pool at 0x%s%n{%n",
				CommandUtils.longToBigInteger(address).toString(CommandUtils.RADIX_HEXADECIMAL));

		walkJ9Pool(address, isInline, ddrCommand, out);

		out.println("}");
	}

	/**
	 * Iterates all elements in the pool and prints each element's address
	 * as a runnable DDR command.
	 *
	 * @param address     address of the pool
	 * @param isInline    true if elements are stored inline in the pool slot;
	 *                    false if the slot holds a pointer to the element
	 * @param ddrCommand  DDR command prefix to prepend
	 * @param out         print stream
	 * @throws DDRInteractiveCommandException
	 */
	private static void walkJ9Pool(long address, boolean isInline, String ddrCommand, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			J9PoolPointer j9pool = J9PoolPointer.cast(address);
			Pool pool = Pool.fromJ9Pool(j9pool, VoidPointer.class, isInline);

			SlotIterator<VoidPointer> poolIterator = pool.iterator();
			for (int i = 0; poolIterator.hasNext(); i++) {
				VoidPointer currentElement = poolIterator.next();
				out.format("    [%d]    =    %s %s%n", i, ddrCommand, currentElement.getHexAddress());
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException("Either address is not a valid pool address or pool itself is corrupted.");
		}
	}
}
