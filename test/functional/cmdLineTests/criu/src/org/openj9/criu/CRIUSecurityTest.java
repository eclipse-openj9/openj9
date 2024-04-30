/*
 * Copyright IBM Corp. and others 2022
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

import java.io.File;
import java.io.PrintStream;
import java.nio.file.Paths;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.Provider;
import java.security.Security;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.NoSuchPaddingException;

public class CRIUSecurityTest {

	public static void main(String args[]) {
		System.out.println("Pre-checkpoint");

		// Make sure the only provider available during checkpoint is CRIUSEC.
		Provider[] providerList = Security.getProviders();
		assertEquals("Security provider list length", providerList.length, 1);
		assertEquals("Security provider name", providerList[0].getName(), "CRIUSEC");

		// Attempt to get an AES Cipher object for encryption, it should throw an exception.
		try {
			Cipher.getInstance("AES", "SunJCE");
			error("Getting an AES Cipher did not throw an exception");
		} catch (NoSuchAlgorithmException | NoSuchProviderException e) {
			// expected
		} catch (NoSuchPaddingException e) {
			error(e.toString());
		}

		CRIUTestUtils.checkPointJVM(Paths.get("cpData"));
		System.out.println("Post-checkpoint");
	}

	private static void assertEquals(String name, int value, int expected) {
		if (expected != value) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

	private static void assertEquals(String name, String value, String expected) {
		if (!expected.equals(value)) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

	private static void error(String msg) {
		System.out.println("ERR: " + msg);
	}
}
