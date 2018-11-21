/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package j9vm.test.clinit;

class GetStaticTestHelper {
	public static String i;

	static {
		GetStaticTest.write("bad");
		try {
			Thread.sleep(5000);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
		GetStaticTest.write("good");
	}
}

public class GetStaticTest {
	public static boolean passed = false;
	public static void write(String a) { GetStaticTestHelper.i = a; }
	public static String read() { return GetStaticTestHelper.i; }

	public static void test() {
		Thread t = new Thread() {
			public void run() {
				try {
					Thread.sleep(2000);
				} catch(Throwable t) {
					throw new RuntimeException(t);
				}
				String value = read();
				passed = value.equals("good");
			}
		};
		t.start();
		// If running count=0, force read() to be compiled and GetStaticTestHelper to be initialized
		read();
		try {
			t.join(); 
		} catch (Throwable e) {
			throw new RuntimeException(e);
		}
		if (!passed) {
			throw new RuntimeException("TEST FAILED");
		}
	}

	public static void main(String[] args) { GetStaticTest.test(); }
}
