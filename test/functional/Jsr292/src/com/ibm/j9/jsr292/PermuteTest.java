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
package com.ibm.j9.jsr292;
import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodType.methodType;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class PermuteTest  { 
	private final static long const_arg0 = (long)-6892365923966373888L;
	private final static int const_arg1 = (int)-1.36787674E9;
	private final static long const_arg2 = (long)5020723882078406656L;
	private final static float const_arg3 = (float)9.3568392E7f;
	private final static long const_arg4 = (long)-3346392731103490048L;
	private final static double const_arg5 = (double)5.9620004E7;
	private final static int const_arg6 = (int)1.791556E8;
	private final static short const_arg7 = (short)32338.05;
	private final static byte const_arg8 = (byte)87.56171;
	private final static int const_arg9 = (int)-4.25774784E8;
	private final static double const_arg10 = (double)2.5361548E7;
	private final static double const_arg11 = (double)7.3694768E7;
	private final static long const_arg12 = (long)-385721256816320512L;
	private final static byte const_arg13 = (byte)36.451653;
	private final static char const_arg14 = (char)15380.821;
	private final static long const_arg15 = (long)7821635820523210752L;
	private final static long const_arg16 = (long)-8363124449672468480L;
	private final static int const_arg17 = (int)1.00277594E9;
	private final static long const_arg18 = (long)165721501884016640L;
	private final static float const_arg19 = (float)3.4655396E7f;
	private final static long const_arg20 = (long)6156444590360936448L;
	private final static double const_arg21 = (double)4726905.0;
	private final static int const_arg22 = (int)-1.21837594E9;
	private final static short const_arg23 = (short)6472.7783;
	private final static byte const_arg24 = (byte)30.474657;
	private final static int const_arg25 = (int)1.71506688E9;
	private final static double const_arg26 = (double)1.5349482E7;
	private final static double const_arg27 = (double)3.2427682E7;
	private final static long const_arg28 = (long)7460656684432205824L;
	private final static byte const_arg29 = (byte)-15.571779;
	private final static char const_arg30 = (char)43285.996;
	private final static long const_arg31 = (long)-1802975143245193216L;
	private final static long const_arg32 = (long)5883960166769324032L;
	private final static int const_arg33 = (int)-9.8711814E8;
	private final static long const_arg34 = (long)-1052688084268017664L;
	private final static float const_arg35 = (float)6.9030704E7f;
	private final static long const_arg36 = (long)-3364013284780521472L;
	private final static double const_arg37 = (double)9.20242E7;
	private final static int const_arg38 = (int)1.37702106E9;
	private final static short const_arg39 = (short)24879.783;
	private final static byte const_arg40 = (byte)-85.39519;
	private final static int const_arg41 = (int)-1.68733312E9;
	private final static double const_arg42 = (double)3.2349062E7;
	private final static double const_arg43 = (double)7.3861544E7;
	private final static long const_arg44 = (long)-1532836489555326976L;
	private final static byte const_arg45 = (byte)25.961597;
	private final static char const_arg46 = (char)33922.34;
	private final static long const_arg47 = (long)385359843399327744L;
	private final static long const_arg48 = (long)-7557586408342398976L;
	private final static int const_arg49 = (int)1.59989376E9;
	private final static long const_arg50 = (long)-1157996562330238976L;
	private final static float const_arg51 = (float)5.9744684E7f;
	private final static long const_arg52 = (long)8865663534676477952L;
	private final static double const_arg53 = (double)1.3671851E7;
	private final static int const_arg54 = (int)-2.04760947E9;
	private final static short const_arg55 = (short)-4292.53;
	private final static byte const_arg56 = (byte)25.82152;
	private final static int const_arg57 = (int)-4.25327552E8;
	private final static double const_arg58 = (double)5.2456176E7;
	private final static double const_arg59 = (double)8.846028E7;
	private final static long const_arg60 = (long)894138139699785728L;
	private final static byte const_arg61 = (byte)-117.894005;
	private final static char const_arg62 = (char)59286.867;
	private final static long const_arg63 = (long)-2144099025012834304L;
	private final static long const_arg64 = (long)3848515819795462144L;
	private final static int const_arg65 = (int)1.99934733E9;
	private final static long const_arg66 = (long)7659641690613262336L;
	private final static float const_arg67 = (float)5516675.0f;
	private final static long const_arg68 = (long)-7175454108298262528L;
	private final static double const_arg69 = (double)6282143.5;
	private final static int const_arg70 = (int)6.3125722E8;
	private final static short const_arg71 = (short)26729.72;
	private final static byte const_arg72 = (byte)-31.294542;
	private final static int const_arg73 = (int)9.8630128E7;
	private final static double const_arg74 = (double)4904516.5;
	private final static double const_arg75 = (double)2.8933042E7;
	private final static long const_arg76 = (long)-6165387278309316608L;
	private final static byte const_arg77 = (byte)116.00888;
	private final static char const_arg78 = (char)7876.2803;
	private final static long const_arg79 = (long)15691906417793024L;
	private final static long const_arg80 = (long)-4577520954938871808L;
	private final static int const_arg81 = (int)7.4970202E8;
	private final static long const_arg82 = (long)-5202988502162964480L;
	private final static float const_arg83 = (float)1797603.8f;
	private final static long const_arg84 = (long)-1952948710214240256L;
	private final static double const_arg85 = (double)3.1952994E7;
	private final static int const_arg86 = (int)-1.25982106E9;
	private final static short const_arg87 = (short)-28133.2;
	private final static byte const_arg88 = (byte)-22.708725;
	private final static int const_arg89 = (int)-1.60444659E9;
	private final static double const_arg90 = (double)9.7068632E7;
	private final static double const_arg91 = (double)4.3367112E7;
	private final static long const_arg92 = (long)-686509902579492864L;
	private final static byte const_arg93 = (byte)-18.706009;
	private final static char const_arg94 = (char)10218.4795;
	private final static long const_arg95 = (long)-6424251652327706624L;
	private final static long const_arg96 = (long)7457627073118101504L;
	private final static int const_arg97 = (int)7.9667002E8;
	private final static long const_arg98 = (long)6854808083107350528L;
	private final static float const_arg99 = (float)5.8473244E7f;
	private final static long const_arg100 = (long)3756734949877604352L;
	private final static double const_arg101 = (double)2.2191902E7;
	private final static int const_arg102 = (int)-7.4360184E7;
	private final static short const_arg103 = (short)6042.5938;
	private final static byte const_arg104 = (byte)83.2126;
	private final static int const_arg105 = (int)-1.82334362E9;
	private final static double const_arg106 = (double)5.5482588E7;
	private final static double const_arg107 = (double)9.0001464E7;
	private final static long const_arg108 = (long)3249931783055529984L;
	private final static byte const_arg109 = (byte)84.56544;
	private final static char const_arg110 = (char)31136.785;
	private final static long const_arg111 = (long)3516850932204554240L;
	private final static long const_arg112 = (long)-5888803777585096704L;
	private final static int const_arg113 = (int)-1.68064397E9;
	private final static long const_arg114 = (long)-4327247690940416000L;
	private final static float const_arg115 = (float)2.4208068E7f;
	private final static long const_arg116 = (long)6750097495138787328L;
	private final static double const_arg117 = (double)3.473052E7;
	private final static int const_arg118 = (int)5.5497798E8;
	private final static short const_arg119 = (short)-32579.36;
	private final static byte const_arg120 = (byte)73.74936;
	private final static int const_arg121 = (int)2.14067226E9;
	private final static double const_arg122 = (double)3.287431E7;
	private final static double const_arg123 = (double)8.3640952E7;
	private final static long const_arg124 = (long)-8792505295524775936L;
	private final static byte const_arg125 = (byte)44.532955;
	private final static char const_arg126 = (char)37749.37;
	private final static long const_arg127 = (long)-3822851795018663936L;
	private final static long const_arg128 = (long)9196396167515975680L;
	private final static int const_arg129 = (int)1.49046182E9;
	private final static long const_arg130 = (long)-4804481289906941952L;
	private final static float const_arg131 = (float)6.7429768E7f;
	private final static long const_arg132 = (long)7318894466057721856L;
	private final static double const_arg133 = (double)7.322988E7;
	private final static int const_arg134 = (int)-7.8201882E8;
	private final static short const_arg135 = (short)-9883.521;
	private final static byte const_arg136 = (byte)112.27744;
	private final static int const_arg137 = (int)4.09239488E8;
	private final static double const_arg138 = (double)3.1215462E7;
	private final static double const_arg139 = (double)2.3711122E7;
	private final static long const_arg140 = (long)-2525075035488512000L;
	private final static byte const_arg141 = (byte)-54.741833;
	private final static char const_arg142 = (char)55086.42;
	private final static long const_arg143 = (long)2619822279144327168L;
	private final static long const_arg144 = (long)1782600000134451200L;
	private final static int const_arg145 = (int)2.00862758E9;
	private final static long const_arg146 = (long)-7689938984100495360L;
	private final static float const_arg147 = (float)6.395436E7f;
	private final static long const_arg148 = (long)-5331956767576809472L;
	private final static double const_arg149 = (double)1.9876176E7;
	private final static int const_arg150 = (int)-3.57940736E8;
	private final static short const_arg151 = (short)-30176.2;
	private final static byte const_arg152 = (byte)108.62074;
	private final static int const_arg153 = (int)-1.29248717E9;
	private final static double const_arg154 = (double)9.0787024E7;
	private final static double const_arg155 = (double)5.3953652E7;
	private final static long const_arg156 = (long)-7766467616506161152L;
	private final static byte const_arg157 = (byte)101.02202;
	private final static char const_arg158 = (char)11022.495;
	private final static long const_arg159 = (long)-8111151907935979520L;
	private final static long const_arg160 = (long)5590287327717490688L;
	private final static int const_arg161 = (int)3.34640736E8;
	private final static long const_arg162 = (long)-1060203494556989440L;
	private final static float const_arg163 = (float)4.1251936E7f;
	private final static long const_arg164 = (long)-3607266653153327104L;
	private final static double const_arg165 = (double)3.086726E7;
	private final static int const_arg166 = (int)8.7350944E8;
	private final static short const_arg167 = (short)-7758.0845;
	private final static byte const_arg168 = (byte)6.8055143;
	private final static int const_arg169 = (int)-1.75682995E9;

	/* Verify each argument against an expected constant */
	public static void test(long arg0, int arg1, long arg2, float arg3, long arg4, double arg5, int arg6, short arg7, byte arg8, int arg9, double arg10, double arg11, long arg12, byte arg13, char arg14, long arg15, long arg16, int arg17, long arg18, float arg19, long arg20, double arg21, int arg22, short arg23, byte arg24, int arg25, double arg26, double arg27, long arg28, byte arg29, char arg30, long arg31, long arg32, int arg33, long arg34, float arg35, long arg36, double arg37, int arg38, short arg39, byte arg40, int arg41, double arg42, double arg43, long arg44, byte arg45, char arg46, long arg47, long arg48, int arg49, long arg50, float arg51, long arg52, double arg53, int arg54, short arg55, byte arg56, int arg57, double arg58, double arg59, long arg60, byte arg61, char arg62, long arg63, long arg64, int arg65, long arg66, float arg67, long arg68, double arg69, int arg70, short arg71, byte arg72, int arg73, double arg74, double arg75, long arg76, byte arg77, char arg78, long arg79, long arg80, int arg81, long arg82, float arg83, long arg84, double arg85, int arg86, short arg87, byte arg88, int arg89, double arg90, double arg91, long arg92, byte arg93, char arg94, long arg95, long arg96, int arg97, long arg98, float arg99, long arg100, double arg101, int arg102, short arg103, byte arg104, int arg105, double arg106, double arg107, long arg108, byte arg109, char arg110, long arg111, long arg112, int arg113, long arg114, float arg115, long arg116, double arg117, int arg118, short arg119, byte arg120, int arg121, double arg122, double arg123, long arg124, byte arg125, char arg126, long arg127, long arg128, int arg129, long arg130, float arg131, long arg132, double arg133, int arg134, short arg135, byte arg136, int arg137, double arg138, double arg139, long arg140, byte arg141, char arg142, long arg143, long arg144, int arg145, long arg146, float arg147, long arg148, double arg149, int arg150, short arg151, byte arg152, int arg153, double arg154, double arg155, long arg156, byte arg157, char arg158, long arg159, long arg160, int arg161, long arg162, float arg163, long arg164, double arg165, int arg166, short arg167, byte arg168, int arg169) throws IllegalArgumentException {
		int index = 0;
		if(arg0 != const_arg0) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg1 != const_arg1) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg2 != const_arg2) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg3 != const_arg3) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg4 != const_arg4) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg5 != const_arg5) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg6 != const_arg6) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg7 != const_arg7) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg8 != const_arg8) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg9 != const_arg9) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg10 != const_arg10) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg11 != const_arg11) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg12 != const_arg12) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg13 != const_arg13) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg14 != const_arg14) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg15 != const_arg15) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg16 != const_arg16) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg17 != const_arg17) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg18 != const_arg18) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg19 != const_arg19) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg20 != const_arg20) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg21 != const_arg21) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg22 != const_arg22) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg23 != const_arg23) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg24 != const_arg24) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg25 != const_arg25) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg26 != const_arg26) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg27 != const_arg27) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg28 != const_arg28) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg29 != const_arg29) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg30 != const_arg30) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg31 != const_arg31) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg32 != const_arg32) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg33 != const_arg33) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg34 != const_arg34) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg35 != const_arg35) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg36 != const_arg36) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg37 != const_arg37) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg38 != const_arg38) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg39 != const_arg39) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg40 != const_arg40) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg41 != const_arg41) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg42 != const_arg42) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg43 != const_arg43) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg44 != const_arg44) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg45 != const_arg45) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg46 != const_arg46) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg47 != const_arg47) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg48 != const_arg48) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg49 != const_arg49) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg50 != const_arg50) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg51 != const_arg51) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg52 != const_arg52) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg53 != const_arg53) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg54 != const_arg54) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg55 != const_arg55) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg56 != const_arg56) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg57 != const_arg57) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg58 != const_arg58) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg59 != const_arg59) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg60 != const_arg60) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg61 != const_arg61) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg62 != const_arg62) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg63 != const_arg63) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg64 != const_arg64) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg65 != const_arg65) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg66 != const_arg66) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg67 != const_arg67) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg68 != const_arg68) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg69 != const_arg69) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg70 != const_arg70) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg71 != const_arg71) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg72 != const_arg72) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg73 != const_arg73) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg74 != const_arg74) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg75 != const_arg75) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg76 != const_arg76) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg77 != const_arg77) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg78 != const_arg78) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg79 != const_arg79) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg80 != const_arg80) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg81 != const_arg81) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg82 != const_arg82) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg83 != const_arg83) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg84 != const_arg84) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg85 != const_arg85) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg86 != const_arg86) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg87 != const_arg87) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg88 != const_arg88) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg89 != const_arg89) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg90 != const_arg90) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg91 != const_arg91) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg92 != const_arg92) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg93 != const_arg93) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg94 != const_arg94) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg95 != const_arg95) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg96 != const_arg96) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg97 != const_arg97) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg98 != const_arg98) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg99 != const_arg99) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg100 != const_arg100) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg101 != const_arg101) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg102 != const_arg102) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg103 != const_arg103) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg104 != const_arg104) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg105 != const_arg105) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg106 != const_arg106) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg107 != const_arg107) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg108 != const_arg108) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg109 != const_arg109) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg110 != const_arg110) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg111 != const_arg111) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg112 != const_arg112) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg113 != const_arg113) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg114 != const_arg114) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg115 != const_arg115) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg116 != const_arg116) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg117 != const_arg117) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg118 != const_arg118) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg119 != const_arg119) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg120 != const_arg120) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg121 != const_arg121) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg122 != const_arg122) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg123 != const_arg123) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg124 != const_arg124) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg125 != const_arg125) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg126 != const_arg126) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg127 != const_arg127) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg128 != const_arg128) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg129 != const_arg129) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg130 != const_arg130) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg131 != const_arg131) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg132 != const_arg132) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg133 != const_arg133) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg134 != const_arg134) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg135 != const_arg135) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg136 != const_arg136) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg137 != const_arg137) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg138 != const_arg138) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg139 != const_arg139) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg140 != const_arg140) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg141 != const_arg141) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg142 != const_arg142) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg143 != const_arg143) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg144 != const_arg144) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg145 != const_arg145) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg146 != const_arg146) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg147 != const_arg147) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg148 != const_arg148) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg149 != const_arg149) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg150 != const_arg150) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg151 != const_arg151) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg152 != const_arg152) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg153 != const_arg153) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg154 != const_arg154) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg155 != const_arg155) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg156 != const_arg156) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg157 != const_arg157) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg158 != const_arg158) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg159 != const_arg159) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg160 != const_arg160) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg161 != const_arg161) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg162 != const_arg162) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg163 != const_arg163) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg164 != const_arg164) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg165 != const_arg165) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg166 != const_arg166) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg167 != const_arg167) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg168 != const_arg168) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
		index++;
		if(arg169 != const_arg169) {
			throw new IllegalArgumentException("Unexpected argument at index "+index);
		}
	}

	/**
	 * Stress test permuteArguments by making call using the maximum number of stack slots (255). Keep in mind
	 * that the receiver consumes a single stack slot so only 254 slots are available.  The method test
	 * (found within this class) compares each argument with an expected value.  In essence, this test
	 * reverses the argument order and verifies the results.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testPermuteHandleMaximumArguments() throws Throwable {
		// Describe the test function
		MethodType mt = methodType(void.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class, double.class, double.class, long.class, byte.class, char.class, long.class, long.class, int.class, long.class, float.class, long.class, double.class, int.class, short.class, byte.class, int.class);

		// Describe the test function
		MethodType rmt = methodType(void.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class, long.class, char.class, byte.class, long.class, double.class, double.class, int.class, byte.class, short.class, int.class, double.class, long.class, float.class, long.class, int.class, long.class);

		// Lookup and create the destination function MethodHandle
		MethodHandle mh = MethodHandles.publicLookup().findStatic(PermuteTest.class, "test", mt);

		// Permute the arguments
		mh = MethodHandles.permuteArguments(mh, rmt, 169, 168, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157, 156, 155, 154, 153, 152, 151, 150, 149, 148, 147, 146, 145, 144, 143, 142, 141, 140, 139, 138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

		boolean exceptionThrown = false;
		
		try {
			// Invoke the method handle, which returns the argument index which didn't match what it expected, or -1 on success
			mh.invoke(const_arg169, const_arg168, const_arg167, const_arg166, const_arg165, const_arg164, const_arg163, const_arg162, const_arg161, const_arg160, const_arg159, const_arg158, const_arg157, const_arg156, const_arg155, const_arg154, const_arg153, const_arg152, const_arg151, const_arg150, const_arg149, const_arg148, const_arg147, const_arg146, const_arg145, const_arg144, const_arg143, const_arg142, const_arg141, const_arg140, const_arg139, const_arg138, const_arg137, const_arg136, const_arg135, const_arg134, const_arg133, const_arg132, const_arg131, const_arg130, const_arg129, const_arg128, const_arg127, const_arg126, const_arg125, const_arg124, const_arg123, const_arg122, const_arg121, const_arg120, const_arg119, const_arg118, const_arg117, const_arg116, const_arg115, const_arg114, const_arg113, const_arg112, const_arg111, const_arg110, const_arg109, const_arg108, const_arg107, const_arg106, const_arg105, const_arg104, const_arg103, const_arg102, const_arg101, const_arg100, const_arg99, const_arg98, const_arg97, const_arg96, const_arg95, const_arg94, const_arg93, const_arg92, const_arg91, const_arg90, const_arg89, const_arg88, const_arg87, const_arg86, const_arg85, const_arg84, const_arg83, const_arg82, const_arg81, const_arg80, const_arg79, const_arg78, const_arg77, const_arg76, const_arg75, const_arg74, const_arg73, const_arg72, const_arg71, const_arg70, const_arg69, const_arg68, const_arg67, const_arg66, const_arg65, const_arg64, const_arg63, const_arg62, const_arg61, const_arg60, const_arg59, const_arg58, const_arg57, const_arg56, const_arg55, const_arg54, const_arg53, const_arg52, const_arg51, const_arg50, const_arg49, const_arg48, const_arg47, const_arg46, const_arg45, const_arg44, const_arg43, const_arg42, const_arg41, const_arg40, const_arg39, const_arg38, const_arg37, const_arg36, const_arg35, const_arg34, const_arg33, const_arg32, const_arg31, const_arg30, const_arg29, const_arg28, const_arg27, const_arg26, const_arg25, const_arg24, const_arg23, const_arg22, const_arg21, const_arg20, const_arg19, const_arg18, const_arg17, const_arg16, const_arg15, const_arg14, const_arg13, const_arg12, const_arg11, const_arg10, const_arg9, const_arg8, const_arg7, const_arg6, const_arg5, const_arg4, const_arg3, const_arg2, const_arg1, const_arg0);
		} catch (Exception e) {
			e.printStackTrace();
			exceptionThrown = true;
		}
		
		AssertJUnit.assertFalse(exceptionThrown);
}
	
	/**
	 * Do a series of simple tests to permute arguments in various ways.  The test builds on top of each other
	 * and does a series of drops and duplications, intending to stress test argument permutation.  Double
	 * slot and single slot arguments are mixed in an attempt to find any possible issues.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testPermuteArguments() throws Throwable {
		MethodType I_I_type = methodType(int.class, int.class);
		MethodType II_I_type = methodType(int.class, int.class, int.class);

		MethodHandle sub = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "subtract", II_I_type);
		AssertJUnit.assertTrue(sub.type().equals(II_I_type));

		MethodHandle rsub = MethodHandles.permuteArguments(sub, II_I_type, 1, 0);
		AssertJUnit.assertEquals(4, (int)rsub.invokeExact(1, 5));

		MethodHandle add = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", II_I_type);
		AssertJUnit.assertEquals(II_I_type, add.type());

		MethodHandle twice = MethodHandles.permuteArguments(add, I_I_type, 0, 0);
		AssertJUnit.assertEquals(I_I_type, twice.type());
		AssertJUnit.assertEquals(14, (int)twice.invokeExact(7));

		// Create an identity handle
		MethodHandle iIdent = MethodHandles.identity(int.class);
		MethodHandle dIdent = MethodHandles.identity(double.class);

		// Create permute handle that takes a double and an int, and drops the double
		MethodType DI_I_type = methodType(int.class, double.class, int.class);
		MethodHandle diiHandle = MethodHandles.permuteArguments(iIdent, DI_I_type, 1);
		AssertJUnit.assertEquals(1, diiHandle.invoke(1.2,1));

		// Create a permute handle that takes a double and an int, and drops the int
		MethodType DI_D_type = methodType(double.class, double.class, int.class);
		MethodHandle didHandle = MethodHandles.permuteArguments(dIdent, DI_D_type, 0);
		AssertJUnit.assertEquals(1.2, didHandle.invoke(1.2,1));

		// Permutes IDI to DI
		MethodType IDI_D_type = methodType(double.class, int.class, double.class, int.class);
		MethodHandle idid = MethodHandles.permuteArguments(didHandle, IDI_D_type, 1,0);
		AssertJUnit.assertEquals(1.2, idid.invoke(Integer.MAX_VALUE,1.2,1));

		// Permute DID to IDI
		MethodType DID_D_type = methodType(double.class, double.class, int.class, double.class);
		MethodHandle didd = MethodHandles.permuteArguments(idid, DID_D_type, 1,0,1);
		AssertJUnit.assertEquals(1.2, didd.invoke(1.2,1,1.9));

		// Permutes IDDDD to DID
		MethodType IDDDD_D_type = methodType(double.class, int.class, double.class,double.class,double.class,double.class);
		MethodHandle iddddd = MethodHandles.permuteArguments(didd, IDDDD_D_type, 3,0,2);
		AssertJUnit.assertEquals(1.9, iddddd.invoke(9,1.2,88.5,1.9,23.1));

	}

	/* Used in testFirstArgument, testMiddleArgument, testLastArgument */
	private static MethodType getMethodType() {
		return MethodType.methodType(double.class, long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class, 
				double.class, 
				long.class, 
				byte.class, 
				char.class, 
				long.class, 
				long.class, 
				int.class, 
				long.class, 
				float.class, 
				long.class, 
				double.class, 
				int.class, 
				short.class, 
				byte.class, 
				int.class, 
				double.class);
	}

	/**
	 * Use permuteArguments to drop all arguments except for an arbitrary double argument in
	 * the missing of the call and use MethodHandles.identity to return that value.  
	 * Compare it to an expected value. The actual argument picked is the 8th double to
	 * be called.
	 * 
	 * There are a series of comments next to each argument.  This is the number of slots that
	 * argument occupies, and the total consumed slots thus far.  The maximum number of slots
	 * per function call is 255.  Remember, receiver consumes one slot, so the maximum allowed
	 * in this test is 254.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testMiddleArgument() throws Throwable{

		MethodType mt = getMethodType();

		MethodHandle ident = MethodHandles.identity(double.class);

		MethodHandle mh = MethodHandles.permuteArguments(ident, mt, 41);

		AssertJUnit.assertEquals(1.8, mh.invoke((long)-3203180362171660288L, 		// num: 2 total: 2
				(float)7.171048E7f, 		// num: 1 total: 3
				(long)5339160688581042176L, 		// num: 2 total: 5
				(double)8.9554576E7, 		// num: 2 total: 7
				(int)1.25104589E9, 		// num: 1 total: 8
				(short)7843.807, 		// num: 1 total: 9
				(byte)33.169678, 		// num: 1 total: 10
				(int)-1.55029389E9, 		// num: 1 total: 11
				(double)5.503734E7, 		// num: 2 total: 13
				(double)8.9038912E7, 		// num: 2 total: 15
				(long)-5169201657267570688L, 		// num: 2 total: 17
				(byte)54.93014, 		// num: 1 total: 18
				(char)7055.623, 		// num: 1 total: 19
				(long)-3497224257527621632L, 		// num: 2 total: 21
				(long)5878185161578051584L, 		// num: 2 total: 23
				(int)1.50359232E9, 		// num: 1 total: 24
				(long)3380793726609856512L, 		// num: 2 total: 26
				(float)4.168188E7f, 		// num: 1 total: 27
				(long)-3993298687727976448L, 		// num: 2 total: 29
				(double)7.2788312E7, 		// num: 2 total: 31
				(int)-1.77448294E9, 		// num: 1 total: 32
				(short)-9754.77, 		// num: 1 total: 33
				(byte)9.254554, 		// num: 1 total: 34
				(int)-3.70784224E8, 		// num: 1 total: 35
				(double)6.2799244E7, 		// num: 2 total: 37
				(double)4.2424844E7, 		// num: 2 total: 39
				(long)-3893495019984762880L, 		// num: 2 total: 41
				(byte)33.833794, 		// num: 1 total: 42
				(char)52790.406, 		// num: 1 total: 43
				(long)-7255662582137067520L, 		// num: 2 total: 45
				(long)-985054663470168064L, 		// num: 2 total: 47
				(int)3.6900096E8, 		// num: 1 total: 48
				(long)2209057434292498432L, 		// num: 2 total: 50
				(float)4.2143004E7f, 		// num: 1 total: 51
				(long)-4993207563018072064L, 		// num: 2 total: 53
				(double)1.482387E7, 		// num: 2 total: 55
				(int)-6.0968768E7, 		// num: 1 total: 56
				(short)-15536.124, 		// num: 1 total: 57
				(byte)111.127, 		// num: 1 total: 58
				(int)2.08704154E9, 		// num: 1 total: 59
				(double)8.9123904E7, 		// num: 2 total: 61
				1.8, 		// num: 2 total: 63
				(long)9142906052754137088L, 		// num: 2 total: 65
				(byte)115.29249, 		// num: 1 total: 66
				(char)54774.516, 		// num: 1 total: 67
				(long)-6693027720563402752L, 		// num: 2 total: 69
				(long)5457968159355297792L, 		// num: 2 total: 71
				(int)4.338736E8, 		// num: 1 total: 72
				(long)7821156722567321600L, 		// num: 2 total: 74
				(float)7.2992296E7f, 		// num: 1 total: 75
				(long)1704228185344000000L, 		// num: 2 total: 77
				(double)1.1575101E7, 		// num: 2 total: 79
				(int)1.52252186E9, 		// num: 1 total: 80
				(short)-16456.012, 		// num: 1 total: 81
				(byte)-93.99391, 		// num: 1 total: 82
				(int)-1.53191872E9, 		// num: 1 total: 83
				(double)5.0222204E7, 		// num: 2 total: 85
				(double)8.0645312E7, 		// num: 2 total: 87
				(long)2995670601650804736L, 		// num: 2 total: 89
				(byte)84.732414, 		// num: 1 total: 90
				(char)27034.49, 		// num: 1 total: 91
				(long)676881792554905600L, 		// num: 2 total: 93
				(long)7764068144727302144L, 		// num: 2 total: 95
				(int)1.47474957E9, 		// num: 1 total: 96
				(long)8494457029451546624L, 		// num: 2 total: 98
				(float)9.0300152E7f, 		// num: 1 total: 99
				(long)3891569714658539520L, 		// num: 2 total: 101
				(double)2.70031E7, 		// num: 2 total: 103
				(int)-1.90898163E9, 		// num: 1 total: 104
				(short)32598.158, 		// num: 1 total: 105
				(byte)-114.99583, 		// num: 1 total: 106
				(int)1.52044352E9, 		// num: 1 total: 107
				(double)4.9004188E7, 		// num: 2 total: 109
				(double)4.2048228E7, 		// num: 2 total: 111
				(long)-8579726908909932544L, 		// num: 2 total: 113
				(byte)11.572607, 		// num: 1 total: 114
				(char)8244.297, 		// num: 1 total: 115
				(long)5092247331817613312L, 		// num: 2 total: 117
				(long)6760240151782103040L, 		// num: 2 total: 119
				(int)1.80054746E9, 		// num: 1 total: 120
				(long)8997645018063001600L, 		// num: 2 total: 122
				(float)2.935457E7f, 		// num: 1 total: 123
				(long)-5863345824712359936L, 		// num: 2 total: 125
				(double)7.1900248E7, 		// num: 2 total: 127
				(int)2.11336026E9, 		// num: 1 total: 128
				(short)6451.464, 		// num: 1 total: 129
				(byte)49.917686, 		// num: 1 total: 130
				(int)-7.6394688E8, 		// num: 1 total: 131
				(double)4.7346408E7, 		// num: 2 total: 133
				(double)1.92255E7, 		// num: 2 total: 135
				(long)-1487336717242705920L, 		// num: 2 total: 137
				(byte)54.120953, 		// num: 1 total: 138
				(char)58817.887, 		// num: 1 total: 139
				(long)5057081444118452224L, 		// num: 2 total: 141
				(long)-6562681839230472192L, 		// num: 2 total: 143
				(int)-1.58745318E9, 		// num: 1 total: 144
				(long)-6584736502456768512L, 		// num: 2 total: 146
				(float)1.3935081E7f, 		// num: 1 total: 147
				(long)4472217696850677760L, 		// num: 2 total: 149
				(double)4.6066192E7, 		// num: 2 total: 151
				(int)1.01984912E8, 		// num: 1 total: 152
				(short)-31271.809, 		// num: 1 total: 153
				(byte)65.936165, 		// num: 1 total: 154
				(int)-1.90919245E9, 		// num: 1 total: 155
				(double)2.8007106E7, 		// num: 2 total: 157
				(double)9.2201056E7, 		// num: 2 total: 159
				(long)-5253666077784686592L, 		// num: 2 total: 161
				(byte)30.762087, 		// num: 1 total: 162
				(char)40515.258, 		// num: 1 total: 163
				(long)2372213227409434624L, 		// num: 2 total: 165
				(long)-8181293946630713344L, 		// num: 2 total: 167
				(int)-3.3647364E7, 		// num: 1 total: 168
				(long)-27132565736196096L, 		// num: 2 total: 170
				(float)9.2700896E7f, 		// num: 1 total: 171
				(long)3164464155936280576L, 		// num: 2 total: 173
				(double)6.458284E7, 		// num: 2 total: 175
				(int)-9.3258112E8, 		// num: 1 total: 176
				(short)11517.835, 		// num: 1 total: 177
				(byte)119.96983, 		// num: 1 total: 178
				(int)2.346968E8, 		// num: 1 total: 179
				(double)3927204.0, 		// num: 2 total: 181
				(double)2.7818502E7, 		// num: 2 total: 183
				(long)-8005666839323181056L, 		// num: 2 total: 185
				(byte)-114.85905, 		// num: 1 total: 186
				(char)50565.82, 		// num: 1 total: 187
				(long)-2394771929317384192L, 		// num: 2 total: 189
				(long)-4814531126618560512L, 		// num: 2 total: 191
				(int)-1.39272538E9, 		// num: 1 total: 192
				(long)-7508656545357506560L, 		// num: 2 total: 194
				(float)4.0512312E7f, 		// num: 1 total: 195
				(long)-9075974017665474560L, 		// num: 2 total: 197
				(double)1.6997122E7, 		// num: 2 total: 199
				(int)1.69337165E9, 		// num: 1 total: 200
				(short)13108.793, 		// num: 1 total: 201
				(byte)-56.074783, 		// num: 1 total: 202
				(int)3.6717792E8, 		// num: 1 total: 203
				(double)3.7203064E7, 		// num: 2 total: 205
				(double)4.2195324E7, 		// num: 2 total: 207
				(long)-4338933738822316032L, 		// num: 2 total: 209
				(byte)64.68621, 		// num: 1 total: 210
				(char)24572.482, 		// num: 1 total: 211
				(long)8114436875812677632L, 		// num: 2 total: 213
				(long)1524724838129885184L, 		// num: 2 total: 215
				(int)1.01250253E9, 		// num: 1 total: 216
				(long)-1761847386827798528L, 		// num: 2 total: 218
				(float)9.4517736E7f, 		// num: 1 total: 219
				(long)-4268109591618652160L, 		// num: 2 total: 221
				(double)1.7519344E7, 		// num: 2 total: 223
				(int)1.12439603E9, 		// num: 1 total: 224
				(short)16512.586, 		// num: 1 total: 225
				(byte)38.35186, 		// num: 1 total: 226
				(int)-5.6585916E7, 		// num: 1 total: 227
				(double)2.7199912E7, 		// num: 2 total: 229
				(double)2.2942766E7, 		// num: 2 total: 231
				(long)7614703348831082496L, 		// num: 2 total: 233
				(byte)-100.02652, 		// num: 1 total: 234
				(char)48268.863, 		// num: 1 total: 235
				(long)7169779744517623808L, 		// num: 2 total: 237
				(long)-8595974293885444096L, 		// num: 2 total: 239
				(int)5.2847568E8, 		// num: 1 total: 240
				(long)4890038659257524224L, 		// num: 2 total: 242
				(float)7.5006352E7f, 		// num: 1 total: 243
				(long)-6049589841802512384L, 		// num: 2 total: 245
				(double)9.6237176E7, 		// num: 2 total: 247
				(int)1.80856384E9, 		// num: 1 total: 248
				(short)16381.117, 		// num: 1 total: 249
				(byte)123.96007, 		// num: 1 total: 250
				(int)-2.09081229E9, 		// num: 1 total: 251
				(double)1.780585E7)); 		// num: 2 total: 253

	}

	/**
	 * Use permuteArguments to drop all arguments except for the first double argument and use
	 * MethodHandles.identity to return that value.  Compare it to an expected value.
	 * 
	 * There are a series of comments next to each argument.  This is the number of slots that
	 * argument occupies, and the total consumed slots thus far.  The maximum number of slots
	 * per function call is 255.  Remember, receiver consumes one slot, so the maximum allowed
	 * in this test is 254.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testFirstArgument() throws Throwable {
		MethodType mt = getMethodType();

		MethodHandle ident = MethodHandles.identity(double.class);

		MethodHandle mh = MethodHandles.permuteArguments(ident, mt, 3);

		AssertJUnit.assertEquals(1.99, mh.invoke((long)2057049365891047424L, 		// num: 2 total: 2
				(float)3.3248204E7f, 		// num: 1 total: 3
				(long)-1596658795522656256L, 		// num: 2 total: 5
				1.99, 		// num: 2 total: 7
				(int)1.2417568E8, 		// num: 1 total: 8
				(short)-13116.136, 		// num: 1 total: 9
				(byte)69.57229, 		// num: 1 total: 10
				(int)-7.4886736E7, 		// num: 1 total: 11
				(double)2.162346E7, 		// num: 2 total: 13
				(double)4.1026428E7, 		// num: 2 total: 15
				(long)6750435188124798976L, 		// num: 2 total: 17
				(byte)79.430435, 		// num: 1 total: 18
				(char)30069.264, 		// num: 1 total: 19
				(long)-7457808532033626112L, 		// num: 2 total: 21
				(long)3545381522114433024L, 		// num: 2 total: 23
				(int)-1.23332186E9, 		// num: 1 total: 24
				(long)4395216543246026752L, 		// num: 2 total: 26
				(float)4.3141664E7f, 		// num: 1 total: 27
				(long)-4902263574143195136L, 		// num: 2 total: 29
				(double)3.5635228E7, 		// num: 2 total: 31
				(int)7.6856237E8, 		// num: 1 total: 32
				(short)-4313.166, 		// num: 1 total: 33
				(byte)-56.533463, 		// num: 1 total: 34
				(int)1.01504282E9, 		// num: 1 total: 35
				(double)3.0608242E7, 		// num: 2 total: 37
				(double)3.076918E7, 		// num: 2 total: 39
				(long)-6303678274520227840L, 		// num: 2 total: 41
				(byte)-70.90424, 		// num: 1 total: 42
				(char)65425.832, 		// num: 1 total: 43
				(long)6789869740212318208L, 		// num: 2 total: 45
				(long)-2743799026798000128L, 		// num: 2 total: 47
				(int)1.46388173E9, 		// num: 1 total: 48
				(long)4892142575604934656L, 		// num: 2 total: 50
				(float)5.1981712E7f, 		// num: 1 total: 51
				(long)-6693873758359072768L, 		// num: 2 total: 53
				(double)1.4271397E7, 		// num: 2 total: 55
				(int)3.18325536E8, 		// num: 1 total: 56
				(short)6388.5635, 		// num: 1 total: 57
				(byte)64.90687, 		// num: 1 total: 58
				(int)1.94001677E9, 		// num: 1 total: 59
				(double)3.2802208E7, 		// num: 2 total: 61
				(double)3.6718356E7, 		// num: 2 total: 63
				(long)-4565957498366681088L, 		// num: 2 total: 65
				(byte)-116.441345, 		// num: 1 total: 66
				(char)49257.3, 		// num: 1 total: 67
				(long)-190734580375732224L, 		// num: 2 total: 69
				(long)-8308012861219088384L, 		// num: 2 total: 71
				(int)1.31992819E9, 		// num: 1 total: 72
				(long)7060407783806167040L, 		// num: 2 total: 74
				(float)2.8768634E7f, 		// num: 1 total: 75
				(long)800076918289551360L, 		// num: 2 total: 77
				(double)7.7681576E7, 		// num: 2 total: 79
				(int)-1.78129728E9, 		// num: 1 total: 80
				(short)15143.424, 		// num: 1 total: 81
				(byte)-11.566322, 		// num: 1 total: 82
				(int)-2.2967126E7, 		// num: 1 total: 83
				(double)5.0063668E7, 		// num: 2 total: 85
				(double)6.5565364E7, 		// num: 2 total: 87
				(long)-249808883378114560L, 		// num: 2 total: 89
				(byte)-118.25458, 		// num: 1 total: 90
				(char)41799.008, 		// num: 1 total: 91
				(long)-2472853930697168896L, 		// num: 2 total: 93
				(long)5766316152834109440L, 		// num: 2 total: 95
				(int)-1.45615667E9, 		// num: 1 total: 96
				(long)-2504792983542507520L, 		// num: 2 total: 98
				(float)4.3677008E7f, 		// num: 1 total: 99
				(long)-2453858987052083200L, 		// num: 2 total: 101
				(double)828030.25, 		// num: 2 total: 103
				(int)1.74248653E9, 		// num: 1 total: 104
				(short)-18037.021, 		// num: 1 total: 105
				(byte)-32.173225, 		// num: 1 total: 106
				(int)2.0116425E9, 		// num: 1 total: 107
				(double)5.890718E7, 		// num: 2 total: 109
				(double)3.2890086E7, 		// num: 2 total: 111
				(long)1393861424270131200L, 		// num: 2 total: 113
				(byte)-42.60434, 		// num: 1 total: 114
				(char)63438.91, 		// num: 1 total: 115
				(long)9171394038351972352L, 		// num: 2 total: 117
				(long)3292422773839065088L, 		// num: 2 total: 119
				(int)-1.68562867E9, 		// num: 1 total: 120
				(long)2987634792627372032L, 		// num: 2 total: 122
				(float)2.6772356E7f, 		// num: 1 total: 123
				(long)-7082658629598701568L, 		// num: 2 total: 125
				(double)4.3756608E7, 		// num: 2 total: 127
				(int)8.8829542E8, 		// num: 1 total: 128
				(short)-3108.5532, 		// num: 1 total: 129
				(byte)82.47665, 		// num: 1 total: 130
				(int)1.87625677E9, 		// num: 1 total: 131
				(double)4.6780996E7, 		// num: 2 total: 133
				(double)4.260726E7, 		// num: 2 total: 135
				(long)6630347734336305152L, 		// num: 2 total: 137
				(byte)-48.303013, 		// num: 1 total: 138
				(char)12880.834, 		// num: 1 total: 139
				(long)-8063673605991550976L, 		// num: 2 total: 141
				(long)7370691734393360384L, 		// num: 2 total: 143
				(int)2.73375264E8, 		// num: 1 total: 144
				(long)-6856089306008756224L, 		// num: 2 total: 146
				(float)8590396.0f, 		// num: 1 total: 147
				(long)-3151823057703292928L, 		// num: 2 total: 149
				(double)2.816233E7, 		// num: 2 total: 151
				(int)4.72372608E8, 		// num: 1 total: 152
				(short)2027.9454, 		// num: 1 total: 153
				(byte)-29.623098, 		// num: 1 total: 154
				(int)-2.84620288E8, 		// num: 1 total: 155
				(double)1.248518E7, 		// num: 2 total: 157
				(double)1178537.6, 		// num: 2 total: 159
				(long)-8249954297444534272L, 		// num: 2 total: 161
				(byte)-31.99032, 		// num: 1 total: 162
				(char)15637.167, 		// num: 1 total: 163
				(long)8834424337385818112L, 		// num: 2 total: 165
				(long)5016490523928459264L, 		// num: 2 total: 167
				(int)1.05513254E9, 		// num: 1 total: 168
				(long)-1327013163988903936L, 		// num: 2 total: 170
				(float)2.0481882E7f, 		// num: 1 total: 171
				(long)4896746503026331648L, 		// num: 2 total: 173
				(double)2.9970542E7, 		// num: 2 total: 175
				(int)-1.0775028E8, 		// num: 1 total: 176
				(short)19653.012, 		// num: 1 total: 177
				(byte)60.237724, 		// num: 1 total: 178
				(int)1.10074086E9, 		// num: 1 total: 179
				(double)6.0730316E7, 		// num: 2 total: 181
				(double)1.6590137E7, 		// num: 2 total: 183
				(long)9223361597289562112L, 		// num: 2 total: 185
				(byte)-88.818985, 		// num: 1 total: 186
				(char)55988.465, 		// num: 1 total: 187
				(long)-1337964432311511040L, 		// num: 2 total: 189
				(long)1739855928065984512L, 		// num: 2 total: 191
				(int)-8.8588979E8, 		// num: 1 total: 192
				(long)-6602693899564214272L, 		// num: 2 total: 194
				(float)4723068.5f, 		// num: 1 total: 195
				(long)6437359138031542272L, 		// num: 2 total: 197
				(double)3.7933724E7, 		// num: 2 total: 199
				(int)1.9166856E7, 		// num: 1 total: 200
				(short)1736.8943, 		// num: 1 total: 201
				(byte)95.22048, 		// num: 1 total: 202
				(int)-1.95618778E9, 		// num: 1 total: 203
				(double)4.0836832E7, 		// num: 2 total: 205
				(double)3.2096132E7, 		// num: 2 total: 207
				(long)8003145997316270080L, 		// num: 2 total: 209
				(byte)23.949598, 		// num: 1 total: 210
				(char)39272.504, 		// num: 1 total: 211
				(long)-2594099861682894848L, 		// num: 2 total: 213
				(long)-4507267283947671552L, 		// num: 2 total: 215
				(int)-2.96264608E8, 		// num: 1 total: 216
				(long)-8068065524624621568L, 		// num: 2 total: 218
				(float)8.4391816E7f, 		// num: 1 total: 219
				(long)6760260154867468288L, 		// num: 2 total: 221
				(double)2.8243924E7, 		// num: 2 total: 223
				(int)7.120311E8, 		// num: 1 total: 224
				(short)-11052.873, 		// num: 1 total: 225
				(byte)-79.20657, 		// num: 1 total: 226
				(int)-1.52229798E9, 		// num: 1 total: 227
				(double)7.9574984E7, 		// num: 2 total: 229
				(double)4.4727352E7, 		// num: 2 total: 231
				(long)4387323700686755840L, 		// num: 2 total: 233
				(byte)-2.0605624, 		// num: 1 total: 234
				(char)55130.258, 		// num: 1 total: 235
				(long)8657139882764234752L, 		// num: 2 total: 237
				(long)8334347915311243264L, 		// num: 2 total: 239
				(int)6.7880627E8, 		// num: 1 total: 240
				(long)3320568648412581888L, 		// num: 2 total: 242
				(float)4.3009324E7f, 		// num: 1 total: 243
				(long)-1485130604004706304L, 		// num: 2 total: 245
				(double)5160794.0, 		// num: 2 total: 247
				(int)-3.09672768E8, 		// num: 1 total: 248
				(short)22107.887, 		// num: 1 total: 249
				(byte)114.208435, 		// num: 1 total: 250
				(int)-1.01827258E9, 		// num: 1 total: 251
				(double)5.8145452E7)); 		// num: 2 total: 253

	}
	
	/**
	 * Use permuteArguments to drop all arguments except for the last double argument and use
	 * MethodHandles.identity to return that value.  Compare it to an expected value.
	 * 
	 * There are a series of comments next to each argument.  This is the number of slots that
	 * argument occupies, and the total consumed slots thus far.  The maximum number of slots
	 * per function call is 255.  Remember, receiver consumes one slot, so the maximum allowed
	 * in this test is 254.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLastArgument() throws Throwable {
		MethodType mt = getMethodType();

		MethodHandle ident = MethodHandles.identity(double.class);

		MethodHandle mh = MethodHandles.permuteArguments(ident, mt, 168);

		AssertJUnit.assertEquals((double)5.6475464E7, mh.invoke((long)1580137349297076224L, 		// num: 2 total: 2
				(float)7.1264104E7f, 		// num: 1 total: 3
				(long)7396342793090865152L, 		// num: 2 total: 5
				(double)3.4365352E7, 		// num: 2 total: 7
				(int)-1.53093645E9, 		// num: 1 total: 8
				(short)2026.8043, 		// num: 1 total: 9
				(byte)-5.027892, 		// num: 1 total: 10
				(int)-5.9936819E8, 		// num: 1 total: 11
				(double)6.3947464E7, 		// num: 2 total: 13
				(double)6.3012412E7, 		// num: 2 total: 15
				(long)1412149259284840448L, 		// num: 2 total: 17
				(byte)-47.496567, 		// num: 1 total: 18
				(char)2716.6106, 		// num: 1 total: 19
				(long)2430549790276419584L, 		// num: 2 total: 21
				(long)7217243361956806656L, 		// num: 2 total: 23
				(int)1.43024653E9, 		// num: 1 total: 24
				(long)-5020189348399060992L, 		// num: 2 total: 26
				(float)3.4641264E7f, 		// num: 1 total: 27
				(long)8615685391862054912L, 		// num: 2 total: 29
				(double)9.151796E7, 		// num: 2 total: 31
				(int)1.63405402E9, 		// num: 1 total: 32
				(short)30667.996, 		// num: 1 total: 33
				(byte)46.0659, 		// num: 1 total: 34
				(int)9.2596992E8, 		// num: 1 total: 35
				(double)7.9601752E7, 		// num: 2 total: 37
				(double)6.0080672E7, 		// num: 2 total: 39
				(long)5915669612491364352L, 		// num: 2 total: 41
				(byte)70.98394, 		// num: 1 total: 42
				(char)59562.414, 		// num: 1 total: 43
				(long)2980319651624886272L, 		// num: 2 total: 45
				(long)665152291980931072L, 		// num: 2 total: 47
				(int)-2.03555814E9, 		// num: 1 total: 48
				(long)-8401224491453986816L, 		// num: 2 total: 50
				(float)5.8896824E7f, 		// num: 1 total: 51
				(long)3374656510558181376L, 		// num: 2 total: 53
				(double)6.2582536E7, 		// num: 2 total: 55
				(int)-1.35926554E9, 		// num: 1 total: 56
				(short)-2105.1165, 		// num: 1 total: 57
				(byte)27.437984, 		// num: 1 total: 58
				(int)7.0141069E8, 		// num: 1 total: 59
				(double)8.0329976E7, 		// num: 2 total: 61
				(double)6.2795276E7, 		// num: 2 total: 63
				(long)-5388525404138135552L, 		// num: 2 total: 65
				(byte)97.925674, 		// num: 1 total: 66
				(char)45218.766, 		// num: 1 total: 67
				(long)-8993171974615707648L, 		// num: 2 total: 69
				(long)4032542554917154816L, 		// num: 2 total: 71
				(int)-1.55290995E9, 		// num: 1 total: 72
				(long)-715813854417897472L, 		// num: 2 total: 74
				(float)1.2989713E7f, 		// num: 1 total: 75
				(long)-8586904235718418432L, 		// num: 2 total: 77
				(double)8945824.0, 		// num: 2 total: 79
				(int)1.83456627E9, 		// num: 1 total: 80
				(short)12968.975, 		// num: 1 total: 81
				(byte)-122.92477, 		// num: 1 total: 82
				(int)7.7270784E8, 		// num: 1 total: 83
				(double)3.3156372E7, 		// num: 2 total: 85
				(double)6.6951748E7, 		// num: 2 total: 87
				(long)3124492593914419200L, 		// num: 2 total: 89
				(byte)-48.155846, 		// num: 1 total: 90
				(char)30147.95, 		// num: 1 total: 91
				(long)-5705794949374410752L, 		// num: 2 total: 93
				(long)4906390903829786624L, 		// num: 2 total: 95
				(int)-1.35060237E9, 		// num: 1 total: 96
				(long)1497651512901687296L, 		// num: 2 total: 98
				(float)1.8057464E7f, 		// num: 1 total: 99
				(long)-7982580409056735232L, 		// num: 2 total: 101
				(double)3.1270692E7, 		// num: 2 total: 103
				(int)-1.30979392E9, 		// num: 1 total: 104
				(short)9738.722, 		// num: 1 total: 105
				(byte)113.48444, 		// num: 1 total: 106
				(int)1.04707821E9, 		// num: 1 total: 107
				(double)6.024152E7, 		// num: 2 total: 109
				(double)5.6646336E7, 		// num: 2 total: 111
				(long)6471443577216692224L, 		// num: 2 total: 113
				(byte)-82.59888, 		// num: 1 total: 114
				(char)24979.805, 		// num: 1 total: 115
				(long)-7408455901547454464L, 		// num: 2 total: 117
				(long)7126289057206708224L, 		// num: 2 total: 119
				(int)8.4698758E8, 		// num: 1 total: 120
				(long)8431654305328836608L, 		// num: 2 total: 122
				(float)2.6008108E7f, 		// num: 1 total: 123
				(long)-2564050472775606272L, 		// num: 2 total: 125
				(double)6.1027544E7, 		// num: 2 total: 127
				(int)3.92163328E8, 		// num: 1 total: 128
				(short)8125.5303, 		// num: 1 total: 129
				(byte)-100.95522, 		// num: 1 total: 130
				(int)-1.26804584E8, 		// num: 1 total: 131
				(double)2.7117782E7, 		// num: 2 total: 133
				(double)5.1813268E7, 		// num: 2 total: 135
				(long)8845165412070383616L, 		// num: 2 total: 137
				(byte)-84.91118, 		// num: 1 total: 138
				(char)12289.232, 		// num: 1 total: 139
				(long)-4136493182664636416L, 		// num: 2 total: 141
				(long)-8684539089388810240L, 		// num: 2 total: 143
				(int)5.8385504E8, 		// num: 1 total: 144
				(long)3858631105457483776L, 		// num: 2 total: 146
				(float)4.8502756E7f, 		// num: 1 total: 147
				(long)-1576107826588055552L, 		// num: 2 total: 149
				(double)8666295.0, 		// num: 2 total: 151
				(int)-2.04377997E9, 		// num: 1 total: 152
				(short)23615.137, 		// num: 1 total: 153
				(byte)-42.674793, 		// num: 1 total: 154
				(int)1.88571763E9, 		// num: 1 total: 155
				(double)9.0690432E7, 		// num: 2 total: 157
				(double)3.1152562E7, 		// num: 2 total: 159
				(long)7226090936388399104L, 		// num: 2 total: 161
				(byte)-0.9020691, 		// num: 1 total: 162
				(char)25733.008, 		// num: 1 total: 163
				(long)-8820063058680072192L, 		// num: 2 total: 165
				(long)-8647554445748639744L, 		// num: 2 total: 167
				(int)-1.04553037E9, 		// num: 1 total: 168
				(long)-149729089984290816L, 		// num: 2 total: 170
				(float)8.6328736E7f, 		// num: 1 total: 171
				(long)-1092376797389668352L, 		// num: 2 total: 173
				(double)1.8428998E7, 		// num: 2 total: 175
				(int)-1.6221176E8, 		// num: 1 total: 176
				(short)-14803.136, 		// num: 1 total: 177
				(byte)16.866615, 		// num: 1 total: 178
				(int)9.0209581E8, 		// num: 1 total: 179
				(double)5.700434E7, 		// num: 2 total: 181
				(double)8.044856E7, 		// num: 2 total: 183
				(long)4064947388223490048L, 		// num: 2 total: 185
				(byte)-84.131, 		// num: 1 total: 186
				(char)47173.324, 		// num: 1 total: 187
				(long)7275523320706107392L, 		// num: 2 total: 189
				(long)858920513111713792L, 		// num: 2 total: 191
				(int)-3.9816672E8, 		// num: 1 total: 192
				(long)5676218303433138176L, 		// num: 2 total: 194
				(float)9163403.0f, 		// num: 1 total: 195
				(long)-7901843307575439360L, 		// num: 2 total: 197
				(double)7.9803088E7, 		// num: 2 total: 199
				(int)-1.44413222E9, 		// num: 1 total: 200
				(short)22193.244, 		// num: 1 total: 201
				(byte)88.92891, 		// num: 1 total: 202
				(int)1.48687088E8, 		// num: 1 total: 203
				(double)9.8776368E7, 		// num: 2 total: 205
				(double)1.6278985E7, 		// num: 2 total: 207
				(long)4473073241770209280L, 		// num: 2 total: 209
				(byte)-109.172424, 		// num: 1 total: 210
				(char)16695.662, 		// num: 1 total: 211
				(long)-7591762704550205440L, 		// num: 2 total: 213
				(long)-4371812780410517504L, 		// num: 2 total: 215
				(int)1.16891238E9, 		// num: 1 total: 216
				(long)-6107467503477600256L, 		// num: 2 total: 218
				(float)3.5728552E7f, 		// num: 1 total: 219
				(long)-489103829752930304L, 		// num: 2 total: 221
				(double)4.5615268E7, 		// num: 2 total: 223
				(int)1.62407117E9, 		// num: 1 total: 224
				(short)-26882.488, 		// num: 1 total: 225
				(byte)96.702866, 		// num: 1 total: 226
				(int)-1.0507863E9, 		// num: 1 total: 227
				(double)8.2693296E7, 		// num: 2 total: 229
				(double)3.5114712E7, 		// num: 2 total: 231
				(long)7143725174889668608L, 		// num: 2 total: 233
				(byte)-34.197193, 		// num: 1 total: 234
				(char)53823.4, 		// num: 1 total: 235
				(long)-5644737223091402752L, 		// num: 2 total: 237
				(long)7430726967597205504L, 		// num: 2 total: 239
				(int)2.3583056E8, 		// num: 1 total: 240
				(long)1970478522488750080L, 		// num: 2 total: 242
				(float)6.4335468E7f, 		// num: 1 total: 243
				(long)-2352404489188362240L, 		// num: 2 total: 245
				(double)6765977.5, 		// num: 2 total: 247
				(int)8.71842E7, 		// num: 1 total: 248
				(short)-15693.532, 		// num: 1 total: 249
				(byte)23.486866, 		// num: 1 total: 250
				(int)-4.37342048E8, 		// num: 1 total: 251
				(double)5.6475464E7)); 		// num: 2 total: 253

	}

}

