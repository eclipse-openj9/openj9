/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.fieldwatch;

class TestDriver {
	public static int localInt = 9;
	public static long localLong = 8L;
	public static float localFloat = 7.89f;
	public static double localDouble = 5.67;
	public static Object myObject1 = new Object();
	public static Object myObject2 = new Object();

	public static void jitme_write(MyObject unresolvedObj)
	{
		if (unresolvedObj != null) {
			unresolvedObj.instanceIntField = localInt;
			unresolvedObj.instanceLongField = localLong;
			unresolvedObj.instanceSingleField = localFloat;
			unresolvedObj.instanceDoubleField = localDouble;
			unresolvedObj.instanceObjField = myObject1;

			MyObject.staticIntField = localInt;
			MyObject.staticLongField = localLong;
			MyObject.staticSingleField = localFloat;
			MyObject.staticDoubleField = localDouble;
			MyObject.staticObjField = myObject2;
		}
	}

	public static void jitme_read(MyObject unresolvedObj)
	{
		localInt = unresolvedObj.instanceIntField + MyObject.staticIntField;
		localLong = unresolvedObj.instanceLongField + MyObject.staticLongField;
		localDouble = unresolvedObj.instanceDoubleField + MyObject.staticDoubleField;
		localFloat = unresolvedObj.instanceSingleField + MyObject.staticSingleField;
		myObject1 = MyObject.staticObjField;
		myObject2 = unresolvedObj.instanceObjField;
	}
}
