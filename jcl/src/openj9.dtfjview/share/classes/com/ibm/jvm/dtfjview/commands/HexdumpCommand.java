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
import java.util.Arrays;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class HexdumpCommand extends BaseJdmpviewCommand {

	{
		addCommand("hexdump", "<hex address> [<bytes_to_print>]", "outputs a section of memory in hexadecimal, ASCII and EBCDIC");	
	}

	@Override
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if (initCommand(command, args, context, out)) {
			return; // processing already handled by super class
		}
		doCommand(args);
	}

	public void doCommand(String[] args) {
		ImageAddressSpace imageAddressSpace = ctx.getAddressSpace();

		if (null == imageAddressSpace) {
			out.println("Could not find an address space which contains a process in this core file");
			return;
		}

		if (args.length == 0) {
			out.println("\"hexdump\" requires at least an address parameter");
			return;
		}

		long address;

		{
			Long longAddress = Utils.longFromString(args[0]);

			if (null != longAddress) {
				address = longAddress.longValue();
			} else {
				out.println("Specified address is invalid: " + args[0]);
				return;
			}
		}

		int numBytesToPrint = 256; // dump 256 bytes by default

		if (args.length > 1) {
			try {
				numBytesToPrint = Integer.decode(args[1]);
			} catch (NumberFormatException e) {
				out.println("Specified length is invalid: " + args[1]);
				return;
			}
		}

		boolean is_zOSdump = false;

		try {
			is_zOSdump = ctx.getImage().getSystemType().toLowerCase().contains("z/os");
		} catch (CorruptDataException | DataUnavailable e) {
			// unable to get the dump OS type, continue without the additional z/OS EBCDIC option
		}

		ImagePointer imagePointer = imageAddressSpace.getPointer(address);
		StringBuilder lineBuffer = new StringBuilder();
		char[] asciiBlock = new char[16];
		char[] ebcdicBlock = new char[16];

		out.println();

		for (int byteOffset = 0; byteOffset < numBytesToPrint; ++byteOffset) {
			try {
				int lineOffset = byteOffset % 16;

				if (0 == lineOffset) {
					lineBuffer.append(Long.toHexString(address + byteOffset));
					lineBuffer.append(':');
				}

				int byteValue = imagePointer.getByteAt(byteOffset) & 0xFF;

				if (0 == (lineOffset % 4)) {
					lineBuffer.append(' ');
				}

				lineBuffer.append(String.format("%02x", byteValue));
				asciiBlock[lineOffset] = Utils.byteToAscii.charAt(byteValue);

				if (is_zOSdump) {
					// for z/OS dumps, output additional EBCDIC interpretation of the memory bytes
					ebcdicBlock[lineOffset] = Utils.byteToEbcdic.charAt(byteValue);
				}

				if (15 == lineOffset) {
					lineBuffer.append("  |");
					lineBuffer.append(asciiBlock);
					lineBuffer.append('|');

					if (is_zOSdump) {
						lineBuffer.append(" |");
						lineBuffer.append(ebcdicBlock);
						lineBuffer.append('|');
					}

					out.println(lineBuffer.toString());
					lineBuffer.setLength(0);
				}
			} catch (CorruptDataException e) {
				out.println("Dump data is corrupted");
				return;
			} catch (MemoryAccessException e) {
				out.println("Address not in memory - 0x" + Long.toHexString(address + byteOffset));
				return;
			}
		}

		int lineOffset = numBytesToPrint % 16;

		if (0 != lineOffset) {
			for (int offset = lineOffset; offset < 16; ++offset) {
				if (0 == (offset % 4)) {
					lineBuffer.append(' ');
				}
				lineBuffer.append("  ");
			}

			Arrays.fill(asciiBlock, lineOffset, 16, ' ');
			lineBuffer.append("  |");
			lineBuffer.append(asciiBlock);
			lineBuffer.append('|');

			if (is_zOSdump) {
				Arrays.fill(ebcdicBlock, lineOffset, 16, ' ');
				lineBuffer.append(" |");
				lineBuffer.append(ebcdicBlock);
				lineBuffer.append('|');
			}

			out.println(lineBuffer.toString());
		}

		out.println();

		ctx.getProperties().put(Utils.CURRENT_MEM_ADDRESS, Long.valueOf(address));
		ctx.getProperties().put(Utils.CURRENT_NUM_BYTES_TO_PRINT, Integer.valueOf(numBytesToPrint));
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.format("outputs a section of memory in hexadecimal, ASCII and EBCDIC%n"
				+ "%n" 
				+ "parameters: <hex_address> [<bytes_to_print>]%n" 
				+ "%n"
				+ "outputs <bytes_to_print> (default 256) bytes of memory contents starting from <hex_address>%n"
				+ "EBCDIC output is also provided for z/OS dumps%n");
	}
}
