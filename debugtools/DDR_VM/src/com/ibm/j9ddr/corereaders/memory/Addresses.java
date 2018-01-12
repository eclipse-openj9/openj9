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
package com.ibm.j9ddr.corereaders.memory;

/**
 * Contains static helpers for working with addresses as long.
 * 
 * Encapsulates 64bit unsigned maths using 64 bit signed longs
 * 
 * @see http://www.javamex.com/java_equivalents/unsigned_arithmetic.shtml
 * 
 * @author andhall
 *
 */
public class Addresses 
{

	/**
	 * 
	 * @param a
	 * @param b
	 * @return True is a is > b
	 */
	public static boolean greaterThan(long a, long b)
	{
		if (signBitsSame(a, b)) {
			return  a > b;
		} else {
			return a < b;
		}
	}
	
	public static boolean greaterThanOrEqual(long a, long b) {
		return greaterThan(a,b) || a == b;
	}
	
	/**
	 * 
	 * @param a
	 * @param b
	 * @return True if a is < b
	 */
	public static boolean lessThan(long a, long b)
	{
		if (signBitsSame(a, b)) {
			return  a < b;
		} else {
			return a > b;
		}
	}
	
	public static boolean lessThanOrEqual(long a, long b) {
		return lessThan(a,b) || a == b;
	}
	
	private static boolean signBitsSame(long a, long b)
	{
		boolean aSigned = a < 0;
		boolean bSigned = b < 0;
		
		return !(aSigned ^ bSigned);
	}

}
