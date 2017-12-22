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
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.J9JavaStackIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaStackPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class FindStackValueCommand extends Command 
{
	public FindStackValueCommand()
	{
		addCommand("findstackvalue", "<stackvalue>", "search stacks for a specific value");
	}
	
	private void printUsage(PrintStream out){
		out.println("\tUSAGE: ");
		out.println("\t!findstackvalue <stackvalue>");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (0 == args.length) {
			out.println("Usage error: Missing stackvalue to search for. See usage.");
			printUsage(out);
			return;
		} else if (1 < args.length) {
			out.println("Usage error: Too many stackvalues to search for. See usage.");
			printUsage(out);
			return;
		}
		
		try {
			
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);

			UDATAPointer value = UDATAPointer.cast(address);

			GCVMThreadListIterator gcvmThreadListIterator = GCVMThreadListIterator.from();
			while (gcvmThreadListIterator.hasNext()) {
				J9VMThreadPointer vmThreadPointer = gcvmThreadListIterator.next();
				J9JavaStackPointer javaStackPointer = vmThreadPointer.stackObject();
				J9JavaStackIterator javaStackIterator = J9JavaStackIterator.fromJ9JavaStack(javaStackPointer);

				boolean found = false;

				UDATA relativeSP = new UDATA(javaStackPointer.end().sub(vmThreadPointer.sp()));

				while (javaStackIterator.hasNext()) {
					J9JavaStackPointer stack = javaStackIterator.next();

					UDATAPointer localEnd = stack.end().sub(1).add(1);
					UDATAPointer search = localEnd.sub(relativeSP);

					while (!search.eq(localEnd)) {
						if (search.at(0).longValue() == value.longValue()) {
							if (!found) {
								out.append(String.format("!j9vmthread %s\n", Long.toHexString(vmThreadPointer.getAddress())));
								found = true;
							}
							out.append(String.format("\tFound at %s\n", Long.toHexString(search.getAddress())));
						}
						search = search.add(1);
					}
				}

			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
