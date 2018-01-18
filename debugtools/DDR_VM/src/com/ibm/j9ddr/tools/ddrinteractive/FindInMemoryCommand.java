/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;

import com.ibm.j9ddr.corereaders.memory.IProcess;

/**
 * searches memory for a given ASCII string, U8, U16, U64, UDATA or pointer occurrence.
 */
public class FindInMemoryCommand extends Command 
{
	private byte[] currentPattern;
	private int currentAlignment;
	private long currentAddress = -1;

	private static final int TYPE_NONE = 0;
	private static final int TYPE_U8 = 1;
	private static final int TYPE_U16 = 2;
	private static final int TYPE_U32 = 3;
	private static final int TYPE_U64 = 4;
	private static final int TYPE_ASCII = 5;


	public FindInMemoryCommand()
	{
		addCommand("find", "<width|ascii pattern startaddr>", "Find first number or a search string in memory starting at an address");
		addCommand("findall", "<width|ascii pattern startaddr>", "Find all occurances of number or a search string in memory starting at an address");
		addCommand("findnext", "", "Find next match in the search initiated by find");
	}

	private void printHelp(PrintStream out) {
		out.append("Usage: \n");
		out.append("  !find u8|u16|u32|u64|udata|pointer 0xHEX|HEX [0xHexStartAddr|LongStartAddr]\n");
		out.append("  !find ascii <string> [0xHexStartAddr|LongStartAddr]\n");
		out.append("  !findall u8|u16|u32|u64|udata|pointer 0xHEX|HEX [0xHexStartAddr|LongStartAddr]\n");
		out.append("  !findall ascii <string> [0xHexStartAddr|LongStartAddr]\n");
		out.append("  !findnext\n");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		IProcess process = context.process;
		boolean isLittleEndian = process.getByteOrder().equals(ByteOrder.LITTLE_ENDIAN);
		int udataSize = process.bytesPerPointer();
		int udataSearch = udataSize == 4 ? TYPE_U32 : TYPE_U64;
		int searchType = TYPE_NONE;
		String searchTypeName;
		boolean findAll = false;
		byte[] searchPattern;
		int alignment;
		long startAddress;
		int argIndex = 0;
		String arg;

		// check for the findnext and findall variants of the command
		if (command.endsWith("findnext")) {
			findNext(out, process);
			return;
		} else if (command.endsWith("findall")) {
			findAll = true;
		}

		// except for findnext (handled above), there should be parameters following the command
		if (args.length == 0) {
			printHelp(out);
			return;
		}
		// get the first parameter (the search type), and post-increment the parameter index
		arg = args[argIndex++];

		searchTypeName = arg;
		if (arg.equalsIgnoreCase("u8")) {
			searchType = TYPE_U8;
			alignment = 1;
		} else if (arg.equalsIgnoreCase("u16")) {
			searchType = TYPE_U16;
			alignment = 2;
		} else if (arg.equalsIgnoreCase("u32")) {
			searchType = TYPE_U32;
			alignment = 4;
		} else if (arg.equalsIgnoreCase("u64")) {
			searchType = TYPE_U64;
			alignment = 8;
		} else if (arg.equalsIgnoreCase("udata")) {
			searchType = udataSearch;
			alignment = udataSize;
		} else if (arg.equalsIgnoreCase("pointer")) {
			searchType = udataSearch;
			alignment = udataSize;
		} else if (arg.equalsIgnoreCase("ascii")) {
			searchType = TYPE_ASCII;
			alignment = 1;
		} else {
			printHelp(out);
			out.println("Unknown search type '" + searchTypeName + "'\n");
			return;
		}

		/* Now parse the search criteria */
		if (args.length <= argIndex) {
			printHelp(out);
			out.println("Expected pattern as argument " + (argIndex + 1) + "\n");
			return;
		}
		arg = args[argIndex++];
		if (searchType == TYPE_ASCII) {
			if (arg.startsWith("\"")) {
				arg = arg.substring(1, arg.lastIndexOf('"'));
			}
			try {
				searchPattern = arg.getBytes("ASCII");
			} catch (UnsupportedEncodingException e) {
				searchPattern = arg.getBytes(); // fall back to default platform
											// encoding
			}
		} else {
			searchPattern = new byte[alignment];
			if (arg.startsWith("0x")) {
				arg = arg.substring(2 /* 0x */);
			}
			int hexBytes = (arg.length() + 1) / 2;
			if (hexBytes > alignment) {
				out.println("Search pattern too long for type '" + searchTypeName + "'");
				return;
			}
			// Pad for simplicity
			arg = "0000000000000000".concat(arg);
			arg = arg.substring(arg.length() - alignment * 2);
			for (int i = 0; i < alignment; i++) {
				int b = Integer.parseInt(arg.substring(i * 2, i * 2 + 2), 16);
				if (isLittleEndian) {
					searchPattern[alignment - i - 1] = (byte) (b & 0xFF);
				} else {
					searchPattern[i] = (byte) (b & 0xFF);
				}
			}
		}

		/* Now parse the start address (optional) */
		if (args.length <= argIndex) {
			startAddress = 0L;
		} else {
			arg = args[argIndex++];
			if (arg.startsWith("0x")) {
				startAddress = Long.decode(arg);
			} else {
				startAddress = Long.parseLong(arg, 16);
			}
		}

		currentAddress = startAddress;
		currentAlignment = alignment;
		currentPattern = searchPattern;

		boolean result = findFirst(out, process);
		if (findAll) {
			while (result) {
				result = findNext(out, process);
			}
		}
	}

	/**
	 * Search for next occurrence of byte pattern in process memory.
	 * @return false if previous search did not find a pattern or or no pattern was found. 
	 * True if pattern byte was found.
	 */
	private boolean findNext(PrintStream out, IProcess process) {
		if (currentAddress == -1) {
			out.println("No current search");
			return false;
		} else {
			long addr = currentAddress + currentAlignment;
			long result = process.findPattern(currentPattern, currentAlignment, addr);
			if (result == -1) {
				out.println("No more matches");
				currentPattern = null;
				currentAlignment = 1;
				currentAddress = -1;
				return false;
			} else {
				String address = "0x" + Long.toHexString(result);
				out.println("Match found at " + address);
				currentAddress = result;
				return true;
			}
		}
	}

	/**
	 * Search process memory for the byte pattern starting from the current address. 
	 * @return true if match was found. false if no match found.
	 */
	private boolean findFirst(PrintStream out, IProcess process) {
		out.print("Scanning memory for ");
		for (int i = 0; i < currentPattern.length; i++) {
			out.print(String.format("%02x ", currentPattern[i] & 0xFF));
		}
		out.println("aligned to " + currentAlignment + " starting from 0x" + Long.toHexString(currentAddress));
		long result = process.findPattern(currentPattern, currentAlignment, currentAddress);
		if (result == -1) {
			out.println("Search term not found");
			currentPattern = null;
			currentAlignment = 1;
			currentAddress = -1;
			return false;
		} else {
			String address = "0x" + Long.toHexString(result);
			out.println("Match found at " + address);
			currentAddress = result;
			return true;
		}
	}
}
