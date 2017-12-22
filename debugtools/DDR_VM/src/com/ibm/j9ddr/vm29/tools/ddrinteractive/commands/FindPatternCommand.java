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
import java.math.BigInteger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.types.UDATA;

public class FindPatternCommand extends Command 
{
	
	private final static int PATTERN_LENGHT = 1024;
	
	/**
	 * Constructor
	 */
	public FindPatternCommand()
	{
		addCommand("findpattern", "<pattern>", "search memory for a specific pattern");
	}
	
	/**
     * Prints the usage for the findpattern command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		CommandUtils.dbgPrint(out, "Usage: \n");
		CommandUtils.dbgPrint(out, "  !findpattern hexstring,alignment\n");
		CommandUtils.dbgPrint(out, "  !findpattern hexstring,alignment,startPtr\n");
		CommandUtils.dbgPrint(out, "  !findpattern hexstring,alignment,startPtr,bytesToSearch\n");
	}
	
	/**
	 * Run method for !findpattern extension.
	 * 
	 * @param command  !findpattern
	 * @param args	args passed by !findpattern extension.
	 * @param context Context
	 * @param out PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {			
			byte[] pattern = null;
			BigInteger bytesToSearch = CommandUtils.longToBigInteger(UDATA.MAX.longValue());
			int length = 0;
			if(args.length != 1) {
				printUsage(out);
				return;				
			}
			
			String[] realArgs = args[0].split(",");
			
			if(realArgs.length == 1) {
				CommandUtils.dbgError(out, "Error: must specify alignment\n");
				printUsage(out);
				return;
			}
			
			String hexstring = realArgs[0];
			int patternAlignment = Integer.parseInt(realArgs[1]);
			
			long startSearchFrom = 0;
		
			if(realArgs.length == 3) {
				startSearchFrom = CommandUtils.parsePointer(realArgs[2], J9BuildFlags.env_data64);
				bytesToSearch = bytesToSearch.subtract(CommandUtils.longToBigInteger(startSearchFrom));
			} else if(realArgs.length == 4) {
				startSearchFrom = CommandUtils.parsePointer(realArgs[2], J9BuildFlags.env_data64);
				
				bytesToSearch = CommandUtils.parseNumber(realArgs[3]);
				if (bytesToSearch.add(CommandUtils.longToBigInteger(startSearchFrom)).toString(CommandUtils.RADIX_HEXADECIMAL).length() > UDATA.SIZEOF ) {
					out.println("Warning: bytesToSearch value (" 
							+ realArgs[3] 
							+ ") is larger than the max available memory after the search start address (" 
							+ realArgs[2] 
							+ ").\n Pattern will be searched in all the remaining memory after the search start address");
					bytesToSearch = CommandUtils.longToBigInteger(UDATA.MAX.longValue()).subtract(CommandUtils.longToBigInteger(startSearchFrom));
				} 

			} else if(realArgs.length > 4) {
				CommandUtils.dbgError(out, "Error: too many arguments\n");
			}
			
			length = hexstring.length() / 2; 
			if( length > PATTERN_LENGHT) {
				CommandUtils.dbgPrint(out, String.format("Pattern is too long. Truncating to %d bytes\n", PATTERN_LENGHT));
				length = PATTERN_LENGHT;				
			}
			
			pattern = new byte[length];
			
			for (int i = 0; i < length; i++) {
				int hex1 = hexValue(hexstring.charAt(i * 2));
				int hex2 = hexValue(hexstring.charAt(i * 2 + 1));

				if ( (hex1 < 0) || (hex2 < 0) ) {
					CommandUtils.dbgError(out, "Error: non-hex value found in hex string\n");
					return;
				}

				pattern[i] = (byte) ((hex1 << 4) + hex2);
			}
			
			/* ensure that alignment is > 0 */
			if(patternAlignment == 0) {
				patternAlignment = 1;
			}
			CommandUtils.dbgPrint(out, String.format("Searching for %d bytes. Alignment = %d, start = %s, bytesToSearch = %s ...\n", length, patternAlignment, U8Pointer.cast(startSearchFrom).getHexAddress(), bytesToSearch.toString()));
			long result = dbgFindPatternInRange(context, pattern, patternAlignment, startSearchFrom, bytesToSearch);
			if (0 != result) {
				CommandUtils.dbgPrint(out, String.format("Result = %s\n", U8Pointer.cast(result).getHexAddress()));
			} else {
				CommandUtils.dbgPrint(out, String.format("Result = No Match Found.\n"));
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * This method takes a character value (case insensitive) and returns its hexadecimal equivalent
	 * @param c char
	 */
	int hexValue(char c)
	{
		if (c >= '0' && c <= '9') {
			return c - '0';
		} else if (c >= 'a' && c <= 'f') {
			return 10 + c - 'a';
		} else if (c >= 'A' && c <= 'F') {
			return 10 + c - 'A';
		} else {
			return -1;
		}
	}	
}
