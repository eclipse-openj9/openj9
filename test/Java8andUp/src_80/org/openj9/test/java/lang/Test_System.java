package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.openj9.test.support.Support_Exec;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.io.Writer;
import java.security.Permission;
import java.util.Enumeration;
import java.util.Properties;

@Test(groups = { "level.sanity" })
public class Test_System {

	// Properties orgProps = System.getProperties();
	static boolean flag = false;
	static boolean ranFinalize = false;

	/**
	 * @tests java.lang.System#setIn(java.io.InputStream)
	 */
	@Test
	public void test_setIn() {
		InputStream orgIn = System.in;
		InputStream in = new ByteArrayInputStream(new byte[0]);
		System.setIn(in);
		AssertJUnit.assertTrue("in not set", System.in == in);
		System.setIn(orgIn);
	}

	/**
	 * @tests java.lang.System#setOut(java.io.PrintStream)
	 */
	@Test
	public void test_setOut() {
		PrintStream orgOut = System.out;
		PrintStream out = new PrintStream(new ByteArrayOutputStream());
		System.setOut(out);
		AssertJUnit.assertTrue("out not set", System.out == out);
		System.setOut(orgOut);
	}

	/**
	 * @tests java.lang.System#setErr(java.io.PrintStream)
	 */
	@Test
	public void test_setErr() {
		PrintStream orgErr = System.err;
		PrintStream err = new PrintStream(new ByteArrayOutputStream());
		System.setErr(err);
		AssertJUnit.assertTrue("err not set", System.err == err);
		System.setErr(orgErr);
	}

	/**
	 * @tests java.lang.System#arraycopy(java.lang.Object, int,
	 *        java.lang.Object, int, int)
	 */
	@Test
	public void test_arraycopy() {
		// Test for method void java.lang.System.arraycopy(java.lang.Object,
		// int, java.lang.Object, int, int)
		Integer a[] = new Integer[20];
		Integer b[] = new Integer[20];
		int i = 0;
		while (i < a.length) {
			a[i] = new Integer(i);
			++i;
		}
		System.arraycopy(a, 0, b, 0, a.length);
		for (i = 0; i < a.length; i++)
			AssertJUnit.assertTrue("Copied elemets incorrectly", a[i].equals(b[i]));

		/* Non primitive array types don't need to be identical */
		String[] source1 = new String[] { "element1" };
		Object[] dest1 = new Object[1];
		System.arraycopy(source1, 0, dest1, 0, dest1.length);
		AssertJUnit.assertTrue("Invalid copy 1", dest1[0] == source1[0]);

		/* [PR 109441] System.arraycopy throws class cast exception */
		char[][] source = new char[][] { { 'H', 'e', 'l', 'l', 'o' },
				{ 'W', 'o', 'r', 'l', 'd' } };
		char[][] dest = new char[2][];
		System.arraycopy(source, 0, dest, 0, dest.length);
		AssertJUnit.assertTrue("Invalid copy 2", dest[0] == source[0]
				&& dest[1] == source[1]);
	}

	/**
	 * @tests java.lang.System#currentTimeMillis()
	 */
	@Test
	public void test_currentTimeMillis() {
		try {
			long firstRead = System.currentTimeMillis();
			try {
				Thread.sleep(150);
			} catch (InterruptedException e) {
			}
			long secondRead = System.currentTimeMillis();
			AssertJUnit.assertTrue("Incorrect times returned: " + firstRead + ", "
					+ secondRead, firstRead < secondRead);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Exception during test: " + e.toString());
		}
	}

	/**
	 * @tests java.lang.System#getProperties()
	 */
	@Test
	public void test_getProperties() {
		Properties p = System.getProperties();
 		AssertJUnit.assertTrue("Incorrect properties returned", p.getProperty(
 				"java.version").indexOf("1.", 0) >= 0);

		// ensure spec'ed properties are non-null. See System.getProperties()
		// spec.
		String[] props = { "java.version", "java.vendor", "java.vendor.url",
				"java.home", "java.vm.specification.version",
				"java.vm.specification.vendor", "java.vm.specification.name",
				"java.vm.version", "java.vm.vendor", "java.vm.name",
				"java.specification.name", "java.specification.vendor",
				"java.specification.name", "java.class.version",
				"java.class.path", "os.name", "os.arch",
				"os.version", "file.separator", "path.separator",
				"line.separator", "user.name", "user.home", "user.dir", };
		for (int i = 0; i < props.length; i++) {
			AssertJUnit.assertTrue(props[i], System.getProperty(props[i]) != null);
		}

		/*
		 * This test should only run for Java 1.8.0 and above. For Java 1.9.0
		 * and above, we do not support java.ext.dirs property.
		 */
		if (p.getProperty("java.version").startsWith("1.8.0")) {
			String javaExtDirs = "java.ext.dirs";
			AssertJUnit.assertTrue(javaExtDirs, System.getProperty(javaExtDirs) != null);
		}
	}

	/**
	 * @tests java.lang.System#getProperty(java.lang.String)
	 */
	@Test
	public void test_getProperty() {
 		AssertJUnit.assertTrue("Incorrect properties returned", System.getProperty(
 				"java.version").indexOf("1.", 0) >= 0);

		boolean is8859_1 = true;
		String encoding = System.getProperty("file.encoding");
		byte[] bytes = new byte[128];
		char[] chars = new char[128];
		for (int i = 0; i < bytes.length; i++) {
			bytes[i] = (byte) (i + 128);
			chars[i] = (char) (i + 128);
		}
		String charResult = new String(bytes);
		byte[] byteResult = new String(chars).getBytes();
		if (charResult.length() == 128 && byteResult.length == 128) {
			for (int i = 0; i < bytes.length; i++) {
				if (charResult.charAt(i) != (char) (i + 128)
						|| byteResult[i] != (byte) (i + 128))
					is8859_1 = false;
			}
		} else
			is8859_1 = false;
		String[] possibles = new String[] { "ISO8859_1", "8859_1", "ISO8859-1",
				"ISO-8859-1", "ISO_8859-1", "ISO_8859-1:1978", "ISO-IR-100",
				"LATIN1", "CSISOLATIN1" };
		boolean found8859_1 = false;
		for (int i = 0; i < possibles.length; i++) {
			if (possibles[i].equals(encoding)) {
				found8859_1 = true;
				break;
			}
		}
		AssertJUnit.assertTrue("Wrong encoding: " + encoding, !is8859_1 || found8859_1);
	}

	/**
	 * @tests java.lang.System#getProperty(java.lang.String, java.lang.String)
	 */
	@Test
	public void test_getProperty2() {
		AssertJUnit.assertTrue("Failed to return correct property value: "
				+ System.getProperty("java.version", "99999"), System
				.getProperty("java.version", "99999").indexOf("1.", 0) >= 0);

		AssertJUnit.assertTrue("Failed to return correct property value", System
				.getProperty("bogus.prop", "bogus").equals("bogus"));
	}

	/**
	 * @tests java.lang.System#setProperty(java.lang.String, java.lang.String)
	 */
	@Test
	public void test_setProperty3() {
		try {
			AssertJUnit.assertTrue("Failed to return null", System.setProperty("testing",
					"value1") == null);
			AssertJUnit.assertTrue("Failed to return old value", System.setProperty(
					"testing", "value2") == "value1");
			AssertJUnit.assertTrue("Failed to find value",
					System.getProperty("testing") == "value2");

			/* [PR CMVC 80288] setProperty() should check for empty key */
			boolean exception = false;
			try {
				System.setProperty("", "default");
			} catch (IllegalArgumentException e) {
				exception = true;
			}
			AssertJUnit.assertTrue("Expected IllegalArgumentException", exception);
		} finally {
			// remove "testing" property in case test method is called again
			System.getProperties().remove("testing");
		}
	}

	/**
	 * @tests java.lang.System#getSecurityManager()
	 */
	@Test
	public void test_getSecurityManager() {
		AssertJUnit.assertTrue("Returned incorrect SecurityManager", System
				.getSecurityManager() == null);
	}

	/**
	 * @tests java.lang.System#identityHashCode(java.lang.Object)
	 */
	@Test
	public void test_identityHashCode() {
		Object o = new Object();
		String s = "Gabba";
		AssertJUnit.assertTrue("Nonzero returned for null",
				System.identityHashCode(null) == 0);
		AssertJUnit.assertTrue("Nonequal has returned for Object", System
				.identityHashCode(o) == o.hashCode());
		AssertJUnit.assertTrue("Same as usual hash returned for String", System
				.identityHashCode(s) != s.hashCode());
	}

	/**
	 * @tests java.lang.System#runFinalization()
	 */
	@Test
	public void test_runFinalization() {
		flag = true;
		createInstance();
		int count = 10;
		// BB the gc below likely bogosifies the test, but will have to do for
		// the moment
		while (!ranFinalize && count-- > 0) {
			System.gc();
			System.runFinalization();
		}
		AssertJUnit.assertTrue("Failed to run finalization", ranFinalize);
	}

	/**
	 * @tests java.lang.System#runFinalizersOnExit(boolean)
	 */
	@Test
	public void test_runFinalizersOnExit() {
		try {
			System.runFinalizersOnExit(true);
		} catch (Throwable t) {
			AssertJUnit.assertTrue("Failed to set runFinalizersOnExit", false);
		}
		AssertJUnit.assertTrue("Passed runFinalizersOnExit", true);
	}

	/**
	 * @tests java.lang.System#setProperties(java.util.Properties)
	 */
	@Test
	public void test_setProperties() {
		Properties orgProps = System.getProperties();
		java.util.Properties tProps = new java.util.Properties();
		tProps.put("test.prop", "this is a test property");
		tProps.put("bogus.prop", "bogus");
		System.setProperties(tProps);
		try {
			AssertJUnit.assertTrue("Failed to set properties", System.getProperties()
					.getProperty("test.prop").equals("this is a test property"));
		} finally {
			// restore the original properties
			System.setProperties(orgProps);
		}
	}

	/**
	 * @tests java.lang.System#lineSeparator()
	 */
	@Test
	public void test_lineSeparator() {
		boolean onUnix = File.separatorChar == '/';
		String	ls = System.lineSeparator();
		if (onUnix) {
			AssertJUnit.assertTrue("wrong line separator returned", ls.equalsIgnoreCase("\n"));
		} else {
			AssertJUnit.assertTrue("wrong line separator returned", ls.equalsIgnoreCase("\r\n"));
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	@Test
	public void test_setSecurityManager() {
		/* [PR 97686, 123113] Improve System.setSecurityManager() initialization */
		class MySecurityManager extends SecurityManager {
			public void preloadClasses(Permission perm) {
				if (!(perm instanceof RuntimePermission)
						|| !perm.getName().toLowerCase().equals(
								"setsecuritymanager")) {
					// do nothing, required classes are now loaded
				}
			}

			public void checkPermission(Permission perm) {
				if (!(perm instanceof RuntimePermission)
						|| !perm.getName().toLowerCase().equals(
								"setsecuritymanager")) {
					super.checkPermission(perm);
				}
			}
		}

		MySecurityManager newManager = new MySecurityManager();
		// preload the required classes without calling implies()
		try {
			newManager.preloadClasses(new RuntimePermission("anything"));
		} catch (SecurityException e) {
		}
		try {
					System.setSecurityManager(new SecurityManager());
			System.getProperty("someProperty");
			Assert.fail("should cause SecurityException");
		} catch (SecurityException e) {
			// expected
		} finally {
					System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	@Test
	public void test_setSecurityManager2() {
		/* [PR 125194] Allow java.security.manager from system ClassLoader */
		try {
			String helperName = "org.openj9.test.java.lang.Test_System$TestSecurityManager";
			String output = Support_Exec.execJava(new String[] {
					"-Djava.security.manager=" + helperName, helperName },
					null, true);
			AssertJUnit.assertTrue("not correct SecurityManager: " + output, output
					.startsWith(helperName));
		} catch (Exception e) {
			Assert.fail("Unexpected: " + e);
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	public static class TestSecurityManager extends SecurityManager {
		public static void main(String[] args) {
			System.out
					.println(System.getSecurityManager().getClass().getName());
		}
	}

	/**
	 * Sets up the fixture, for example, open a network connection. This method
	 * is called before a test is executed.
	 */
	@BeforeMethod
	protected void setUp() {
		flag = false;
		ranFinalize = false;
	}

	public Test_System(String name) {
	}

	protected Test_System createInstance() {
		return new Test_System("FT");
	}

	protected void finalize() {
		if (flag)
			ranFinalize = true;
	}

}
