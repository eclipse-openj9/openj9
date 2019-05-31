/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class FindCommand extends BaseJdmpviewCommand{
	
	static final int text = 1;
	static final int binary = 2;
	
	/*args for the find command must be comma separated
	 * e.g. find cpu,0,21665f64,64,16,30
	 */	
	FindAttribute findAtt = new FindAttribute();
	StringBuffer sb = new StringBuffer();
	ArrayList matches = new ArrayList();
	
	{
		addCommand("find", "", "searches memory for a given string. Please run \"help find\" for details.");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 1) {
			String line = args[0];
			if(line.endsWith(",")) {
				//in order for the split to work there needs to always be a last parameter present. If it is missing we can default to only displaying the first match.
				line += "1";
			}
			String[] params = line.split(",");
			doCommand(params);	
		} else {
			out.println("\"find\" takes a set comma separated parameters with no spaces. Please run \"help find\" for details.");
		}
	}
	
	public void doCommand(String[] params){
		sb = new StringBuffer();
		matches.clear();
		if (!isParametersValid(params)) return;
		determineModeFromPattern();
		if (!parseParams(params)) return;
		Iterator imageSections = ctx.getAddressSpace().getImageSections();
		
		while(imageSections.hasNext()){
			if (matches.size() > findAtt.numMatchesToDisplay) break;
			ImageSection imageSection = (ImageSection)imageSections.next();
			if (scanImageSection(imageSection)) break;
		}
		if (matches.size() > 0) 
			findAtt.lastMatch = ((Long)matches.get(matches.size()-1)).longValue();
		ctx.getProperties().put(Utils.FIND_ATTRIBUTES, findAtt);
		doPrint();
		if (matches.size() > 0) 
			printLastMatchContent();
		
		//preppend "0x" back to pattern for findnext command 
		restoreHexPrefix();
	}

	private void restoreHexPrefix(){
		if (findAtt.mode == binary)
			findAtt.pattern = "0x" + findAtt.pattern; 
	}
	
	private void printLastMatchContent(){
		//forward onto the hexdump command
		ctx.execute("hexdump" + " 0x" 
				+ Long.toHexString(findAtt.lastMatch) + " "
				+ findAtt.numBytesToPrint, out);
	}
	
	private void doPrint(){
		int size = matches.size();
		if(0 == size){
			sb.append("No matches found.\n");
		}
		else{
			int limit = Math.min(findAtt.numMatchesToDisplay, size);
			for(int i = 0; i < limit; i++){
				long match = ((Long)matches.get(i)).longValue();
				sb.append("#" + i + ": " + "0x" + Long.toHexString(match) + "\n");
			}
		}
		out.print(new String(sb));
	}
	
	private boolean scanImageSection(ImageSection imageSection){
		long imageStartAddress = imageSection.getBaseAddress().getAddress();
		long imageEndAddress = imageStartAddress + imageSection.getSize() - 1;
		if (findAtt.startAddress < imageStartAddress && findAtt.endAddress < imageStartAddress){
			return false;
		}
		else if(findAtt.startAddress > imageEndAddress && findAtt.endAddress > imageEndAddress){
			return false;
		}
		else if(findAtt.startAddress >= imageStartAddress && findAtt.endAddress <= imageEndAddress){
			return scanRegion(findAtt.startAddress, findAtt.endAddress, imageSection);
		}
		else if(findAtt.startAddress <= imageStartAddress && findAtt.endAddress <= imageEndAddress 
				&& findAtt.endAddress >= imageStartAddress){
			return scanRegion(imageStartAddress, findAtt.endAddress, imageSection);
		}
		else if(findAtt.startAddress <= imageEndAddress && findAtt.startAddress >= imageStartAddress 
				&& findAtt.endAddress >= imageEndAddress){
			return scanRegion(findAtt.startAddress, imageEndAddress, imageSection);
		}
		else{
			return scanRegion(imageStartAddress, imageEndAddress, imageSection);
		}
	}
	
	private boolean scanRegion(long start, long end, ImageSection imageSection){
		ImagePointer imagePointer = imageSection.getBaseAddress();
		long i;
		if (0 != start%findAtt.boundary) {
			i = start - start%findAtt.boundary + findAtt.boundary;
		} else {
			i = start;
		}
		int patternLength = findAtt.length();
		byte[] bytes = findAtt.getBytes();
		
		for(; i <= end; i+=findAtt.boundary){
			int j;
			for(j = 0; j < patternLength; j++){
				byte oneByte = bytes[j];
				try {
					if (getByteFromImage(imagePointer, i+j) == oneByte){
						continue;
					} else {
						break;
					}

				} catch (MemoryAccessException mae) {
					return false;
				}
			}
			if (j >= patternLength){
				matches.add(Long.valueOf(i));
				if (matches.size() == findAtt.numMatchesToDisplay) 
					return true;
			}
		}
		return false;
	}
	
	private byte getByteFromImage(ImagePointer imagePointer, long address) throws MemoryAccessException {
		long imagePointerBaseAddress = imagePointer.getAddress();
		try{
			return imagePointer.getByteAt(address-imagePointerBaseAddress);
		}catch(CorruptDataException cde){
			return 0;
		}
	}
	
	private boolean parseParams(String[] params) {
		if (params[1].equals("")) {
			findAtt.startAddress = 0;
		} else {
			findAtt.startAddress = Utils.longFromString(params[1]).longValue();
		}
		
		if (params[2].equals("")) {
			//TODO how to convert to "unsigned" long
			findAtt.endAddress = Long.MAX_VALUE;
		} else {
			findAtt.endAddress = Utils.longFromString(params[2]).longValue();
		}
		
		if (findAtt.startAddress > findAtt.endAddress) {
			out.println("start address must not be greater than end address");
			return false;
		}
		
		if (params[3].equals("")) {
			findAtt.boundary = 1;
		} else {
			findAtt.boundary = Integer.parseInt(params[3]);
			if (findAtt.boundary <= 0) {
				out.println("memory boundary must be a positive non-zero value");
				return false;
			}
		}
		
		if (params[4].equals("")) {
			findAtt.numBytesToPrint = 256;
		} else {
			findAtt.numBytesToPrint = Integer.parseInt(params[4]);
			if (findAtt.numBytesToPrint < 0) {
				out.println("bytes to print must be a non-negative value");
				return false;
			}
		}
		
		if (params[5].equals("")) {
			findAtt.numMatchesToDisplay = 1;
		} else {
			findAtt.numMatchesToDisplay = Integer.parseInt(params[5]);
			if (findAtt.numMatchesToDisplay < 0) {
				out.println("matches to display must be a non-negative value");
				return false;
			}
		}
		return true;
	}
	
	private void determineModeFromPattern(){
		if (findAtt.pattern.startsWith("0x")) {
			findAtt.pattern = findAtt.pattern.substring(2);
			findAtt.mode = binary;
			alignBits();
		}
		else{
			findAtt.mode = text;
		}
	}
	
	private void alignBits(){
		int patternLength = findAtt.pattern.length();
		if (0 != patternLength%2){
			findAtt.pattern = "0" + findAtt.pattern;
		}
	}
	
	private boolean isParametersValid(String[] params){
		if (6 != params.length){
			out.println("incorrect number of parameters");
			return false;
		}
		findAtt.pattern = params[0];
		if (findAtt.pattern.equals("")) {
			out.println("missing search pattern string");
			return false;
		}
		return true;
	}
	
	class FindAttribute{
		String pattern;
		long startAddress, endAddress;
		int boundary, numBytesToPrint, numMatchesToDisplay;
		long lastMatch;
		int mode;
		
		public int length() {
			if (mode == binary) {
				return pattern.length() / 2;
			} else {
				return pattern.length();
			}
		}
		
		public byte[] getBytes() {
			if (mode == text) {
				return pattern.getBytes();
			} else {
				byte[] result = new byte[length()];
				for (int i = 0; i < length(); i++) {
					result[i] = (byte)Integer.parseInt(pattern.substring(i*2, i*2+2), 16);
				}
				return result;
			}
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("searches memory for a given string\n\n" +
				"parameters, comma separated: " + 
				"<pattern>,<start_address>,<end_address>,<memory_boundary>,<bytes_to_print>,<matches_to_display>\n\n" +
				"the find command searches for <pattern> in the memory segment from <start_address> to <end_address> inclusive, " +
				"matching only addresses that start at the specified <memory_boundary>, for example 1,2,4 or 8 bytes. It " +
				"displays <bytes_to_print> bytes from the final match, and lists the first <matches_to_display> matches found." +
				"\n example: find J9,,,,64,3"
				);
		
	}
}
