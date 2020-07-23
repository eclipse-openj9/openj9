/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package j9vm.test.jnichk;

public class PassingFieldID extends Test {

	private final static char staticCharField = 's';
	private final char instanceCharField = 'i';
	private final static Object staticObjectField = new Object();
	private final Object instanceObjectField = new Object();
	
	public static void main(String[] args) {
		new PassingFieldID().runTests(args);
	}
	
	void runTests(String[] args) {
		if (args.length == 0) {
			System.out.println("No testcase specified!");
		} else {
			switch (args[0]) {
			case "testToReflectedFieldExpectedStaticGotNonStatic":
				testToReflectedFieldExpectedStaticGotNonStatic();
				break;
			case "testToReflectedFieldExpectedNonStaticGotStatic":
				testToReflectedFieldExpectedNonStaticGotStatic();
				break;
			case "testGetCharFieldPassedStaticID":
				testGetCharFieldPassedStaticID();
				break;
			case "testGetStaticCharFieldPassedNonStaticID":
				testGetStaticCharFieldPassedNonStaticID();
				break;
			case "testSetObjectFieldPassedStaticID":
				testSetObjectFieldPassedStaticID();
				break;
			case "testSetStaticObjectFieldPassedNonStaticID":
				testSetStaticObjectFieldPassedNonStaticID();
				break;
			default:
				System.out.println("The testcase string is not recognized!");
				System.exit(-1);
			}
		}
		System.out.println("The error was not detected immediately.");
		System.exit(-1);
	}
	
	private native void testToReflectedFieldExpectedStaticGotNonStatic();
	private native void testToReflectedFieldExpectedNonStaticGotStatic();
	private native void testGetCharFieldPassedStaticID();
	private native void testGetStaticCharFieldPassedNonStaticID();
	private native void testSetObjectFieldPassedStaticID();
	private native void testSetStaticObjectFieldPassedNonStaticID();
}
