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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.PrintStream;
import java.io.PrintWriter;

@Test(groups = { "level.sanity" })
public class Test_Throwable {

	private static final Logger logger = Logger.getLogger(Test_Throwable.class);
	/**
	 * @tests java.lang.Throwable#Throwable()
	 */
	@Test
	public void test_Constructor() {
		try {
			if (true)
				throw new Throwable();
		} catch (Throwable e) {
			return;
		}
		AssertJUnit.assertTrue("Failed to throw Throwable", false);
	}

	/**
	 * @tests java.lang.Throwable#Throwable(java.lang.String)
	 */
	@Test
	public void test_Constructor2() {
		try {
			if (true)
				throw new Throwable("Throw");
		} catch (Throwable e) {
			AssertJUnit.assertTrue("Threw Throwable with incorrect message", e.getMessage().equals("Throw"));
			return;
		}
		AssertJUnit.assertTrue("Failed to throw Throwable", false);
	}

	/**
	 * @tests java.lang.Throwable#fillInStackTrace()
	 */
	@Test
	public void test_fillInStackTrace() {
		class Test implements Runnable {
			public int x;

			public Test(int x) {
				this.x = x;
			}

			public void anotherMethod() {
				if (true)
					throw new IndexOutOfBoundsException();
			}

			public void run() {
				if (x == 0)
					throw new IndexOutOfBoundsException();
				try {
					anotherMethod();
				} catch (IndexOutOfBoundsException e) {
					e.fillInStackTrace();
					throw e;
				}
			}
		}
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		try {
			new Test(0).run();
		} catch (Throwable e) {
			e.printStackTrace(ps);
		}
		ps.flush();
		String s = fixStacktrace(new String(bao.toByteArray(), 0, bao.size()));

		bao.reset();
		try {
			new Test(1).run();
		} catch (Throwable e) {
			e.printStackTrace(ps);
		}
		ps.close();
		String s2 = fixStacktrace(new String(bao.toByteArray(), 0, bao.size()));
		AssertJUnit.assertTrue("Invalid stackTrace? length: " + s2.length() + "\n" + s2, s2.length() > 300);
		AssertJUnit.assertTrue("Incorrect stackTrace printed: \n" + s2 + "\n\nCompared with:\n" + s, s2.equals(s));
		/*
		 * This works only for a previous VM, and not for JDK.
		 * "java.lang.UnknownError\r\n\r\n" + "Stack trace:\r\n" +
		 * "   com/oti/bb/automatedtests/Test_Throwable$1$TestThread.run()V\r\n"
		 * +
		 * "   com/oti/bb/automatedtests/Test_Throwable.test_fillInStackTrace()V\r\n"
		 * + "   com/oti/bb/automatedtests/Test_Throwable$3.runTest()V\r\n" +
		 * "   test/framework/TestCase.run(Ltest/framework/TestResult;)V\r\n"
		 */
	}

	/**
	 * @tests java.lang.Throwable#fillInStackTrace()
	 * @tests java.lang.Throwable#getStackTrace()
	 */
	@Test
	public void test_fillInStackTrace_getStackTrace() {
		/*
		 * [PR CMVC 129810] getStackTrace() always returns first class in VM to
		 * call it
		 */
		Throwable throwable = new Throwable();
		stackHelper1(throwable);
		StackTraceElement[] stack1 = throwable.getStackTrace();
		AssertJUnit.assertTrue("unexpected method1: " + stack1[0].getMethodName(),
				stack1[0].getMethodName().equals("stackHelper1"));
		stackHelper2(throwable);
		StackTraceElement[] stack2 = throwable.getStackTrace();
		AssertJUnit.assertTrue("unexpected method2: " + stack2[0].getMethodName(),
				stack2[0].getMethodName().equals("stackHelper2"));
	}

	/**
	 * @tests java.lang.Throwable#fillInStackTrace()
	 */
	private void stackHelper1(Throwable throwable) {
		throwable.fillInStackTrace();
	}

	/**
	 * @tests java.lang.Throwable#fillInStackTrace()
	 */
	private void stackHelper2(Throwable throwable) {
		throwable.fillInStackTrace();
	}

	private String fixStacktrace(String trace) {
		// remove linenumbers
		StringBuffer sb = new StringBuffer();
		int lastIndex = 0;
		while (lastIndex < trace.length()) {
			int index = trace.indexOf('\n', lastIndex);
			if (index == -1)
				index = trace.length();
			String line = trace.substring(lastIndex, index);
			lastIndex = index + 1;

			index = line.indexOf("(");
			if (index > -1) {
				line = line.substring(0, index);
			}
			// Usually the construction of the exception is removed
			// however if running with the JIT, it may not be removed
			/*
			 * [PR CMVC 93898] constructor may not be removed when running with
			 * jit
			 */
			if (line.indexOf("java.lang.Throwable") > -1)
				continue;
			sb.append(line);
			sb.append('\n');
		}
		return sb.toString();
	}

	/**
	 * @tests java.lang.Throwable#getMessage()
	 */
	@Test
	public void test_getMessage() {
		Throwable x = new ClassNotFoundException("A Message");
		AssertJUnit.assertTrue("Returned incorrect messasge string", x.getMessage().equals("A Message"));
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace()
	 */
	@Test
	public void test_printStackTrace() {
		Throwable x = new ClassNotFoundException("A Test Message");
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		PrintStream err = System.err;
		System.setErr(ps);
		x.printStackTrace();
		System.setErr(err);
		ps.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf("org.openj9.test.java.lang.Test_Throwable.test_printStackTrace") != -1;
		match &= s.indexOf("A Test Message") != -1;
		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace(java.io.PrintStream)
	 */
	@Test
	public void test_printStackTrace2() {
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		Throwable x = new java.net.UnknownHostException("A Test Message");
		x.printStackTrace(ps);
		ps.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf("org.openj9.test.java.lang.Test_Throwable.test_printStackTrace2") != -1;
		match &= s.indexOf("A Test Message") != -1;
		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
		/*
		 * AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, s.startsWith(
		 * "java.net.UnknownHostException: A Message\r\n\r\n" +
		 * "Stack trace:\r\n" + "   java/lang/Throwable.<init>()V\r\n" +
		 * "   java/lang/Throwable.<init>(Ljava/lang/String;)V\r\n" +
		 * "   java/lang/Exception.<init>(Ljava/lang/String;)V\r\n" +
		 * "   java/io/IOException.<init>(Ljava/lang/String;)V\r\n" +
		 * "   java/net/UnknownHostException.<init>(Ljava/lang/String;)V\r\n" +
		 * "   com/oti/bb/automatedtests/Test_Throwable.test_printStackTraceLjava_io_PrintStream()V\r\n"
		 * + "   com/oti/bb/automatedtests/Test_Throwable$6.runTest()V\r\n" +
		 * "   test/framework/TestCase.run(Ltest/framework/TestResult;)V\r\n"
		 * ));
		 */

	}

	/**
	 * @tests java.lang.Throwable#printStackTrace(java.io.PrintWriter)
	 */
	@Test
	public void test_printStackTrace3() {
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintWriter pw = new PrintWriter(bao);
		Throwable x = new java.net.UnknownHostException("A Test Message");
		x.printStackTrace(pw);
		pw.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf("org.openj9.test.java.lang.Test_Throwable.test_printStackTrace3") != -1;
		match &= s.indexOf("A Test Message") != -1;
		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace()
	 */
	@Test
	public void test_printStackTrace4() {

		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		PrintStream err = System.err;
		System.setErr(ps);

		try {
			mytest1();
		} catch (Throwable t) {
			t.printStackTrace();
		}

		System.setErr(err);
		ps.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf(
				"org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;

		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace(java.io.PrintStream)
	 */
	@Test
	public void test_printStackTrace5() {
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		try {
			mytest2();
		} catch (Throwable t) {
			t.printStackTrace(ps);
		}
		ps.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf(
				"org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace(java.io.PrintWriter)
	 */
	@Test
	public void test_printStackTrace6() {
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintWriter pw = new PrintWriter(bao);
		try {
			mytest3();
		} catch (Throwable t) {
			t.printStackTrace(pw);
		}
		pw.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		boolean match = s.indexOf(
				"org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource3") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource3") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource3") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2") != -1;
		match &= s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		match &= s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1") != -1;
		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace()
	 */
	@Test
	public void test_printStackTrace7() {

		try {
			mytest12();
		} catch (NullPointerException e) {
			// this is correct, ignore it
		} catch (Throwable e) {
			Assert.fail("NullPointerException not caught, thrown " + e.getMessage());
		}

		try {
			mytest13();
		} catch (IllegalArgumentException e) {
			// this is correct, ignore it
		} catch (Throwable e) {
			Assert.fail("IllegalArgumentException not caught, thrown " + e.getMessage());
		}
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace(java.io.PrintStream)
	 */
	@Test
	public void test_printStackTrace8() {
		ByteArrayOutputStream bao = new ByteArrayOutputStream();
		PrintStream ps = new PrintStream(bao);
		try {
			mytest21();
		} catch (Throwable t) {
			t.printStackTrace(ps);
		}
		ps.close();
		String s = new String(bao.toByteArray(), 0, bao.size());
		int pos1, pos2;
		boolean match;
		pos1 = s.indexOf(
				"org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1");
		pos2 = s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening Resource2");
		match = pos1 != -1;
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource2");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening Resource2");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening Resource2");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Suppressed: org.openj9.test.java.lang.Test_Throwable$TopException: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: closing Resource1");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$MiddleException: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;
		pos2 = s.indexOf(
				"Caused by: org.openj9.test.java.lang.Test_Throwable$BottomException: BottomException thrown: opening resource1");
		match &= pos2 != -1;
		match &= pos2 > pos1;
		pos1 = pos2;

		AssertJUnit.assertTrue("Incorrect stackTrace printed:\n" + s, match);
	}

	/**
	 * @tests java.lang.Throwable#printStackTrace
	 *
	 *        Tests printStackTrace with enableSuppression = false
	 *
	 */
	@Test
	public void test_printStackTrace9() {
		Exception x = new TestException(false, false);
		try {
			x.printStackTrace();
		} catch (java.lang.NullPointerException npe) {
			Assert.fail("printStackTrace() failed");
		}
	}

	/**
	 * @tests java.lang.Throwable#getSuppressed()
	 */
	@Test
	public void test_getSuppressed() {
		try {
			mytest1();
		} catch (Throwable e) {
			Throwable t[] = e.getSuppressed();
			AssertJUnit.assertTrue("getSuppressed returns incorrect Throwable array size = " + t.length, t.length == 1);
		}
	}

	/**
	 * @tests java.lang.Throwable#writeObject()
	 * @tests java.lang.Throwable#readObject()
	 * @tests java.lang.Throwable#getStackTrace()
	 *
	 *        This tests enableSuppression = true and enableWritableStackTrace =
	 *        true when serialising and deserialising Throwable twice
	 *
	 */
	@Test
	public void test_exceptionSerializeWritableStackTraceTrue() throws TestException {

		Exception x = new TestException(false, false);

		try {
			throw new TestException(true, true);
		} catch (Exception e) {
			try {
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				ObjectOutputStream oos = new ObjectOutputStream(out);
				oos.writeObject(e);
				oos.close();

				byte[] pickled = out.toByteArray();
				InputStream in = new ByteArrayInputStream(pickled);
				ObjectInputStream ois = new ObjectInputStream(in);
				Exception newE = (Exception)ois.readObject();

				ByteArrayOutputStream out2 = new ByteArrayOutputStream();
				ObjectOutputStream oos2 = new ObjectOutputStream(out2);
				oos2.writeObject(newE);
				oos2.close();

				byte[] pickled2 = out2.toByteArray();
				InputStream in2 = new ByteArrayInputStream(pickled2);
				ObjectInputStream ois2 = new ObjectInputStream(in2);
				Exception newNewE = (Exception)ois2.readObject();

				StackTraceElement[] eST = e.getStackTrace();
				StackTraceElement[] newEST = newE.getStackTrace();
				StackTraceElement[] newNewEST = newNewE.getStackTrace();

				for (int i = 0; i < eST.length; i++) {
					AssertJUnit.assertTrue(eST[i].equals(newEST[i]));
					AssertJUnit.assertTrue(eST[i].equals(newNewEST[i]));
				}

				StackTraceElement[] xST = x.getStackTrace();

				newE.setStackTrace(xST);
				newNewE.setStackTrace(xST);

				newEST = newE.getStackTrace();
				newNewEST = newNewE.getStackTrace();

				for (int i = 0; i < newEST.length; i++) {
					AssertJUnit.assertFalse(eST[i].equals(newEST[i]));
					AssertJUnit.assertTrue(newEST[i].equals(newNewEST[i]));
				}

			} catch (java.io.IOException ee) {
				Assert.fail("java.io.IOException");
			} catch (java.lang.ClassNotFoundException eee) {
				Assert.fail("java.lang.ClassNotFoundException");
			}

		}
	}

	/**
	 * @tests java.lang.Throwable#writeObject()
	 * @tests java.lang.Throwable#readObject()
	 * @tests java.lang.Throwable#getStackTrace()
	 *
	 *        This tests enableSuppression = true and enableWritableStackTrace =
	 *        false when serialising and deserialising Throwable twice
	 *
	 */
	@Test
	public void test_exceptionSerializeWritableStackTraceFalse() throws TestException {

		Exception x = new TestException(true, true);

		try {
			throw new TestException(true, false);
		} catch (Exception e) {
			try {
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				ObjectOutputStream oos = new ObjectOutputStream(out);
				oos.writeObject(e);
				oos.close();

				byte[] pickled = out.toByteArray();
				InputStream in = new ByteArrayInputStream(pickled);
				ObjectInputStream ois = new ObjectInputStream(in);
				Exception newE = (Exception)ois.readObject();

				ByteArrayOutputStream out2 = new ByteArrayOutputStream();
				ObjectOutputStream oos2 = new ObjectOutputStream(out2);
				oos2.writeObject(newE);
				oos2.close();

				byte[] pickled2 = out2.toByteArray();
				InputStream in2 = new ByteArrayInputStream(pickled2);
				ObjectInputStream ois2 = new ObjectInputStream(in2);
				Exception newNewE = (Exception)ois2.readObject();

				AssertJUnit.assertTrue(e.getStackTrace().length == 0);
				AssertJUnit.assertTrue(newE.getStackTrace().length == 0);
				AssertJUnit.assertTrue(newNewE.getStackTrace().length == 0);

				AssertJUnit.assertTrue(x.getStackTrace().length != 0);

				newE.setStackTrace(x.getStackTrace());
				newNewE.setStackTrace(x.getStackTrace());

				AssertJUnit.assertTrue(e.getStackTrace().length == 0);
				AssertJUnit.assertTrue(newE.getStackTrace().length == 0);
				AssertJUnit.assertTrue(newNewE.getStackTrace().length == 0);

			} catch (java.io.IOException ee) {
				Assert.fail("java.io.IOException");
			} catch (java.lang.ClassNotFoundException eee) {
				Assert.fail("java.lang.ClassNotFoundException");
			}
		}
	}

	/**
	 * @tests java.lang.Throwable#writeObject()
	 * @tests java.lang.Throwable#readObject()
	 * @tests java.lang.Throwable#addSuppressed()
	 * @tests java.lang.Throwable#getSuppressed()
	 *
	 *        This tests enableSuppression = true and enableWritableStackTrace =
	 *        true when serialising and deserialising Throwable twice
	 *
	 */
	@Test
	public void test_exceptionSerializeEnableSuppressionTrue() throws TestException {

		try {
			throw new TestException(true, true);
		} catch (Exception e) {
			try {
				e.addSuppressed(new Throwable());

				ByteArrayOutputStream out = new ByteArrayOutputStream();
				ObjectOutputStream oos = new ObjectOutputStream(out);
				oos.writeObject(e);
				oos.close();

				byte[] pickled = out.toByteArray();
				InputStream in = new ByteArrayInputStream(pickled);
				ObjectInputStream ois = new ObjectInputStream(in);
				Exception newE = (Exception)ois.readObject();

				ByteArrayOutputStream out2 = new ByteArrayOutputStream();
				ObjectOutputStream oos2 = new ObjectOutputStream(out2);
				oos2.writeObject(newE);
				oos2.close();

				byte[] pickled2 = out2.toByteArray();
				InputStream in2 = new ByteArrayInputStream(pickled2);
				ObjectInputStream ois2 = new ObjectInputStream(in2);
				Exception newNewE = (Exception)ois2.readObject();

				AssertJUnit.assertTrue(e.getSuppressed().length == 1);
				AssertJUnit.assertTrue(newE.getSuppressed().length == 1);
				AssertJUnit.assertTrue(newNewE.getSuppressed().length == 1);

				e.addSuppressed(new Throwable());
				newE.addSuppressed(new Throwable());
				newNewE.addSuppressed(new Throwable());

				AssertJUnit.assertTrue(e.getSuppressed().length == 2);
				AssertJUnit.assertTrue(newE.getSuppressed().length == 2);
				AssertJUnit.assertTrue(newNewE.getSuppressed().length == 2);

			} catch (java.io.IOException ee) {
				Assert.fail("java.io.IOException");
			} catch (java.lang.ClassNotFoundException eee) {
				Assert.fail("java.lang.ClassNotFoundException");
			}
		}
	}

	/**
	 * @tests java.lang.Throwable#writeObject()
	 * @tests java.lang.Throwable#readObject()
	 * @tests java.lang.Throwable#addSuppressed()
	 * @tests java.lang.Throwable#getSuppressed()
	 *
	 *        This tests enableSuppression = false and enableWritableStackTrace
	 *        = true when serialising and deserialising Throwable twice
	 *
	 */
	@Test
	public void test_exceptionSerializeEnableSuppressionFalse() throws TestException {

		try {
			throw new TestException(false, true);
		} catch (Exception e) {
			try {
				e.addSuppressed(new Throwable());

				ByteArrayOutputStream out = new ByteArrayOutputStream();
				ObjectOutputStream oos = new ObjectOutputStream(out);
				oos.writeObject(e);
				oos.close();

				byte[] pickled = out.toByteArray();
				InputStream in = new ByteArrayInputStream(pickled);
				ObjectInputStream ois = new ObjectInputStream(in);
				Exception newE = (Exception)ois.readObject();

				ByteArrayOutputStream out2 = new ByteArrayOutputStream();
				ObjectOutputStream oos2 = new ObjectOutputStream(out2);
				oos2.writeObject(newE);
				oos2.close();

				byte[] pickled2 = out2.toByteArray();
				InputStream in2 = new ByteArrayInputStream(pickled2);
				ObjectInputStream ois2 = new ObjectInputStream(in2);
				Exception newNewE = (Exception)ois2.readObject();

				newE.addSuppressed(new Throwable());
				newNewE.addSuppressed(new Throwable());

				AssertJUnit.assertTrue(e.getSuppressed().length == 0);
				AssertJUnit.assertTrue(newE.getSuppressed().length == 0);
				AssertJUnit.assertTrue(newNewE.getSuppressed().length == 0);

				e.addSuppressed(new Throwable());
				newE.addSuppressed(new Throwable());
				newNewE.addSuppressed(new Throwable());

				AssertJUnit.assertTrue(e.getSuppressed().length == 0);
				AssertJUnit.assertTrue(newE.getSuppressed().length == 0);
				AssertJUnit.assertTrue(newNewE.getSuppressed().length == 0);

			} catch (java.io.IOException ee) {
				Assert.fail("java.io.IOException");
			} catch (java.lang.ClassNotFoundException eee) {
				Assert.fail("java.lang.ClassNotFoundException");
			}
		}
	}

	void mytest1() throws Throwable {
		logger.debug("Test1 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			logger.debug("rs1 open within try block");
			rs1.open();
		} catch (Exception e) {
			primaryExe1 = e;
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed(e);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	void mytest12() throws Throwable {
		logger.debug("Test12 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			logger.debug("rs1 open within try block");
			rs1.open();
		} catch (Exception e) {
			primaryExe1 = e;
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed(e);
					primaryExe1.addSuppressed(null);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	void mytest13() throws Throwable {
		logger.debug("Test13 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			logger.debug("rs1 open within try block");
			rs1.open();
		} catch (Exception e) {
			primaryExe1 = e;
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed(primaryExe1);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	void mytest2() throws Throwable {
		logger.debug("Test2 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			Throwable primaryExe2 = null;
			Resource2 rs2 = new Resource2();
			try {
				logger.debug("rs1/2 open within try block");
				rs1.open();
				rs2.open();
			} catch (Exception e) {
				primaryExe2 = e;
				throw primaryExe2;
			} finally {
				if (primaryExe2 != null) {
					try {
						logger.debug("close resource 2 within a try block");
						rs2.close();
					} catch (Exception e) {
						primaryExe2.addSuppressed(e);
					}
				} else {
					logger.debug("close resource 2 normally");
					rs2.close();
				}
			}
		} catch (Exception e) {
			primaryExe1 = e;
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed(e);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	void mytest21() throws Throwable {
		logger.debug("Test mytest21 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			Throwable primaryExe2 = null;
			Resource2 rs2 = new Resource2();
			try {
				logger.debug("rs2 open within try block");
				rs2.open();
			} catch (Exception e) {
				primaryExe2 = e;
				throw primaryExe2;
			} finally {
				if (primaryExe2 != null) {
					try {
						logger.debug("close resource 2 within a try block");
						rs2.close();
					} catch (Exception e) {
						primaryExe2.addSuppressed(e);
					}
				} else {
					logger.debug("close resource 2 normally");
					rs2.close();
				}
			}
		} catch (Exception e) {
			try {
				rs1.open();
			} catch (Exception e1) {
				primaryExe1 = e1;
				primaryExe1.addSuppressed(e);
			}
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed(e);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	void mytest3() throws Throwable {
		logger.debug("Test3 starts.....");

		Throwable primaryExe1 = null;
		Resource1 rs1 = new Resource1();
		try {
			Throwable primaryExe2 = null;
			Resource2 rs2 = new Resource2();
			try {
				Throwable primaryExe3 = null;
				Resource3 rs3 = new Resource3();
				try {
					logger.debug("rs1/2/3 open within try block");
					rs1.open();
					rs2.open();
					rs3.open();
				} catch (Exception e) {
					primaryExe3 = e;
					throw primaryExe3;
				} finally {
					if (primaryExe3 != null) {
						try {
							logger.debug("close resource 3 within a try block");
							rs3.close();
						} catch (Exception e) {
							primaryExe3.addSuppressed(e);
						}
					} else {
						logger.debug("close resource 3 normally");
						rs3.close();
					}
				}

			} catch (Exception e) {
				primaryExe2 = e;
				throw primaryExe2;
			} finally {
				if (primaryExe2 != null) {
					try {
						logger.debug("close resource 2 within a try block");
						rs2.close();
					} catch (Exception e) {
						primaryExe2.addSuppressed(e);
					}
				} else {
					logger.debug("close resource 2 normally");
					rs2.close();
				}
			}
		} catch (Exception e) {
			primaryExe1 = e;
			throw primaryExe1;
		} finally {
			if (primaryExe1 != null) {
				try {
					logger.debug("close resource 1 within a try block");
					rs1.close();
				} catch (Exception e) {
					primaryExe1.addSuppressed((Throwable)e);
				}
			} else {
				logger.debug("close resource 1 normally");
				rs1.close();
			}
		}
	}

	static class BottomException extends Exception {
		BottomException(String detailMessage) {
			super(detailMessage);
		}
	}

	static class MiddleException extends Exception {
		MiddleException(Throwable cause) {
			super(cause);
		}
	}

	static class TopException extends Exception {
		TopException(Throwable cause) {
			super(cause);
		}
	}

	static class TestException extends Exception {
		TestException(boolean enableSuppression, boolean enableWritableStackTrace) {
			super("test", null, enableSuppression, enableWritableStackTrace);
		}
	}

	static class Resource1 {

		public void open() throws Exception {
			a("opening resource1");
		}

		public void close() throws Exception {
			a("closing Resource1");
		}
	}

	static class Resource2 {
		public void open() throws Exception {
			a("opening Resource2");
		}

		public void close() throws Exception {
			a("closing Resource2");
		}
	}

	public static class Resource3 {
		public void open() throws Exception {
			a("opening Resource3");
		}

		public void close() throws Exception {
			a("closing Resource3");
		}
	}

	static void a(String id) throws TopException {
		try {
			b(id);
		} catch (MiddleException e) {
			throw new TopException(e);
		}
	}

	static void b(String id) throws MiddleException {
		c(id);
	}

	static void c(String id) throws MiddleException {
		try {
			d(id);
		} catch (BottomException e) {
			throw new MiddleException(e);
		}
	}

	static void d(String id) throws BottomException {
		e(id);
	}

	static void e(String id) throws BottomException {
		throw new BottomException("BottomException thrown: " + id);
	}


	/**
	 * @tests java.lang.Throwable#toString()
	 */
	@Test
	public void test_toString() {
		try {
			if (true)
				throw new Throwable("Throw");
		} catch (Throwable e) {
			AssertJUnit.assertTrue("Threw Throwable with incorrect string",
					e.toString().equals("java.lang.Throwable: Throw"));
			return;
		}
		AssertJUnit.assertTrue("Failed to throw Throwable", false);
	}

	/**
	 * @tests java.lang.Throwable#getCause()
	 */
	static class TestSuperError {
		static {
			if (true) {
				RuntimeException t = new RuntimeException("badness");
				Error e = new Error("some error");
				// create a loop
				e.initCause(t);
				t.initCause(e);
				throw t;
			}
		}
	}

	static class TestError extends TestSuperError {
	}

	/**
	 * @tests java.lang.Throwable#getCause()
	 */
	@Test
	public void test_classInitializationFailure() {
		/*
		 * [PR 118653] Throw better NoClassDefFoundError for failed Class
		 * initialization
		 */
		/*
		 * [PR CMVC 111503] Copying Throwables for NoClassDefFoundError loses
		 * type information.
		 */
		try {
			// first load causes ExceptionInInitializerError
			logger.debug(TestError.class);
		} catch (Throwable e) {
			AssertJUnit.assertEquals("1a", ExceptionInInitializerError.class, e.getClass());
			Throwable cause1 = e.getCause();
			AssertJUnit.assertEquals("1b", "badness", cause1.getMessage());
			AssertJUnit.assertEquals("1c", RuntimeException.class, cause1.getClass());
			Throwable cause2 = cause1.getCause();
			AssertJUnit.assertEquals("1d", "some error", cause2.getMessage());
			AssertJUnit.assertEquals("1e", Error.class, cause2.getClass());
			cause1.setStackTrace(new StackTraceElement[0]);
			cause2.setStackTrace(new StackTraceElement[0]);
		}
		try {
			// second load causes NoClassDefFoundError
			logger.debug(TestError.class);
		} catch (Throwable e) {
			AssertJUnit.assertEquals("2a", NoClassDefFoundError.class, e.getClass());
			AssertJUnit.assertTrue("2b", e.getMessage().startsWith("org.openj9.test.java.lang.Test_Throwable$TestError"));
			Throwable cause1 = e.getCause();
			AssertJUnit.assertEquals("2c", "badness", cause1.getMessage());
			AssertJUnit.assertEquals("2d", RuntimeException.class, cause1.getClass());
			StackTraceElement[] stackTrace1 = cause1.getStackTrace();
			AssertJUnit.assertTrue("2d stack trace modified", stackTrace1.length > 0);
			cause1.setStackTrace(new StackTraceElement[0]);
			Throwable cause2 = cause1.getCause();
			AssertJUnit.assertEquals("2e", "some error", cause2.getMessage());
			AssertJUnit.assertEquals("2f", Error.class, cause2.getClass());
			StackTraceElement[] stackTrace2 = cause2.getStackTrace();
			AssertJUnit.assertTrue("2f stack trace modified", stackTrace2.length > 0);
			cause2.setStackTrace(new StackTraceElement[0]);
		}
		try {
			// second load causes NoClassDefFoundError
			logger.debug(TestSuperError.class);
		} catch (Throwable e) {
			AssertJUnit.assertEquals("3a", e.getClass(), NoClassDefFoundError.class);
			AssertJUnit.assertTrue("3b",
					e.getMessage().startsWith("org.openj9.test.java.lang.Test_Throwable$TestSuperError"));
			Throwable cause1 = e.getCause();
			AssertJUnit.assertEquals("3c", "badness", cause1.getMessage());
			AssertJUnit.assertEquals("3d", RuntimeException.class, cause1.getClass());
			StackTraceElement[] stackTrace1 = cause1.getStackTrace();
			AssertJUnit.assertTrue("3d stack trace modified", stackTrace1.length > 0);
			Throwable cause2 = cause1.getCause();
			AssertJUnit.assertEquals("3e", "some error", cause2.getMessage());
			AssertJUnit.assertEquals("3f", Error.class, cause2.getClass());
			StackTraceElement[] stackTrace2 = cause2.getStackTrace();
			AssertJUnit.assertTrue("3f stack trace modified", stackTrace2.length > 0);
		}
	}

	/**
	 * @tests java.lang.Throwable#initCause()
	 */
	static class RuntimeExceptionA extends RuntimeException {
		Exception e;
		RuntimeExceptionA ea;

		public RuntimeExceptionA(Throwable t) {
			super();
			e = new Exception(t);
			ea = this;
		}

		public Throwable getCause() {
			return e.getCause();
		}

		// this override method should not be called by J9VMInternals.copyThrowable
		public Throwable initCause(Throwable t) {
			e.initCause(t);
			return this;
		}
	}

	/**
	 * @tests java.lang.Throwable#initCause()
	 */
	static class StaticClass {
		public static String a = "here";

		static {
			a();
		}

		public static void a() {
			if (true) {
				throw new RuntimeExceptionA(new Exception());
			}
		}
	}

	/**
	 * @tests java.lang.Throwable#initCause()
	 */
	@Test
	public void test_classWInitCauseInitFailure() {
		try {
			logger.debug(StaticClass.a);
		} catch (Throwable e) {
			AssertJUnit.assertNotNull(e);

			//	currently Throwable.initCause is called and throw exception message "Cause already initialized"
			String eMsg = e.getMessage();
			if (eMsg != null) {
				AssertJUnit.assertFalse("Found unexpected excepton message: Cause already initialized",
						eMsg.equalsIgnoreCase("Cause already initialized"));
			}
			StackTraceElement[] eST = e.getStackTrace();
			for (int i = 0; i < eST.length; i++) {
				AssertJUnit.assertTrue("Found unexpected call (java.lang.Throwable.initCause) in the stacktrace",
						eST[i].toString().indexOf("java.lang.Throwable.initCause(Throwable.java") == -1);
			}

			//	check the cause contains application method in the stacktrace
			Throwable cause = e.getCause();
			AssertJUnit.assertNotNull(cause);
			boolean foundAppCause = false;
			eST = cause.getStackTrace();
			for (int i = 0; i < eST.length; i++) {
				if (eST[i].toString().indexOf("org.openj9.test.java.lang.Test_Throwable$StaticClass.a") == 0) {
					foundAppCause = true;
					break;
				}
			}
			AssertJUnit.assertTrue(
					"Not found expected application class in the stacktrace: org.openj9.test.java.lang.Test_Throwable$StaticClass.a",
					foundAppCause);
		}
	}
}
