/*
 * Copyright IBM Corp. and others 2026
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
package j9vm.test.jnichk;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class DefineClassLoader extends Test {

	public static void main(String[] args) throws IOException {
		if (args.length != 1) {
			throw new IllegalArgumentException("Expected bootstrap, custom, or invalid");
		}

		Object loader;
		switch (args[0]) {
		case "bootstrap":
			loader = null;
			break;
		case "custom":
			loader = new ClassLoader(null) { };
			break;
		case "invalid":
			loader = new Object();
			break;
		default:
			throw new IllegalArgumentException("Unknown test: " + args[0]);
		}

		Class<?> defined = defineClass(loader, classBytes());
		if ("invalid".equals(args[0])) {
			throw new AssertionError("The error was not detected immediately.");
		}
		if (defined == null) {
			throw new AssertionError("DefineClass returned null");
		}
		if (defined.getClassLoader() != loader) {
			throw new AssertionError("DefineClass used the wrong defining loader");
		}
		if (!"j9vm.test.jnichk.DefineClassTarget".equals(defined.getName())) {
			throw new AssertionError("DefineClass returned the wrong class");
		}
		System.out.println("DefineClass " + args[0] + " passed");
	}

	private static byte[] classBytes() throws IOException {
		// Read the class file without loading it through the application loader.
		try (InputStream input = DefineClassLoader.class.getResourceAsStream("DefineClassTarget.class")) {
			if (input == null) {
				throw new IOException("Missing DefineClassTarget.class");
			}
			ByteArrayOutputStream output = new ByteArrayOutputStream();
			byte[] buffer = new byte[1024];
			int count;
			while ((count = input.read(buffer)) != -1) {
				output.write(buffer, 0, count);
			}
			return output.toByteArray();
		}
	}

	private static native Class<?> defineClass(Object loader, byte[] bytes);
}

// A top-level class avoids dependencies on the application-loaded test class.
class DefineClassTarget {
}
