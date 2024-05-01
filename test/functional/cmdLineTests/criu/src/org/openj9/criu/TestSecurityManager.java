/*
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.criu;

public class TestSecurityManager {

	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "setSMProgrammatically":
			testUnsupportedOperationExceptionSM();
			break;
		case "setSMCommandOption":
			System.out.println("TEST FAILED - -Djava.security.manager finished");
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}

	}

	@SuppressWarnings("removal")
	private static void testUnsupportedOperationExceptionSM() {
		try {
			System.setSecurityManager(null);
			System.out.println("TEST FAILED - setSecurityManager(null) finished");
		} catch (UnsupportedOperationException e) {
			System.out.println("TEST PASSED - setSecurityManager(null) throws UnsupportedOperationException");
		}

		try {
			System.setSecurityManager(new SecurityManager());
			System.out.println("TEST FAILED - setSecurityManager(new SecurityManager()) finished");
		} catch (UnsupportedOperationException e) {
			System.out.println(
					"TEST PASSED - setSecurityManager(new SecurityManager()) throws UnsupportedOperationException");
		}

		try {
			System.setSecurityManager(new MySecurityManager());
			System.out.println("TEST FAILED - setSecurityManager(new MySecurityManager()) finished");
		} catch (UnsupportedOperationException e) {
			System.out.println(
					"TEST PASSED - setSecurityManager(new MySecurityManager()) throws UnsupportedOperationException");
		}
	}

	@SuppressWarnings("removal")
	static class MySecurityManager extends SecurityManager {
	}
}
