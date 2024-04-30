/*
 * Copyright IBM Corp. and others 2021
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

import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.*;

/**
 * @file GetConsoleCharset.java
 * @brief Compare the System.err or System.out Charset name to a provided name and
 * return the result via the exit code, 0 meaning there is a match.
 */

public class GetConsoleCharset {
	public static void main(String[] args) throws Exception {
		OutputStream stream;
		if (args.length < 2) {
			System.out.println("Provide two arguments, the first is either 'out' or 'err', and the second is the expected Charset name.");
			System.exit(1);
		}
		if ("out".equals(args[0])) {
			stream = System.out;
		} else {
			stream = System.err;
		}
		Field charOutField = PrintStream.class.getDeclaredField("charOut");
		charOutField.setAccessible(true);
		String encoding = ((OutputStreamWriter)charOutField.get(stream)).getEncoding();
		System.out.println("System." + args[0] + " encoding: " + encoding);
		if (args.length > 2) {
			FileOutputStream out = new FileOutputStream("GetConsoleCharset-result.txt");
			out.write(encoding.getBytes("ISO-8859-1"));
			out.close();
		}
		if (encoding.equals(args[1])) {
			System.out.println("SUCCESS");
			System.exit(0);
		}
		System.out.println("FAILED");
		System.exit(1);
	}
}
