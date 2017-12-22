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
package com.ibm.j9ddr.corereaders.debugger;

import java.util.Comparator;
import java.util.SortedMap;
import java.util.TreeMap;


abstract class JniRegisters 
{
	
	SortedMap<String, Number> _registers = new TreeMap<String, Number>(new RegisterComparator());	
	
	/**
	 * Find the number of registers on the target
	 * 
	 * @param tid
	 *            thread id
	 * @return number of registers
	 */
	public static native long getNumberRegisters(long tid);

	/**
	 * Returns name of a register
	 * 
	 * @param index
	 *            register number
	 * @return name of the register depend on its processor architecture.
	 */
	public static native String getRegisterName(long index);

	public static native Number getRegisterValue(long tid, long index);
	
	/**
	 * Fetch current register values for the top (most recent) frame on the stack 
	 * 
	 * @param tid	Thread ID
	 * @return	true on success
	 */
	private native boolean fetchRegisters(long tid);
	
	protected abstract long getStackPointer();
	protected abstract long getInstructionPointer();
	protected abstract long getLinkRegister();
	protected abstract long getBasePointer();
	
	
	public int size()
	{
		return _registers.size();
	}
	
	public void setRegister(String name, long value)
	{
		_registers.put(new String(name), new Long(value));
	}
	
	public SortedMap<String, Number> readRegisters(long tid)
	{		
		/* call a native that sets all registers in this instance via setRegister() */
		fetchRegisters(tid);		
		return _registers;
	}
	
	public SortedMap<String, Number> getRegisters()
	{
		return _registers;
	}
	
	public class RegisterComparator implements Comparator<String> {
		public int compare(String s1, String s2) {
			// Pad trailing single digit register names, eg gpr1 to gpr01 to sort nicely
			int last = s1.length()-1;
			if (last >= 1) {
				if (Character.isDigit(s1.charAt(last)) && Character.isLetter(s1.charAt(last-1))) {
					s1 = s1.substring(0,last) + "0" + s1.substring(last);
				}
			}
			last = s2.length()-1;
			if (last >= 1) {
				if (Character.isDigit(s2.charAt(last)) && Character.isLetter(s2.charAt(last-1))) {
					s2 = s2.substring(0,last) + "0" + s2.substring(last);
				}
			}
			return s1.compareTo(s2);
		}
	}
}
