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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;

public class J9XCommand extends Command 
{

	private static final int DEFAULT_LINES = 2;
	private static final int DEFAULT_BYTES_PER_LINE = 16;
	private static final String DEFAULT_SEPARATOR = ",";

	private static final char ASCIICodes[] = {
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		' ', '!', '\"', '#', '$', '%', '&', '\'',
		'(', ')', '*', '+', ',', '-', '.', '/',
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', ':', ';', '<', '=', '>', '?',
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
		'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
		'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
		'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
		'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
		'x', 'y', 'z', '{', '|', '}', '~', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.',
		'.', '.', '.', '.', '.', '.', '.', '.'
	};
	
	public J9XCommand()
	{
		addCommand("j9x", "<address>", "Hexdump <address>");
		addCommand("j9xx", "<address>", "Hexdump <address>");
	}
	

	/**
	 * Debug assist method. Display words from the core file start at this
	 * pointer.
	 * 
	 * @param words
	 */
	private void printHexMemoryDump(PrintStream out,  Context context, long addr, long bytesToPrint, int width, int numColumns, boolean doASCII) throws CorruptDataException {
		int column = 0;
		int asciiIndex = 0;
		StringBuffer asciiBuffer = new StringBuffer();

		if (numColumns == 0) {
			return;
		}

		if (((width & (width - 1)) != 0) || width == 0) {
			/* Width must be a power of two */
			return;
		}

		/* bytesToPrint must be a multiple of width */
		bytesToPrint -= bytesToPrint % width;
				
		while (bytesToPrint > 0) {
			if (column % numColumns == 0) {
				out.print(String.format("0x%08X : ", addr));
				asciiIndex = 0;
			}
			
			out.append(" ");
			
			switch (width) {
			case 1:
				out.print(String.format("%02x", context.process.getByteAt(addr)));
				break;
			case 2:				
				out.print(String.format("%04x", context.process.getShortAt(addr)));
				break;
			case 4:
				out.print(String.format("%08x", context.process.getIntAt(addr)));
				break;
			case 8:
				out.print(String.format("%016x", context.process.getLongAt(addr)));
				break;
			default:
				out.print(String.format("Invalid width specified\n"));
				return;
			}

			bytesToPrint -= width;
			
			if (doASCII) {
				for (int i = 0; i < width; i++) { 
					int asciiCode = ((int)context.process.getByteAt(addr + i)) & 0xFF;
					asciiBuffer.insert(asciiIndex++, ASCIICodes[asciiCode]);					
				}				
			}			
			
			if ((column % numColumns == numColumns - 1) || (bytesToPrint == 0)) {
				if(doASCII) {
					// Pad with spaces
					for (int i = 0; i < ((numColumns - 1) - column) * ((width * 2) + 1); i++) {
						out.append(" ");
					}
					out.append(" [ ");
					out.append(asciiBuffer.substring(0, asciiIndex));
					out.append(" ] ");
				}
				out.append("\n");
			}			
			
			addr += width;
			column = (column + 1) % numColumns;
		}
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.j9ddr.tools.ddrinteractive.ICommand#run(java.lang.String,
	 * java.io.StreamTokenizer, com.ibm.j9ddr.tools.ddrinteractive.Context)
	 */
	public void run(String command, String arguments[], Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (arguments.length == 0) {
			printUsage(out);
			out.println("Unexpected number of arguments.");
			return;
		}
		boolean isRange = false;
		String[] formattedArguments = arguments[0].split(DEFAULT_SEPARATOR);
		/* If the user has specified startAddress..endAddress then they want everything
		 * between those two locations. Alter the parameters to make the parsing logic below
		 * simpler.
		 * (This is fiddly but allows the user to use expressions in ranges if they want.)
		 */
		if( formattedArguments[0].contains("..") ) {
			String[] newArguments = new String[formattedArguments.length+1];
			String[] rangeArguments = formattedArguments[0].split("\\.\\.");
			if( rangeArguments.length != 2 ) {
				out.println("Incorrect range specified. Use '..' to separate two addresses in a range, e.g. !j9x address1..address2");
				return;
			}
			newArguments[0] = rangeArguments[0];
			newArguments[1] = rangeArguments[1];
			System.arraycopy(formattedArguments, 1, newArguments, 2, formattedArguments.length - 1 );
			formattedArguments = newArguments;
			isRange = true;
		}

		long startAddress;
		try {
			startAddress = ExpressionEvaluator.eval(formattedArguments[0]);
		} catch (ExpressionEvaluatorException e) {
			out.println("Invalid start address specified.");
			printUsage(out);
			return;
		}

		long bytesToPrint = DEFAULT_BYTES_PER_LINE * DEFAULT_LINES;

		int bytesPerPointer = context.process.bytesPerPointer();
		int numColumns = 0;

		final int argCount = formattedArguments.length;
		switch (argCount) {
		case 4:
			// User specified a number of columns, plus preceding arguments.
			numColumns = Integer.valueOf(formattedArguments[3]);
			if( numColumns < 1 )  {
				out.println("Invalid number of columns specified.");
				printUsage(out);
				return;
			}
		case 3:
			// User specified a number of bytes per pointer, plus preceding arguments.
			bytesPerPointer = Integer.valueOf(formattedArguments[2]);
			boolean validSize = (bytesPerPointer == 1) || (bytesPerPointer == 2) || (bytesPerPointer == 4) || (bytesPerPointer == 8);
			if (validSize != true) {
				out.println("Invalid pointer size specified.");
				printUsage(out);
				return;
			}
		case 2:
			// User specified a count or end address, plus a preceding a start address.
			try {
				if( !isRange ) {
					if(formattedArguments[1].startsWith("-")) {
						bytesToPrint = ExpressionEvaluator.eval(formattedArguments[1].substring(1), CommandUtils.RADIX_HEXADECIMAL);
						startAddress = startAddress - bytesToPrint;
					} else {
						bytesToPrint = ExpressionEvaluator.eval(formattedArguments[1], CommandUtils.RADIX_HEXADECIMAL);
					}
				} else {
					long endAddress = ExpressionEvaluator.eval(formattedArguments[1]);
					bytesToPrint = endAddress - startAddress;
				}
			} catch (ExpressionEvaluatorException e) {
				if( !isRange ) {
					out.println("Invalid count specified.");
				} else {
					out.println("Invalid end address specified.");
				}
				printUsage(out);
				return;
			}
		case 1:
			// Only a start address specified.
			break;
		default:
			printUsage(out);
			return;
		}		
		
		/* Set the number of columns if it wasn't specified. */
		if( numColumns == 0 ) {
			numColumns = DEFAULT_BYTES_PER_LINE / bytesPerPointer;
		}
		
		bytesToPrint = bytesToPrint - bytesToPrint % bytesPerPointer;

		try {
			printHexMemoryDump(out, context, startAddress, bytesToPrint, bytesPerPointer, numColumns, true);
		} catch (Exception e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	public String[] splitArguments(String [] args) {
		return splitArguments(args, DEFAULT_SEPARATOR);
	}

	public String[] splitArguments(String [] args, String separators) {
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < args.length; i++) {
			sb.append(args[i]);
		}
		String arg = sb.toString();
		
		return arg.split(separators);
	}
	
	private void printUsage(PrintStream out) 
	{
		out.append("Usage:\n");
		out.append("  !j9x address                                              : display bytes at address\n");
		out.append("  !j9x address,count                                        : display count bytes from address\n");
		out.append("  !j9x address,-count                                       : display count bytes backwards from address\n");
		out.append("  !j9x startAddress..endAddress                             : display bytes between startAddress and endAddress\n");
		out.append("  !j9x [address,count|startAddress..endAddress],[1|2|4|8]   : set word size in output to 1,2,4 or 8 bytes\n");
		out.append("  !j9x [address,count|startAddress..endAddress],[1|2|4|8],n : format output to have n columns of words per row\n");
		out.append("The address, count, startAddress and endAddress parameters can be decimal or hexadecimal ('0x' prefix indicates hexadecimal, 'd' prefix indicates decimal).\n");
		out.append("If no prefix is specified the 'count' parameter is hexadecimal by default, all other parameters default to decimal.\n");
		out.append("For example:\n");
		out.append("  !j9x 0x7fa54800d4cf,20");
		out.append("  !j9x 0x7fa54800d4cf..0x7fa54800d4ef");
		out.append("  !j9x 0x7fa54800d4cf,d32,8,4");
		out.append("The address, count, startAddress and endAddress parameters can be expressions, for example:\n");
		out.append("  !j9x 0x7fa54800d4cf+32\n");
		out.append("  !j9x 0x7fa54800d4cf+0x20\n");
		out.append("  !j9x 0x7fa54800d4cf+(32*4)\n");
	}

}
