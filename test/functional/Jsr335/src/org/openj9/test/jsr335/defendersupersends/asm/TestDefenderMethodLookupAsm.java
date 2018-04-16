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
package org.openj9.test.jsr335.defendersupersends.asm;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.invoke.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.function.Consumer;
import java.util.stream.Stream;
import java.io.IOException;

import static java.lang.invoke.MethodHandles.*;
import static java.lang.invoke.MethodType.*;

public class TestDefenderMethodLookupAsm {
	
	private static final HashMap<String, Class<? extends Object>> ASMTESTS_CLASSES = new HashMap<>();
	private static boolean ASMTESTS_CLASSES_LOADED = true;
	private static Logger logger = Logger.getLogger(TestDefenderMethodLookupAsm.class);

	
	/**
	 * Dump and Load ASM tests classes
	 */
	{
		try {
			String dumpDir = System.getProperty("user.dir");
			AsmUtils.dumpClasses(dumpDir);
			logger.debug("Classes successfully dumped to "+dumpDir);
		} catch(IOException e) {
			logger.error("Class dump failed: "+e.getLocalizedMessage());
			e.printStackTrace();
		}
		
		String asmTestPackage = "org.openj9.test.jsr335.defendersupersends.asm.";
		String[] asmTestNames = new String[]{"A", "B", "C", "D", "E", "F", "G", "H", "K", "L", "M", "N", "O", "P", "O1", "O2", "O3", "O4", "S", "T_K", "T_L", "U", "V_K", "V_L", "V_M", "W_L", "W_M", "W_O", "X_L", "X_M", "X_N", "Y_M", "Y_N", "Z"};
		Stream<String> asmTests = Arrays.stream(asmTestNames);
		asmTests.forEach(new Consumer<String>() {
			@Override
			public void accept(String s) {
				/* Stop loading more classes if we failed to load any of the specified classes. */
				if (!ASMTESTS_CLASSES_LOADED) {	return; }
				try {
					ASMTESTS_CLASSES.put(s, TestDefenderMethodLookupAsm.class.getClassLoader().loadClass(asmTestPackage + s));
				} catch (ClassNotFoundException e) {
					/* Indicate that ASM tests can not be run */
					ASMTESTS_CLASSES_LOADED = false;
				}
			}
		});
	}

	/**
	 * <b>NEGATIVE</b><br />
	 * Try to get a <b>SPECIAL</b> MethodHandle for the method <b>m()C</b> in the <b>DIRECT</b> super interface <b>K</b> from <b>T_K</b>. 
	 * This should fail because there are conflicting definitions of <b>m()C</b> in other super interfaces of <b>T_K</b>.
	 * 
	 * @throws Throwable Expected exceptions are caught. Any other exception should be treated as an error.
	 */
	@Test(groups = { "level.sanity" })
	public void testConflictingDefinitionsInSuperInterface() throws Throwable {
		if (!ASMTESTS_CLASSES_LOADED) {
			Assert.fail("Could not load ASM test classes. See <clinit> for details.");
		}
		Class<? extends Object> K = ASMTESTS_CLASSES.get("K");
		Class<? extends Object> T_K = ASMTESTS_CLASSES.get("T_K");
		
		MethodHandle lookupMH = lookup().findStatic(K, "lookup", methodType(Lookup.class));
		Lookup l = (Lookup)lookupMH.invoke();
		MethodHandle mh = l.findSpecial(K, "m", methodType(char.class), K);
		try {
			mh.invoke(K.cast(T_K.newInstance()));
			Assert.fail("Successfully invoked MethodHandle with conflicting method definitions.");
		} catch (IncompatibleClassChangeError e) { }
	}
	
	/**
	 * Construct the same tests as found in <i>org.openj9.test.jsr335.defendersupersends.asm</i>, and run these as MethodHandle tests.
	 * Test details can be found in JAZZ103 39862, attachment 20515.
	 */
	@Test(groups = { "level.sanity" })
	public void test335DefenderSupersendsAsmAsMethodHandles() throws Throwable {
		if (!ASMTESTS_CLASSES_LOADED) {
			Assert.fail("Could not load ASM test classes. See <clinit> for details.");
		}
		
		/* Construct test cases */
		ArrayList<LocalAsmTestCase> tests = new ArrayList<TestDefenderMethodLookupAsm.LocalAsmTestCase>(20);
		tests.add(new LocalAsmTestCase("O1",	"E", 'A', null));
		tests.add(new LocalAsmTestCase("O2",	"F", 'F', null));
		tests.add(new LocalAsmTestCase("O3",	"G", 'G', null));
		tests.add(new LocalAsmTestCase("O4",	"H", ' ', NoSuchMethodException.class));
		tests.add(new LocalAsmTestCase("S", 	"P", 'P', null));
		tests.add(new LocalAsmTestCase("T_K",	"K", ' ', IncompatibleClassChangeError.class));
		tests.add(new LocalAsmTestCase("T_L",	"L", 'L', null));
		tests.add(new LocalAsmTestCase("U",		"O", ' ', ClassCastException.class));
		tests.add(new LocalAsmTestCase("V_K",	"K", ' ', IncompatibleClassChangeError.class));
		tests.add(new LocalAsmTestCase("V_L",	"L", 'L', null));
		tests.add(new LocalAsmTestCase("V_M",	"M", 'B', null));
		tests.add(new LocalAsmTestCase("W_L",	"L", 'L', null));
		tests.add(new LocalAsmTestCase("W_M",	"M", 'B', null));
		tests.add(new LocalAsmTestCase("W_O",	"O", 'D', null));
		tests.add(new LocalAsmTestCase("X_L",	"L", 'L', null));
		tests.add(new LocalAsmTestCase("X_M",	"M", 'B', null));
		tests.add(new LocalAsmTestCase("X_N",	"N", 'B', null));
		tests.add(new LocalAsmTestCase("Y_M",	"M", 'B', null));
		tests.add(new LocalAsmTestCase("Y_N",	"N", 'B', null));
		tests.add(new LocalAsmTestCase("Z",		"D", 'D', null));
		
		/* Run tests */
		boolean testsFailed = false;
		StringBuilder testResults = new StringBuilder();
		for (LocalAsmTestCase t : tests) {
			Class<? extends Object> defc = ASMTESTS_CLASSES.get(t.defc);
			Class<? extends Object> target = ASMTESTS_CLASSES.get(t.target);
			
			MethodHandle lookupMH = lookup().findStatic(target, "lookup", methodType(Lookup.class));
			Lookup l = (Lookup)lookupMH.invoke();
			try {
				MethodHandle mh = l.findSpecial(target, "m", methodType(char.class), target);
				char result = (char)mh.invoke(defc.newInstance());
				AssertJUnit.assertEquals(t.result, result);
			} catch (Throwable e) {
				/* Add failure to result if we got unexpected exception */
				if ((null == t.ex) || !(t.ex.isAssignableFrom(e.getClass()))) {
					testsFailed = true;
					testResults.append("\nTest ");
					testResults.append(t.defc);
					testResults.append("\n\tUnexpected exception - ");
					testResults.append(e.getClass());
					testResults.append(": ");
					testResults.append(e.getMessage());
				}
			}
		}
		if (testsFailed) {
			Assert.fail(testResults.toString());
		}
	}
	
	/**
	 * Helper class for test335DefenderSupersendsAsmAsMethodHandles. 
	 */
	private class LocalAsmTestCase {
		public String defc;
		public String target;
		public char result;
		public Class<? extends Throwable> ex;
		
		public LocalAsmTestCase(String defc, String target, char result, Class<? extends Throwable> ex) {
			this.defc = defc;
			this.target = target;
			this.result = result;
			this.ex = ex;
		}
	}
}
