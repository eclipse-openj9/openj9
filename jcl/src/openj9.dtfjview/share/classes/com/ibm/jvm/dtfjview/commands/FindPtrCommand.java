/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.nio.ByteOrder;

import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.IDTFJContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;

@DTFJPlugin(version = "1.*", runtime = false)
public class FindPtrCommand extends BaseJdmpviewCommand {

	String pattern;

	{
		addCommand("findptr", "", "searches memory for the given pointer");
	}

	@Override
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if (initCommand(command, args, context, out)) {
			return; // processing already handled by super class
		}
		if (args.length == 1) {
			String line = args[0];
			if (line.endsWith(",")) {
				// In order for the split to work there needs to always be a last parameter present.
				// If it is missing we can default to only displaying the first match.
				line += "1";
			}
			String[] params = line.split(",");
			if (!isParametersValid(params)) {
				return;
			}
			if (isLittleEndian(context)) {
				pattern = reorderBytes();
			}
			String parameters = getParameters(params);

			out.println("issuing: find" + " " + parameters);

			ctx.execute("find" + " " + parameters, out);
		} else {
			out.println("\"find\" takes a set comma separated parameters with no spaces");
		}
	}

	private String getParameters(String[] params) {
		String temp = "0x" + pattern;
		for (int i = 1; i < params.length; i++) {
			temp += ("," + params[i]);
		}
		return temp;
	}

	private String reorderBytes() {
		if (0 != pattern.length() % 2) {
			pattern = "0" + pattern;
		}
		String temp = "";
		for (int i = pattern.length() / 2 - 1; i >= 0; i--) {
			temp += pattern.substring(i * 2, i * 2 + 2);
		}
		return temp;
	}

	private static boolean isLittleEndian(IContext context) {
		if (context instanceof IDTFJContext) {
			ImageAddressSpace addressSpace = ((IDTFJContext) context).getAddressSpace();

			if (ByteOrder.LITTLE_ENDIAN == addressSpace.getByteOrder()) {
				return true;
			}
		}

		return false;
	}

	private boolean isParametersValid(String[] params) {
		if (6 != params.length) {
			out.println("insufficient number of parameters");
			return false;
		}
		pattern = params[0];
		if (pattern.equals("")) {
			out.println("missing pattern string");
			return false;
		}

		if (pattern.startsWith("0x")) {
			pattern = pattern.substring(2);
		}
		return true;
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("searches memory for the given pointer\n\n"
				+ "parameters: see parameters for \"find\" command\n\n"
				+ "the findptr command searches for <pattern> as a pointer in the memory segment from <start_address> to <end_address> (both inclusive), "
				+ "and outputs the first <matches_to_display> matching addresses that start at the corresponding <memory_boundary>. "
				+ "It also display the next <bytes_to_print> bytes for the last match.");
	}

}
