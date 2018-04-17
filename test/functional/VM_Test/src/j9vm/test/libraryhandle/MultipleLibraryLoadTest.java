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
package j9vm.test.libraryhandle;

import java.math.*;

public class MultipleLibraryLoadTest {

private static native String vmVersion();	// declared in j9ben

public static void main(String[] args) {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {
			System.out.println("Problem opening JNI library first time");
			e.printStackTrace();
			throw new RuntimeException();
		}
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {
			System.out.println("Problem opening JNI library second time");
			e.printStackTrace();
			throw new RuntimeException();
		}

		/*
		 *	we know that BigInteger has a library named j9intXX and that it is loaded in
		 *	the "system" loader.  Trying again here will cause a failure.
		 */
		new BigInteger("7");
		try {
			System.loadLibrary("j9int" + System.getProperty("com.ibm.oti.vm.library.version", "12"));
		} catch (UnsatisfiedLinkError e) {
			System.out.println("Caught expected UnsatisfiedLinkError.");
			return;
		}
		System.out.println("Able to open BigInteger library in different classLoader");
		throw new RuntimeException();
	}
	
}
