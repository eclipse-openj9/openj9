/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
		addCommand("hexdump", "<hex address>", "outputs a section of memory in hexadecimal, ascii and ebcdic");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		doCommand(args);
	}
	
	public void doCommand(String[] args){
		StringBuffer stringBuffer = new StringBuffer();
		ImageAddressSpace imageAddressSpace = ctx.getAddressSpace();
		if(null == imageAddressSpace) {
			out.println("Could not find an address space which contains a process in this core file");
			return;
		}
		long addressInDecimal = 0;
		int numBytesToPrint = 16*16; //dump 256 bytes by default
		int asciiIndex = 0;
		
		if (args.length != 0){
			Long address = Utils.longFromString(args[0]);
			if(null == address){
				out.println("Specified address is invalid");
				return;
			}
			addressInDecimal = address.longValue();
		}
		else
		{
			out.println("\"hexdump\" requires at least an address parameter");
			return;
		}
		
		if (args.length > 1){
			try{
				numBytesToPrint = Integer.parseInt(args[1]);
			}catch(NumberFormatException nfe){
				out.println("Specified length is invalid");
				return;
			}
		}
		
		ImagePointer imagePointerBase = imageAddressSpace.getPointer(addressInDecimal);
		boolean is_zOSdump = false;
		try {
			is_zOSdump = ctx.getImage().getSystemType().toLowerCase().indexOf("z/os") >= 0;
		} catch (DataUnavailable e1) {
			// unable to get the dump OS type, continue without the additional zOS EBCDIC option
		} catch (CorruptDataException e1) {
			// unable to get the dump OS type, continue without the additional zOS EBCDIC option
		}
		
		String asciiChars = "";
		String ebcdicChars = "";
		
		long i; 
		for (i = 0; i < numBytesToPrint; i++){
			ImagePointer imagePointer = imagePointerBase.add(i);
			//stringBuffer.append(Long.toHexString(addressInDecimal+i)+":\t");
			//stringBuffer.append(Long.toHexString(imagePointer.getAddress())+":\t");
			try {
				byte byteValue = imagePointer.getByteAt(0);
				asciiIndex = byteValue & 0xFF;

				String rawHexString = Integer.toHexString(asciiIndex);
				String fixedHexString = fixHexStringLength(rawHexString);
				
				String hexText = fixedHexString;
				if (0 == i%4){
					hexText = " "+hexText;
				}
				
				if (0 == i%16){
					hexText  = "\n"+Long.toHexString(imagePointer.getAddress())+":"+hexText;
					asciiChars = "  |";
					ebcdicChars = " |";
				}
				
				stringBuffer.append(hexText);
				
				asciiChars += Utils.byteToAscii.substring(asciiIndex,asciiIndex+1);
				if (15 == i%16 && i != 0){
					asciiChars += "|";
					stringBuffer.append(asciiChars);
				}
				if (is_zOSdump){
					// for zOS dumps, output additional EBCDIC interpretation of the memory byes
					ebcdicChars += Utils.byteToEbcdic.substring(asciiIndex,asciiIndex+1);
					if (15 == i%16 && i != 0){
						ebcdicChars += "|";
						stringBuffer.append(ebcdicChars);
					}
				}
			}catch (MemoryAccessException e) {
				out.println("Address not in memory - 0x" 
						+ Long.toHexString(imagePointer.getAddress()));
				return;
			}catch (CorruptDataException e) {
				out.println("Dump data is corrupted");
				return;
			}
		}
		
		long undisplayedBytes = 16 - i%16; 
		if (16 != undisplayedBytes){
			stringBuffer.append(padSpace(undisplayedBytes, asciiChars));
			if (is_zOSdump){
				// Add padding and output the remaining EBCDIC characters
				for (int j = 0; j < undisplayedBytes; j++){
					ebcdicChars = " " + ebcdicChars;
				}
				stringBuffer.append(" " + ebcdicChars);
			}
		}
		stringBuffer.append("\n");
		out.println(new String(stringBuffer));
		
		/*properties.put(Utils.CURRENT_MEM_ADDRESS, 
				Long.toHexString(addressInDecimal+numBytesToPrint));*/
		ctx.getProperties().put(Utils.CURRENT_MEM_ADDRESS, Long.valueOf(addressInDecimal));
		ctx.getProperties().put(Utils.CURRENT_NUM_BYTES_TO_PRINT, Integer.valueOf(numBytesToPrint));
	}
	
	private String padSpace(long undisplayedBytes, String asciiChars){
		for (int i = 0; i < 2*undisplayedBytes + undisplayedBytes/4; i++){
			asciiChars = " " + asciiChars;
		}
		return asciiChars;
	}
	
	private String fixHexStringLength(String rawHexString){
		int length = rawHexString.length();
		if (1 == length){
			return ("0" + rawHexString);
		}
		else if (2 == length){
			return rawHexString;
		}
		else if (8 == length){
			return rawHexString.substring(6, 8);
		}
		else{
			return "ERROR fixHexStringLength";
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("outputs a section of memory in hexadecimal, ascii and ebcdic\n\n" + 
				"parameters: <hex_address> <bytes_to_print>\n\n" +
				"outputs <bytes_to_print> bytes of memory contents " +
				"starting from <hex_address>, ebcdic output is provided for z/OS dumps"
				);
		
	}
}
