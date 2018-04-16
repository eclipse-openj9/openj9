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

/* Provides double constants to be used in tests.  All the constants in
	this class should be verified by CheckDoubleAssumptions! */

public class D {

	/* Important double constants */

	public static final double PZERO = 0.0;
	public static final double NZERO = -0.0;
	public static final double PINF = Double.POSITIVE_INFINITY;
	public static final double NINF = Double.NEGATIVE_INFINITY;
	public static final double PMAX = Double.MAX_VALUE;
	public static final double NMAX = -Double.MAX_VALUE;
	public static final double PMAXODD = (1l<<53)-1d;	//PMAX,NMAX are even
	public static final double NMAXODD = -PMAXODD;		//these are the largest odds
	public static final double PMIN = Double.MIN_VALUE;
	public static final double NMIN = -Double.MIN_VALUE;
	public static final double NAN = Double.NaN; /* the canonical double NaN */

	/* Additional double constants used in some tests */

	public static final double pOne = 1.0;
	public static final double nOne = -1.0;
	public static final double pTwo = 2.0;
	public static final double nTwo = -2.0;
	public static final double p1_1 = 1.1;
	public static final double n1_1 = -1.1;
	public static final double p1_5 = 1.5;
	public static final double n1_5 = -1.5;
	public static final double p2_2 = 2.2;
	public static final double n2_2 = -2.2;
	public static final double p100 = 100.0;
	public static final double n100 = -100.0;
	public static final double p200 = 200.0;
	public static final double n200 = -200.0;
	public static final double p999 = 999.0;
	public static final double n999 = -999.0;
	public static final double p1000 = 1000.0;
	public static final double n1000 = -1000.0;
	public static final double p2000 = 2000.0;
	public static final double n2000 = -2000.0;
	public static final double p0_5  = 0.5;
	public static final double n0_5  = -0.5;
	public static final double pOneThird  = 1d/3;
	public static final double nOneThird  = -1d/3;
	public static final double p0_25  = 0.25;
	public static final double n0_25  = -0.25;
	public static final double pOneSixth  = 1d/6;
	public static final double nOneSixth  = -1d/6;
	public static final double p0_01 = 0.01;
	public static final double n0_01 = -0.01;
	public static final double p0_005 = 0.005;
	public static final double n0_005 = -0.005;
	public static final double p0_0001 = 0.0001;
	public static final double n0_0001 = -0.0001;
	public static final double p0_0002 = 0.0002;
	public static final double n0_0002 = -0.0002;

	public static final double pC1 = (1.0 / PMAX);
	public static final double nC1 = (1.0 / NMAX);
	public static final double p1075 = 1075.0;
	public static final double n1075 = -1075.0;
	public static final double p1076 = 1076.0;	//large, finite EVEN integers to test
	public static final double n1076 = -1076.0;	//exponentiation when base is negative

	public static final double pBMAX = 127.0;
	public static final double nBMAX = -128.0;
	public static final double pSMAX = 32767.0;
	public static final double nSMAX = -32768.0;
	public static final double pIMAX = 2147483647.0;
	public static final double nIMAX = -2147483648.0;
	public static final double pLMAX = 9.223372036854776E18;
	public static final double nLMAX = -9.223372036854776E18;
	
	/* Helper methods. */

	public static double L2D(long a)  { return Double.longBitsToDouble(a); }
	public static long D2L(double a)  { return Double.doubleToLongBits(a); }
}
