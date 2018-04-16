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
package org.openj9.test.floatsanity;

/* Provides float constants to be used in tests.  All the constants in
	this class should be verified by CheckFloatAssumptions! */

public class F {

	/* Important float constants */

	public static final float PZERO = 0.0F;
	public static final float NZERO = -0.0F;
	public static final float PINF = Float.POSITIVE_INFINITY;
	public static final float NINF = Float.NEGATIVE_INFINITY;
	public static final float PMAX = Float.MAX_VALUE;
	public static final float NMAX = -Float.MAX_VALUE;
	public static final float PMIN = Float.MIN_VALUE;
	public static final float NMIN = -Float.MIN_VALUE;
	public static final float NAN = Float.NaN; /* the canonical float NaN */

	/* Additional float constants used in some tests */

	public static final float pOne = 1.0F;
	public static final float nOne = -1.0F;
	public static final float pTwo = 2.0F;
	public static final float nTwo = -2.0F;
	public static final float p1_1 = 1.1F;
	public static final float n1_1 = -1.1F;
	public static final float p2_2 = 2.2F;
	public static final float n2_2 = -2.2F;
	public static final float p100 = 100.0F;
	public static final float n100 = -100.0F;
	public static final float p200 = 200.0F;
	public static final float n200 = -200.0F;
	public static final float p1000 = 1000.0F;
	public static final float n1000 = -1000.0F;
	public static final float p0_01 = 0.01F;
	public static final float n0_01 = -0.01F;
	public static final float p0_005 = 0.005F;
	public static final float n0_005 = -0.005F;
	public static final float p0_0001 = 0.0001F;
	public static final float n0_0001 = -0.0001F;
	public static final float p0_0002 = 0.0002F;
	public static final float n0_0002 = -0.0002F;
	public static final float pC1 = (1.0F / PMAX);
	public static final float nC1 = (1.0F / NMAX);
	
	public static final float pBMAX = 127.0F;
	public static final float nBMAX = -128.0F;
	public static final float pSMAX = 32767.0F;
	public static final float nSMAX = -32768.0F;
	public static final float pIMAX = 2147483647.0F;
	public static final float nIMAX = -2147483648.0F;
	public static final float pLMAX = 9.223372036854776E18F;
	public static final float nLMAX = -9.223372036854776E18F;
	
	/* Helper methods. */

	public static float L2F(long a)    { return Float.intBitsToFloat((int)a); }
	public static long F2L(float a)    { return (long)Float.floatToIntBits(a); }
}
