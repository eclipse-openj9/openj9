/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.testclasses;

/**
 * A test class which has ConstantValue attribute in its field_info.
 * 
 * @author ashutosh
 */
public class ConstantValueTest {
	final static int iConst = 1;
	final static float fConst = 2.0f;
	final static long lConst = 10l;
	final static double dConst = 20.0d;
	final static String sConst = "ConstantString";
	final static ConstantValueTest obj = new ConstantValueTest();
	
	public static long foo() {
		return 10l;	/* force cp entry used by ldc to be same as used by ConstantValue attribute */
	}
	public static double bar() {
		return 20.0d;	/* force cp entry used by ldc to be same as used by ConstantValue attribute */
	}
}
