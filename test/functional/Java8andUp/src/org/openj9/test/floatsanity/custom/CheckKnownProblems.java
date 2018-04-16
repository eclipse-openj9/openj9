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
package org.openj9.test.floatsanity.custom;

import org.openj9.test.floatsanity.D;
import org.openj9.test.floatsanity.F;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/* Reproduce some float known float problems on NTO MIPS.  Note: some of
   these are denormal-related. */

@Test(groups = { "level.sanity" })
public class CheckKnownProblems {

	public static Logger logger = Logger.getLogger(CheckFunctionSymmetry.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check for past/present known problems");
	}

	private int juggleInt(int i) {
		i ^= 0x80000000;
		i++;
		if ((i & 1) == 0) {
			i += 0x7FFFFFFF;
		} else {
			i -= 0x80000001;
		}
		return i;
	}

	private long juggleLong(long l) {
		l ^= 0x8000000000000000L;
		l++;
		if ((l & 1) == 0L) {
			l += 0x7FFFFFFFFFFFFFFFL;
		} else {
			l -= 0x8000000000000001L;
		}
		return l;
	}

	private float juggleFloat(float f) {
		return F.L2F(F.F2L(f));
	}

	private double juggleDouble(double d) {
		return D.L2D(D.D2L(d));
	}

	// Genuine conversion problem
	public void knownProblem1() {
		String operation = "test known conversion problem: 13.0f -> 13L";
		logger.debug(operation);
		Assert.assertEquals(13L, (long)juggleFloat(13.0f), operation);
	}

	// I think this one is also denormal-related.
	public void knownProblem2() {
		String operation;
		float f5 = (float)((Math.pow(2.0, 24.0) - 1) * juggleInt(1) * (Math.pow(2.0, -173.0)));
		float f6 = (f5) * (-1);
		// f5 is the finite +ve float closest to the +ve zero.
		operation = "testing known denormal problem: finite positive float closest to +0 does not equal special values";
		logger.debug(operation);
		Assert.assertNotEquals(f5, F.PINF, 0, operation);
		Assert.assertNotEquals(f5, F.NINF, 0, operation);
		Assert.assertNotEquals(f5, F.PZERO, 0, operation);
		Assert.assertNotEquals(f5, F.NZERO, 0, operation);

		// f6 is the finite -ve float closest to the -ve zero.
		operation = "testing known denormal problem: negative positive float closest to -0 does not equal special values";
		logger.debug(operation);
		Assert.assertNotEquals(f6, F.PINF, 0, operation);
		Assert.assertNotEquals(f6, F.NINF, 0, operation);
		Assert.assertNotEquals(f6, F.PZERO, 0, operation);
		Assert.assertNotEquals(f6, F.NZERO, 0, operation);
	}

	// This one (used to?) cause nto mips to crash
	public void knownProblem3() {
		String operation;
		int i = juggleInt(0);
		float z = juggleFloat(1.34f) / i;
		double d2 = juggleDouble(1.45d) / i;
		float f1 = z * juggleInt(0);
		double d1 = d2 * juggleInt(0);

		operation = "test known comparison problem: auto-conversion and comparison <";
		logger.debug(operation);
		Assert.assertFalse(f1 < 2, operation);
		Assert.assertFalse(f1 < f1, operation);
		Assert.assertFalse(f1 < d1, operation);
		Assert.assertFalse(d1 < f1, operation);
		Assert.assertFalse(d1 < 2, operation);
		Assert.assertFalse(d1 < d1, operation);
		Assert.assertFalse(10 < d1, operation);

		operation = "test known comparison problem: auto-conversion and comparison <=";
		logger.debug(operation);
		Assert.assertFalse(f1 <= 2, operation);
		Assert.assertFalse(f1 <= f1, operation);
		Assert.assertFalse(f1 <= d1, operation);
		Assert.assertFalse(d1 <= f1, operation);
		Assert.assertFalse(d1 <= 2, operation);
		Assert.assertFalse(d1 <= d1, operation);
		Assert.assertFalse(10 <= d1, operation);

		operation = "test known comparison problem: auto-conversion and comparison >";
		logger.debug(operation);
		Assert.assertFalse(f1 > 2, operation);
		Assert.assertFalse(f1 > f1, operation);
		Assert.assertFalse(f1 > d1, operation);
		Assert.assertFalse(d1 > f1, operation);
		Assert.assertFalse(d1 > 2, operation);
		Assert.assertFalse(d1 > d1, operation);
		Assert.assertFalse(10 > d1, operation);

		operation = "test known comparison problem: auto-conversion and comparison >=";
		logger.debug(operation);
		Assert.assertFalse(f1 >= 2, operation);
		Assert.assertFalse(f1 >= f1, operation);
		Assert.assertFalse(f1 >= d1, operation);
		Assert.assertFalse(d1 >= f1, operation);
		Assert.assertFalse(d1 >= 2, operation);
		Assert.assertFalse(d1 >= d1, operation);
		Assert.assertFalse(10 >= d1, operation);

		operation = "test known comparison problem: auto-conversion and comparison !=";
		logger.debug(operation);
		Assert.assertTrue(f1 != 2, operation);
		Assert.assertTrue(f1 != f1, operation);
		Assert.assertTrue(f1 != d1, operation);
		Assert.assertTrue(d1 != f1, operation);
		Assert.assertTrue(d1 != 2, operation);
		Assert.assertTrue(d1 != d1, operation);
		Assert.assertTrue(10 != d1, operation);
	}

	// I think this one is denormal-related??
	public void knownProblem4() {
		float f = juggleFloat(1.40239846e-45f);
		String operation = "known problem: denormal float: " + f + " / 2 == +0 and == -0";
		logger.debug(operation);
		Assert.assertEquals(f / 2, F.NZERO, 0, operation);
		Assert.assertEquals(f / 2, F.PZERO, 0, operation);
	}

	// Genuine conversion problem
	public void knownProblem5() {
		String operation = "testing known problem converting 13L -> 13.0f";
		logger.debug(operation);
		Assert.assertEquals(13.0f, (float)juggleLong(13L), 0, operation);
	}

	// Smells denormal-related
	public void knownProblem6() {
		int i = juggleInt(-1);
		String operation = "known problem with smallest positive float and modulus";
		Assert.assertEquals(F.PMIN % i, F.PMIN, 0, operation);
	}

	// Modulo errors, denormal-related
	public void knownProblem7() {
		String operation = "known problems with modulus and denormal numbers, testing with Float.MIN_VALUE and Float.MAX_VALUE";
		logger.debug(operation);
		Assert.assertTrue(D.PMAX % D.PMIN == 0.0, operation);
		Assert.assertTrue(D.PMAX % D.PMAX >= 0.0, operation);
		Assert.assertTrue(D.PMIN % D.PMIN >= 0.0, operation);
		Assert.assertTrue(D.PMIN % D.PMAX > 0.0, operation);
		Assert.assertTrue(D.PMAX % D.NMIN == 0.0, operation);
		Assert.assertTrue(D.PMAX % D.NMAX >= 0.0, operation);
		Assert.assertTrue(D.PMIN % D.NMIN >= 0.0, operation);
		Assert.assertTrue(D.PMIN % D.NMAX > 0.0, operation);
		Assert.assertTrue(D.NMAX % D.PMIN == 0.0, operation);
		Assert.assertTrue(D.NMAX % D.PMAX <= 0.0, operation);
		Assert.assertTrue(D.NMIN % D.PMIN <= 0.0, operation);
		Assert.assertTrue(D.NMIN % D.PMAX < 0.0, operation);
		Assert.assertTrue(D.NMAX % D.NMIN == 0.0, operation);
		Assert.assertTrue(D.NMAX % D.NMAX <= 0.0, operation);
		Assert.assertTrue(D.NMIN % D.NMIN <= 0.0, operation);
		Assert.assertTrue(D.NMIN % D.NMAX < 0.0, operation);
	}

	// Part of a commutativity test for multiplication operator.
	// (I believe the failures are denormal-related)
	public void knownProblem8() {
		float af1[] = { 1.40239846e-45f, -0.0f, +0.0f, 3.40282347e+38f };
		float af2[] = { 1.40239846e-45f, -0.0f, +0.0f, 3.40282347e+38f };
		for (int i = 0; i < af1.length; i++) {
			for (int j = 0; j < af2.length; j++) {
				float val1 = af1[i] * af2[j];
				float val2 = af2[j] * af1[i];
				String operation = "known multiplication commutativity issue with denormals";
				logger.debug(operation);
				Assert.assertEquals(val2, val1, 0, operation);
			}
		}
	}

	// Float->long conversion problem.
	public void knownProblem9() {
		long l = juggleLong(11L);
		float f = juggleFloat(12.0f);
		String operation = "known float->long conversion problem with " + f + " - " + l;
		logger.debug(operation);
		Assert.assertEquals((-l) + f, f - l, 0, operation);
	}

}
