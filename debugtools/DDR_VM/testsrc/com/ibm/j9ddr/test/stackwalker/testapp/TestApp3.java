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
package com.ibm.j9ddr.test.stackwalker.testapp;

/**
 * 
 * Generates a StackOverFlowError - which generates a JIT resolve frame. 
 * 
 * Should be run with -Xdump:system:events=systhrow,filter=java/lang/StackOverflowError
 * 
 * @author andhall
 *
 */
public class TestApp3
{

	private static void loop(Object o)
	{
		double a1 = 0;
		double a2 = 0;
		double a3 = 0;
		double a4 = 0;
		double a5 = 0;
		double a6 = 0;
		double a7 = 0;
		double a8 = 0;
		double a9 = 0;
		double a10 = 0;
		double a11 = 0;
		double a12 = 0;
		double a13 = 0;
		double a14 = 0;
		double a15 = 0;
		double a16 = 0;
		double a17 = 0;
		double a18 = 0;
		double a19 = 0;
		double a20 = 0;
		loop(new Object());
		
		
		a1++;
		a2++;
		a3++;
		a4++;
		a5++;
		a6++;
		a7++;
		a8++;
		a9++;
		a10++;
		a11++;
		a12++;
		a13++;
		a14++;
		a15++;
		a16++;
		a17++;
		a18++;
		a19++;
		a10++;
		
		System.err.println("" + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20);
		
		o.toString();
	}
	
	public static void main(String args[])
	{
		loop(new Object());
	}
}
