/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.string;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;

@Test(groups = { "level.sanity" })
public class StringInterning {

	public static final Logger logger = Logger.getLogger(StringInterning.class);

	String testDeclaredField;

	public class createStrings extends Thread {
		private static final int NUM_STRINGS = 1000;
		private String[] buffer;
		private String testString;
		private Integer i;
		boolean passed, intern;

		public createStrings(String testString, boolean intern) {
			super();
			this.testString = testString;
			this.intern = intern;
			buffer = new String[NUM_STRINGS];
		}

		@Override
		public void run() {
			int j;
			String temp = testString;
			passed = true;

			for (j = 0; j < 10; j++) {
				for (i = 0; i < NUM_STRINGS; i = i + 1) {
					temp = testString + i.toString();
					if (intern)
						temp = temp.intern();
					buffer[i] = temp;
				}
				for (i = 0; i < NUM_STRINGS; i = i + 1) {
					temp = testString + i.toString();
					if (intern)
						temp = temp.intern();
					passed &= passed && (buffer[i].compareTo(temp) == 0);
					if (intern) {
						passed &= buffer[i] == temp;
					}
				}
			}
		}

		public boolean isPassed() {
			return passed;
		}

	}

	String head = "head", tail = "tail";
	String salt;

	public void testTwoIdenticalStrings() {
		salt = "testTwoIdenticalStrings";
		String s1, s2, reference = head + salt + tail;
		s1 = head + salt;
		s1 = s1 + tail;
		s2 = head;
		s2 = s2 + salt + tail;
		if (s1 == s2) {
			logger.debug("testTwoIdenticalStrings: s1 == s2");
		}

		s1 = s1.intern();
		s2 = s2.intern();
		AssertJUnit.assertEquals("compare s1 to reference", reference.compareTo(s1), 0);
		AssertJUnit.assertEquals("compare s2 to reference", reference.compareTo(s2), 0);
		AssertJUnit.assertSame("expect s1 === s2 ", s1, s2);
	}

	public void testTwoIdenticalMultibyteStrings() {
		salt = "testTwoIdenticalMultibyteStrings";
		String[][] helloWorlds = { { "hello", "world" },
				{ "\uc548\ub155\ud558\uc138\uc694", "\uc138\uacc4" },
				{ "\u039a\u03b1\u03bb\u03b7\u03bc\u03ad\u03c1\u03b1", "\u03ba\u03cc\u03c3\u03bc\u03b5" } };
		for (int i = 0; i < helloWorlds.length; ++i) {
			String[] hw = helloWorlds[i];
			String hello = hw[0], world = hw[1];
			String s1, s2, reference = hello + "," + world;
			logger.debug(reference);
			s1 = hello + ",";
			s1 = s1 + world;
			s2 = hello;
			s2 = s2 + "," + world;

			s1 = s1.intern();
			s2 = s2.intern();
			AssertJUnit.assertEquals("compare s1 to reference", reference.compareTo(s1), 0);
			AssertJUnit.assertEquals("compare s2 to reference", reference.compareTo(s2), 0);
			AssertJUnit.assertSame("expect s1 === s2 ", s1, s2);
		}
	}

	public void testNonCollidingStrings() {
		salt = "testNonCollidingStrings";
		String s1, s2, reference1 = head + salt + "s1" + tail, reference2 = head + salt + "s2" + tail;
		s1 = head + salt + "s1";
		s1 = s1 + tail;
		s2 = head;
		s2 = s2 + salt + "s2" + tail;

		s1 = s1.intern();
		s2 = s2.intern();
		AssertJUnit.assertEquals("compare s1 to reference", reference1.compareTo(s1), 0);
		AssertJUnit.assertEquals("compare s2 to reference", reference2.compareTo(s2), 0);
		AssertJUnit.assertNotSame("expect s1 != s2", s1, s2);
	}

	public void testCollidingStrings() {
		salt = "testCollidingStrings";
		String s1, s2, reference1 = head + salt + "bQ" + tail, reference2 = head + salt + "ap" + tail;
		s1 = head + salt + "bQ";
		s1 = s1 + tail;
		s2 = head;
		s2 = s2 + salt + "ap"
				+ tail; /* this should have the same hash as s1. Subtract 1 from character N and add 31 to character N+1 */
		s1 = s1.intern();
		s2 = s2.intern();
		AssertJUnit.assertEquals("compare s1 to reference", reference1.compareTo(s1), 0);
		AssertJUnit.assertEquals("compare s2 to reference", reference2.compareTo(s2), 0);
		AssertJUnit.assertNotSame("expect s1 != s2", s1, s2);

	}

	public void testImplicitInterning() {
		String sObject = new String("testDeclaredField");
		Field f = null;
		try {
			f = this.getClass().getDeclaredField("testDeclaredField");
		} catch (SecurityException e) {
			Assert.fail("SecurityException");
		} catch (NoSuchFieldException e) {
			Assert.fail("NoSuchFieldException");
		}
		String sName = f.getName();
		String sObject2 = sObject.intern();
		AssertJUnit.assertSame(sObject2, sName);
	}

	public void testConcurrentStringCreation() {
		createStrings t1, t2;

		t1 = new createStrings("Thread1", false);
		t2 = new createStrings("Thread2", false);
		t1.start();
		t2.start();
		try {
			t1.join();
			t2.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
			Assert.fail("exception in createStrings");
		}

		AssertJUnit.assertTrue(t1.isPassed());
		AssertJUnit.assertTrue(t2.isPassed());

	}

	public void testConcurrentStringInterning() {
		createStrings t1, t2;

		t1 = new createStrings("Thread1a", true);
		t2 = new createStrings("Thread2a", true);
		t1.start();
		t2.start();
		try {
			t1.join();
			t2.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
			Assert.fail("exception in createStrings");
		}

		AssertJUnit.assertTrue(t1.isPassed());
		AssertJUnit.assertTrue(t2.isPassed());

	}

}
