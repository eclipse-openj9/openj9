/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
import java.math.BigInteger;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;


/**
 * @author andhall
 * 
 */
public class CommandUtils {
	private static final Logger logger = Logger.getLogger("com.ibm.j9ddr.tools.ddrinteractive");
	
	public static final BigInteger UDATA_MAX_64BIT = new BigInteger("18446744073709551615");
	public static final BigInteger UDATA_MAX_32BIT = new BigInteger("4294967295"); 
	
	public static final int RADIX_BINARY = 2;
	public static final int RADIX_DECIMAL = 10;
	public static final int RADIX_HEXADECIMAL = 16;
	public static final String HEX_SUFFIX = "0x";
	
	/**
	 * This method parses the given address string and return the address value in type long. 
	 * 
	 * @param input String representation of the address of a pointer. 
	 * @param is64bit True, if the platform where core file is generated is 64 bit platform. False, otherwise. 
	 * @return Address of the parsed pointer. Return 0, if any parse error occurs. 
	 * @throws DDRInteractiveCommandException
	 */
	public static long parsePointer(String input, boolean is64bit) throws DDRInteractiveCommandException 
	{
		BigInteger pointer = parseNumber(input);
		
		/* Check whether the parsed address is negative or not. */
		if (-1 == pointer.signum()) {
			throw new DDRInteractiveCommandException(input + " is not a valid address: Address can not be a negative number");
		}
		
		/* Check whether the parsed address exceeds the UDATA.MAX value or not for the platform that core generated */
		if (is64bit) {
			if (pointer.compareTo(UDATA_MAX_64BIT) == 1) {
				throw new DDRInteractiveCommandException(input + " is larger than the max available memory address: 0xFFFFFFFFFFFFFFFF (" + UDATA_MAX_64BIT + ")");
			}
		} else {
			if (pointer.compareTo(UDATA_MAX_32BIT) == 1) {
				throw new DDRInteractiveCommandException(input + " is larger than the max available memory address: 0xFFFFFFFF (" + UDATA_MAX_32BIT + ")");
			}
		}
	
		return pointer.longValue();
	}
	
	/**
	 * This method converts long type pointer into BigInteger instance. 
	 * Since long type pointer can be negative too, such values converted into positive BigInteger by treating the bits of long as UDATA. 
	 * 
	 * @param pointer long data type pointer. 
	 * @return BigInteger instance of the given pointer
	 */
	public static BigInteger longToBigInteger(long pointer) {
		BigInteger result;
		
		result = new BigInteger(Long.toBinaryString(pointer), RADIX_BINARY);
		
		
		return result;
	}
	
	/**
	 * This method parses given number into a BigInteger
	 * 
	 * @param number String representation of the given number to be parsed
	 * 
	 * @return BigInteger instance of the parsed number
	 * @throws DDRInteractiveCommandException
	 */
	public static BigInteger parseNumber(String number) throws DDRInteractiveCommandException {
		BigInteger result = null;
		
		if (null == number) {
			throw new DDRInteractiveCommandException("Parsed value is null");
		}
		
		/* Make sure there are no empty spaces at the beginning of the address input */
		number = number.trim();
		if (0 == number.length()) {
			throw new DDRInteractiveCommandException("Parsed value is empty. (zero length string)");
		}
		
		/* Check whether the parsed address is a valid hex or decimal number. */
		try {
			if (number.startsWith(HEX_SUFFIX)) {
				result = new BigInteger(number.substring(HEX_SUFFIX.length()), RADIX_HEXADECIMAL);
			} else {
				result = new BigInteger(number, RADIX_DECIMAL);
			}
		} catch (NumberFormatException nfe) {
			throw new DDRInteractiveCommandException(number + " is not a valid decimal or hexadecimal number.");
		}
		
		return result;
	}

	public static Logger getLogger() {
		return logger;
	}
	
	public static long followPointerFromStructure(Context context, String structureName, long structureAddress, String fieldName) throws MemoryFault, DDRInteractiveCommandException {
		
		if( structureAddress == 0 ) {
			throw new DDRInteractiveCommandException(new NullPointerException("Null " + structureName + " found."));
		}
		
		StructureDescriptor desc = StructureCommandUtil.getStructureDescriptor(structureName, context);
		
		
		for( FieldDescriptor f: desc.getFields()) {
			if( f.getDeclaredName().equals(fieldName)) {
				long offset = f.getOffset();
				long pointerAddress = structureAddress + offset;
				long pointer = context.process.getPointerAt(pointerAddress);
				return pointer;
			}
		}
		return 0;
	}
	
	public static long getOffsetForField(StructureDescriptor desc, String fieldName) throws com.ibm.j9ddr.NoSuchFieldException {
		for( FieldDescriptor f: desc.getFields()) {
			if( f.getDeclaredName().equals(fieldName)) {
				return f.getOffset();
			}
		}
		throw new com.ibm.j9ddr.NoSuchFieldException(desc.getName() + " does not contain a field named " + fieldName );
	}

	public static String getCStringAtAddress(IProcess process, long address) throws CorruptDataException {
		return getCStringAtAddress(process, address, Long.MAX_VALUE);
	}
	
	public static boolean fieldExists(Class class1, String string) {
		try {
			class1.getField(string);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchFieldException e) {
			return false;
		}
		return true;
	}
	
	public static String getCStringAtAddress(IProcess process, long address, long maxLength) throws CorruptDataException {
		int length = 0;
		while(0 != process.getByteAt(address + length) && length < maxLength) {
			length++;
		}
		byte[] buffer = new byte[length];
		process.getBytesAt(address, buffer);
		
		try {
			return new String(buffer,"UTF-8");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}
	}
	
	public static void dbgError(PrintStream out, String msg) 
	{
		dbgPrint(out, msg);
	}

	public static void dbgPrint(PrintStream out, String msg) 
	{
		out.append(msg);
	}
	
	public static void dbgPrint(PrintStream out, String msg, Object... args) 
	{		
		dbgPrint(out, String.format(msg, args));
	}		

	public static byte[] longToByteArray(long index, int size) 
	{
		byte[] bytearray = new byte[size];
		for (int k = 0; k < size; k++) {
			bytearray[k] = (byte) (index >>> (k * 8));
		}
		return bytearray;
	}
}
