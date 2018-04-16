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

/* Provides double constants for testing exponent functions.  All the
   constants in this class should be verified by CheckDoubleAssumptions! */

public class ED	{

	public static final	double PE = Math.E;
	public static final	double NE = -PE;

	public static final double p1    = 1.0;
	public static final double p2    = 2.0;
	public static final double p3    = 3.0;
	public static final double p4    = 4.0;
	public static final double p5    = 5.0;
	public static final double p6    = 6.0;
	public static final double p7    = 7.0;
	public static final double p8    = 8.0;
	public static final double p9    = 9.0;
	public static final double p10   = 10.0;
	public static final double p11   = 11.0;
	public static final double p12   = 12.0;
	public static final double p13   = 13.0;
	public static final double p14   = 14.0;
	public static final double p15   = 15.0;
	public static final double p16   = 16.0;

	public static final double[] values = {
		ED.p1, ED.p2, ED.p3, ED.p4, ED.p5, ED.p6, ED.p7, ED.p8,
		ED.p9, ED.p10, ED.p11, ED.p12, ED.p13, ED.p14, ED.p15, ED.p16
	};

	public static final double n1    = -p1;
	public static final double n2    = -p2;
	public static final double n3    = -p3;
	public static final double n4    = -p4;
	public static final double n5    = -p5;
	public static final double n6    = -p6;
	public static final double n7    = -p7;
	public static final double n8    = -p8;
	public static final double n9    = -p9;
	public static final double n10   = -p10;
	public static final double n11   = -p11;
	public static final double n12   = -p12;
	public static final double n13   = -p13;
	public static final double n14   = -p14;
	public static final double n15   = -p15;
	public static final double n16   = -p16;
	
	public static final double nvalues[] = {
		ED.n1, ED.n2, ED.n3, ED.n4, ED.n5, ED.n6, ED.n7, ED.n8,
		ED.n9, ED.n10, ED.n11, ED.n12, ED.n13, ED.n14, ED.n15, ED.n16
	};

	public static final double p1R2  = p1;
	public static final double p2R2  = 1.4142135623730951;
	public static final double p3R2  = 1.7320508075688772;
	public static final double p4R2  = p2;
	public static final double p5R2  = 2.23606797749979;
	public static final double p6R2  = 2.449489742783178;
	public static final double p7R2  = 2.6457513110645907;
	public static final double p8R2  = 2.8284271247461903;
	public static final double p9R2  = p3;
	public static final double p10R2 = 3.1622776601683795;
	public static final double p11R2 = 3.3166247903554;
	public static final double p12R2 = 3.4641016151377544;
	public static final double p13R2 = 3.605551275463989;
	public static final double p14R2 = 3.7416573867739413;
	public static final double p15R2 = 3.872983346207417;
	public static final double p16R2 = p4;

	public static final double[] squareroots = {
		ED.p1R2, ED.p2R2, ED.p3R2, ED.p4R2, ED.p5R2, ED.p6R2, ED.p7R2, ED.p8R2,
		ED.p9R2, ED.p10R2, ED.p11R2, ED.p12R2, ED.p13R2, ED.p14R2, ED.p15R2, ED.p16R2
	};

	public static final double p1P2  = p1;
	public static final double p2P2  = p4;
	public static final double p3P2  = p9;
	public static final double p4P2  = p16;
	public static final double p5P2  = 25.0;
	public static final double p6P2  = 36.0;
	public static final double p7P2  = 49.0;
	public static final double p8P2  = 64.0;
	public static final double p9P2  = 81.0;
	public static final double p10P2 = 100.0;
	public static final double p11P2 = 121.0;
	public static final double p12P2 = 144.0;
	public static final double p13P2 = 169.0;
	public static final double p14P2 = 196.0;
	public static final double p15P2 = 225.0;
	public static final double p16P2 = 256.0;
	
	public static final double[] squares = {
		ED.p1P2, ED.p2P2, ED.p3P2, ED.p4P2, ED.p5P2, ED.p6P2, ED.p7P2, ED.p8P2,
		ED.p9P2, ED.p10P2, ED.p11P2, ED.p12P2, ED.p13P2, ED.p14P2, ED.p15P2, ED.p16P2
	};

	public static final double p1P3  = p1;
	public static final double p2P3  = p8;
	public static final double p3P3  = 27.0;
	public static final double p4P3  = 64.0;
	public static final double p5P3  = 125.0;
	public static final double p6P3  = 216.0;
	public static final double p7P3  = 343.0;
	public static final double p8P3  = 512.0;
	public static final double p9P3  = 729.0;
	public static final double p10P3 = 1000.0;
	public static final double p11P3 = 1331.0;
	public static final double p12P3 = 1728.0;
	public static final double p13P3 = 2197.0;
	public static final double p14P3 = 2744.0;
	public static final double p15P3 = 3375.0;
	public static final double p16P3 = 4096.0;
	
	public static final double[] cubes = {
		ED.p1P3, ED.p2P3, ED.p3P3, ED.p4P3, ED.p5P3, ED.p6P3, ED.p7P3, ED.p8P3,
		ED.p9P3, ED.p10P3, ED.p11P3, ED.p12P3, ED.p13P3, ED.p14P3, ED.p15P3, ED.p16P3
	};

	public static final double n1P3  = -p1P3;
	public static final double n2P3  = -p2P3;
	public static final double n3P3  = -p3P3;
	public static final double n4P3  = -p4P3;
	public static final double n5P3  = -p5P3;
	public static final double n6P3  = -p6P3;
	public static final double n7P3  = -p7P3;
	public static final double n8P3  = -p8P3;
	public static final double n9P3  = -p9P3;
	public static final double n10P3 = -p10P3;
	public static final double n11P3 = -p11P3;
	public static final double n12P3 = -p12P3;
	public static final double n13P3 = -p13P3;
	public static final double n14P3 = -p14P3;
	public static final double n15P3 = -p15P3;
	public static final double n16P3 = -p16P3;
	
	public static final double[] ncubes = {
		ED.n1P3, ED.n2P3, ED.n3P3, ED.n4P3, ED.n5P3, ED.n6P3, ED.n7P3, ED.n8P3,
		ED.n9P3, ED.n10P3, ED.n11P3, ED.n12P3, ED.n13P3, ED.n14P3, ED.n15P3, ED.n16P3
	};

	public static final double plog1 = 0.0;
	public static final double plog2 = 0.6931471805599453;
	public static final double plog3 = 1.0986122886681098;
	public static final double plog4 = 1.3862943611198906;
	public static final double plog5 = 1.6094379124341003;
	public static final double plog6 = 1.791759469228055;
	public static final double plog7 = 1.9459101490553132;
	public static final double plog8 = 2.0794415416798357;
	public static final double plog9 = 2.1972245773362196;
	public static final double plog10 = 2.302585092994046;
	public static final double plog11 = 2.3978952727983707;
	public static final double plog12 = 2.4849066497880004;
	public static final double plog13 = 2.5649493574615367;
	public static final double plog14 = 2.6390573296152584;
	public static final double plog15 = 2.70805020110221;
	public static final double plog16 = 2.772588722239781;

	public static final double[] logvalues = {
		ED.plog1, ED.plog2, ED.plog3, ED.plog4, ED.plog5, ED.plog6, ED.plog7, ED.plog8,
		ED.plog9, ED.plog10, ED.plog11, ED.plog12, ED.plog13, ED.plog14, ED.plog15, ED.plog16
	};

	public static final double pEP1 = 2.718281828459045;
	public static final double pEP2 = 7.38905609893065;
	public static final double pEP3 = 20.085536923187668;
	public static final double pEP4 = 54.598150033144236;
	public static final double pEP5 = 148.4131591025766;
	public static final double pEP6 = 403.4287934927351;
	public static final double pEP7 = 1096.6331584284585;
	public static final double pEP8 = 2980.9579870417283;
	public static final double pEP9 = 8103.083927575384;
	public static final double pEP10 = 22026.465794806718;
	public static final double pEP11 = 59874.14171519782;
	public static final double pEP12 = 162754.79141900392;
	public static final double pEP13 = 442413.3920089205;
	public static final double pEP14 = 1202604.2841647768;
	public static final double pEP15 = 3269017.3724721107;
	public static final double pEP16 = 8886110.520507872;

	public static final double[] expvalues = {
		ED.pEP1, ED.pEP2, ED.pEP3, ED.pEP4, ED.pEP5, ED.pEP6, ED.pEP7, ED.pEP8,
		ED.pEP9, ED.pEP10, ED.pEP11, ED.pEP12, ED.pEP13, ED.pEP14, ED.pEP15, ED.pEP16
	};

}
