/*
 * Copyright IBM Corp. and others 2001
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

import java.util.*;
import java.io.*;
import java.nio.charset.Charset;
import org.openj9.test.util.VersionCheck;

public class SysPropTest
{
	/* TestVa&#187;lue&#161; */
	static final byte[] expectedBytes = { (byte)'T', (byte)'e', (byte)'s', (byte)'t', (byte)'V', (byte)'a', (byte)187, (byte)'l', (byte)'u', (byte)'e', (byte)161};

	public static void main(String args[])
	{
		if (args.length == 0) {
			System.out.println("test failed");
			return;
		}
		String argEncoding = args[0];
		/* check -Dtestkey=TestVa?lue? */
		try {
			boolean isWindows = System.getProperty("os.name").contains("Windows");
			String osEncoding = "";
			String strTestProp;

			if (argEncoding.equals("DEFAULT")) {
				osEncoding = System.getProperty("os.encoding");
			}

			/* Windows converts from the platform default to UTF8 internally to the VM and
			 * sets os.encoding to UTF8. To replicate this behavior, on Windows use the default encoding
			 * and not the os.encoding.
			 */
			if ((osEncoding != null) && (osEncoding.length() != 0) && !isWindows) {
				strTestProp = new String(expectedBytes, osEncoding);
			} else {
				if ((argEncoding.equals("UTF-8") || argEncoding.equals("ISO-8859-1"))) {
					strTestProp = new String(expectedBytes, argEncoding);
				} else {
					strTestProp = new String(expectedBytes, System.getProperty("native.encoding", Charset.defaultCharset().name()));
				}
			}

			String strProp = System.getProperty("testkey");
			if (strProp == null || strTestProp.compareTo(strProp) != 0) {
				System.out.println("test failed");
				System.out.println("os.encoding: " + System.getProperty("os.encoding"));
				System.out.println("native.encoding: " + System.getProperty("native.encoding"));
				System.out.println("defaultCharset(): " + Charset.defaultCharset().name());
				System.out.println("file.encoding: " + System.getProperty("file.encoding"));
				System.out.print("strProp    : ");
				for (int i = 0; i < strProp.length(); i++) {
					System.out.print(Integer.toHexString(strProp.charAt(i)) + " ");
				}
				System.out.println();
				System.out.print("strTestProp: ");
				for (int i = 0; i < strTestProp.length(); i++) {
					System.out.print(Integer.toHexString(strTestProp.charAt(i)) + " ");
				}
				System.out.println();

			} else {
				System.out.println("test succeeded");
			}
		} catch (UnsupportedEncodingException e) {
			System.out.println("test failed");
			e.printStackTrace();
		}
	}
}
