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
package com.ibm.j9.jsr292.indyn;

import org.testng.annotations.Test;
import org.testng.Assert;

public class ComplexIndyTest{
	
	@Test(groups = { "level.extended" })
	public void test_gwtTest_String() {
		String s = com.ibm.j9.jsr292.indyn.ComplexIndy.gwtTest("a");
		if (!s.equals("aa")) Assert.fail("Wrong string returned'" + s +"'");
	}
	@Test(groups = { "level.extended" })
	public void test_gwtTest_Integer() {
		String s = com.ibm.j9.jsr292.indyn.ComplexIndy.gwtTest(new Integer(1));
		if (!s.equals("2")) Assert.fail("Wrong string returned'" + s +"'");
	}
	@Test(groups = { "level.extended" })
	public void test_gwtTest_Object() {
		String s = com.ibm.j9.jsr292.indyn.ComplexIndy.gwtTest(new Object());
		if (!s.equals("DoesNotUnderStand: class java.lang.Object message: double")) Assert.fail("Wrong string returned'" + s +"'");
	}

	@Test(groups = { "level.extended" })
	public void test_permuteTest() {
		Object o = new Object();
		String s = com.ibm.j9.jsr292.indyn.ComplexIndy.permuteTest(1, 2, o);
		if (!o.toString().equals(s)) Assert.fail("Wrong string returned'" + s +"'");
	}

	
	@Test(groups = { "level.extended" })
	public void test_switchpointTest() {
		int s = com.ibm.j9.jsr292.indyn.ComplexIndy.switchpointTest(1, 2);
		if (s != 3) Assert.fail("Wrong int returned'" + s +"'");
	}

	@Test(groups = { "level.extended" })
	public void test_mcsTest() {
		int s = com.ibm.j9.jsr292.indyn.ComplexIndy.mcsTest(1, 2);
		if (s != 3) Assert.fail("Wrong int returned'" + s +"'");
	}

	@Test(groups = { "level.extended" })
	public void test_catchTest() {
		int s = com.ibm.j9.jsr292.indyn.ComplexIndy.catchTest(1, 2);
		if (s != 3) Assert.fail("Wrong int returned'" + s +"'");
	}

	@Test(groups = { "level.extended" })
	public void test_foldTest() {
		int s = com.ibm.j9.jsr292.indyn.ComplexIndy.foldTest(1);
		if (s != 3) Assert.fail("Wrong int returned'" + s +"'");
	}
	
	@Test(groups = { "level.extended" })
	public void test_fibTest() {
		int result = com.ibm.j9.jsr292.indyn.ComplexIndy.fibIndy(10);
		if (result != 89) Assert.fail("test_fibTest expected 89; got" + result);
	}
}
