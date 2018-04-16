/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.getClassVersionNumbers;

public class gcvn001
{
	public static native boolean checkVersionNumbers(int major, int minor);

	public boolean testGetClassVersion()
	{
		/** Versions of the hand compiled VersionedClass.java
		 * @see VersionedClass.java.dont_auto_compile
		 * Java SE 9 = 53 (0x35 hex),[3]
		 * Java SE 8 = 52 (0x34 hex),
		 * Java SE 7 = 51 (0x33 hex),
		 * Java SE 6.0 = 50 (0x32 hex),
		 * Java SE 5.0 = 49 (0x31 hex),
		 * JDK 1.4 = 48 (0x30 hex),
		 * JDK 1.3 = 47 (0x2F hex),
		 * JDK 1.2 = 46 (0x2E hex),
		 * JDK 1.1 = 45 (0x2D hex).
		 */
		int major = 52;
		int minor = 0;

		boolean ret = checkVersionNumbers(minor, major);

		return ret;

	}

	public String helpGetClassVersion()
	{
		return "Obtains the version of a class known to be compiled using a specific -target version";
	}

}
