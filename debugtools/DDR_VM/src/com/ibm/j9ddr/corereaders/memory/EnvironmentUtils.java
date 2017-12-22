/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import java.io.UnsupportedEncodingException;
import java.util.Properties;

/**
 * @author andhall
 *
 */
public class EnvironmentUtils
{
	/**
	 * Extracts environment data from the environ pointer
	 * @param proc Process whose environment is being extracted
	 * @param environPointer char ** environ value
	 * @return Environment properties
	 * @throws MemoryFault
	 */
	public static Properties readEnvironment(IProcess proc, long environPointer) throws MemoryFault
	{
		Properties env = new Properties();
		//Walk NULL terminated char **
		
		long slotCursor = environPointer;
		long stringPtr = proc.getPointerAt(slotCursor);
		
		while (0 != stringPtr) {
			//assemble the string at this address
			//XXX: does it make sense to treat these as ASCII chars?
			
			//Find length of string
			long ptr = stringPtr;
			
			while (proc.getByteAt(ptr) != 0) {
				ptr++;
			}
			
			byte[] byteBuffer = new byte[(int)(ptr - stringPtr)];
			proc.getBytesAt(stringPtr, byteBuffer);
			
			String pair = null;
			try {
				pair = new String(byteBuffer,"ASCII");
			} catch (UnsupportedEncodingException e) {
				throw new RuntimeException(e);
			}
			
			//now buffer is the x=y string
			int equal = pair.indexOf('=');
			if (equal != -1) {
				String variable = pair.substring(0, equal);
				String value = pair.substring(equal+1, pair.length());
				env.put(variable, value);
			}
			
			slotCursor += proc.bytesPerPointer();
			stringPtr = proc.getPointerAt(slotCursor);
		}
		
		return env;
	}
	
	/**
	 * Constructs a set of environment variable properties a the Windows environment variable string block.
	 * Documentation in MSDN:
	 *  The GetEnvironmentStrings function returns a pointer to a block of memory that contains the environment
	 *  variables of the calling process (both the system and the user environment variables). Each environment
	 *  block contains the environment variables in the following format:
	 *
	 * Var1=Value1\0
	 * Var2=Value2\0
	 * Var3=Value3\0
	 * ....
	 * VarN=ValueN\0\0
	 * 
	 * @param proc - process whose environment variables are being extracted
	 * @param environPtr - address of Windows environment strings memory block
	 * @return Properties object containing list of process environment variable names and values
	 * @throws MemoryFault
	 */
	public static Properties readEnvironmentStrings(IProcess proc, long environmentPtr) throws MemoryFault
	{
		Properties env = new Properties();
		
		if (environmentPtr == 0) {
			return env; // return an empty properties object 
		}
		
		long stringPtr = environmentPtr;
		while (proc.getByteAt(stringPtr) != 0) { // a null at the start of the string means we are done
			// Find the length of this environment variable string
			long ptr = stringPtr;
			while (proc.getByteAt(ptr) != 0) {
				ptr++;
			}
			
			// Allocate a buffer for it and copy in the string
			byte[] byteBuffer = new byte[(int)(ptr - stringPtr)];
			proc.getBytesAt(stringPtr, byteBuffer);
			
			// Convert to an ASCII string
			String pair = null;
			try {
				pair = new String(byteBuffer,"ASCII");
			} catch (UnsupportedEncodingException e) {
				throw new RuntimeException(e);
			}
			
			// Now contents of the buffer is the x=y string
			int equal = pair.indexOf('=');
			if (equal != -1) {
				String variable = pair.substring(0, equal);
				String value = pair.substring(equal+1, pair.length());
				env.put(variable, value);
			}
			
			stringPtr = ptr + 1; // hop over the null terminator to the next string
		}
		
		return env;
	}
}
