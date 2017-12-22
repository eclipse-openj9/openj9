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
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RomClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegionNode;

public class QueryRomClassCommand extends Command 
{
	public QueryRomClassCommand()
	{
		addCommand("queryromclass", "<addr>,q1[,q2,...]", "query the specified J9ROMClass (equivalent to -drq in cfdump)\n" +
				String.format("%-25s %-20s \n", "    Query examples:", "<addr>,/romHeader,/romHeader/className,/methods,") +
				String.format("%-25s %-20s \n", "", "<addr>,/methods/method[0],/methods/method[3]/name,") +
				String.format("%-25s %-20s", "", "<addr>,/methods/method[3]/methodBytecodes"));
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length == 0) {
			printUsage(out);
			return;
		}
		try {
			String[] queries = args[0].split(",");
			
			long address = CommandUtils.parsePointer(queries[0], J9BuildFlags.env_data64);
			
			J9ROMClassPointer romClass = J9ROMClassPointer.cast(address);
			queries[0] = "";

			ClassWalker classWalker = new RomClassWalker(romClass, context);
			LinearDumper linearDumper = new LinearDumper();
			J9ClassRegionNode allRegionsNode = linearDumper.getAllRegions(classWalker);

			for (String query : queries) {
				if (query.length() != 0) {
					queryROMClass(out, romClass, query, allRegionsNode);
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	private void printUsage(PrintStream out) 
	{
		CommandUtils.dbgPrint(out, "Usage:" + nl);
		out.println("!queryromclass <addr>,q1[,q2,...] - query the specified J9ROMClass (equivalent to -drq in cfdump)");
		out.println("Query examples: <addr>,/romHeader,/romHeader/className,/methods,");
		out.println("\t<addr>,/methods/method[0],/methods/method[3]/name,");
		out.println("\t<addr>,/methods/method[3]/methodBytecodes");
	}

	private void queryROMClass(PrintStream out, J9ROMClassPointer romClass, String query, J9ClassRegionNode allRegionsNode) throws CorruptDataException 
	{
	
		/* /methods/method[3]/methodBytecodes */
		String[] parts = query.split("/");
		for (String part : parts) {
			if (part.length() != 0) {
				String sectionName;
				int index;
				if (part.endsWith("]")) {
					Pattern p = Pattern.compile("^(.*)\\[(\\d*)\\]");
					Matcher m = p.matcher(part);
					m.find();
					sectionName = m.group(1).trim();
					index = Integer.parseInt(m.group(2).trim());
				} else {
					sectionName = part;
					index = 0;
				}

				final List<J9ClassRegionNode> children = allRegionsNode.getChildren();
				if(children.size() <= index) {
					out.println("Error: section not found or bad formatting");
					printUsage(out);
					return;
				}
				for (J9ClassRegionNode classRegionNode : children) {
					if (classRegionNode.getNodeValue() != null) {
						if (classRegionNode.getNodeValue().getName().equals(sectionName)) {
							if (index-- == 0) {
								allRegionsNode = classRegionNode;
								continue;
							}
						}
					}
				}
			}
		}
		CommandUtils.dbgPrint(out, String.format("ROM Class '%s' at %s:\n\n", J9UTF8Helper.stringValue(romClass.className()), romClass.getHexAddress()));
		LinearDumper.printAllRegions(out, romClass, 9, allRegionsNode, 0);
	}
}
