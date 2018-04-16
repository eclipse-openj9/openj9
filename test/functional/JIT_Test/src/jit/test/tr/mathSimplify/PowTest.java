/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package jit.test.tr.mathSimplify;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.Math;

@Test(groups = { "level.sanity","component.jit" })
public class PowTest {

  static double DELTA=1.0E-5;

  static double deltaBig[] = {
	0.0,
	0.0,
	1.0E-16,
	1.0E-16,
	1.0E-13,
	1.0E-11,
	1.0E-11,
	1.0E-11,
	1.0E-11,
	1.0E-10,
	1.0E-10,
	1.0E-9,
	1.0E-8,
	1.0E-7,
	1.0E-6,
	1.0E-5,
	1.0E-4,
	1.0E-3,
	1.0E-2,
	1.0E-1,
	1.0,
	1.0E1,
	1.0E2,
	1.0E3,
	1.0E4,
	1.0E5,
	1.0E6,
	1.0E7,
	1.0E8,
	1.0E9,
	1.0E10,
	1.0E11,
	1.0E12,
	1.0E13,
	1.0E14,
	1.0E15
  };
  static double deltaSmall[] = {
	0.0,
	0.0,
	1.0E-16,
	1.0E-16,
	1.0E-13,
	1.0E-11,
	1.0E-11,
	1.0E-11,
	1.0E-11,
	1.0E-10,
	1.0E-10,
	1.0E-9,
	1.0E-8,
	1.0E-7,
	1.0E-6,
	1.0E-5,
	1.0E-4,
	1.0E-3,
	1.0E-2,
	1.0E-1,
	1.0,
	1.0E1,
	1.0E2,
	1.0E3,
	1.0E4,
	1.0E5,
	1.0E6,
	1.0E7,
	1.0E8,
	1.0E9,
	1.0E10,
	1.0E11,
	1.0E12,
	1.0E13,
	1.0E14,
	1.0E15
  };

  static double expected1[] = {
		1.0,
		2.0,
		4.0,
		8.0,
		16.0,
		32.0,
		64.0,
		128.0,
		256.0,
		512.0,
		1024.0,
		2048.0,
		4096.0,
		8192.0,
		16384.0,
		32768.0,
		65536.0,
		131072.0,
		262144.0,
		524288.0,
		1048576.0,
		2097152.0,
		4194304.0,
		8388608.0,
		1.6777216E7,
		3.3554432E7,
		6.7108864E7,
		1.34217728E8,
		2.68435456E8,
		5.36870912E8,
		1.073741824E9,
		2.147483648E9,
		4.294967296E9,
		8.589934592E9,
		1.7179869184E10,
		3.4359738368E10,
		0
  		};
  static double expected2[] = { 
	1.0,
	-2.1,
	4.41,
	-9.261000000000001,
	19.448100000000004,
	-40.84101000000001,
	85.76612100000003,
	-180.10885410000006,
	378.22859361000013,
	-794.2800465810003,
	1667.9880978201008,
	-3502.7750054222115,
	7355.827511386645,
	-15447.237773911955,
	32439.199325215108,
	-68122.31858295173,
	143056.86902419862,
	-300419.42495081713,
	630880.7923967161,
	-1324849.6640331037,
	2782184.2944695177,
	-5842587.018385988,
	1.2269432738610575E7,
	-2.5765808751082208E7,
	5.4108198377272636E7,
	-1.1362721659227255E8,
	2.3861715484377235E8,
	-5.0109602517192197E8,
	1.0523016528610362E9,
	-2.209833471008176E9,
	4.64065028911717E9,
	-9.745365607146057E9,
	2.046526777500672E10,
	-4.2977062327514114E10,
	9.025183088777965E10,
	-1.8952884486433728E11,
	0
         };
  static double expected3[] = {
	1.0,
	0.9,
	0.81,
	0.7290000000000001,
	0.6561000000000001,
	0.5904900000000001,
	0.531441,
	0.4782969000000001,
	0.4304672100000001,
	0.3874204890000001,
	0.3486784401000001,
	0.31381059609000006,
	0.2824295364810001,
	0.2541865828329001,
	0.2287679245496101,
	0.20589113209464907,
	0.18530201888518416,
	0.16677181699666577,
	0.15009463529699918,
	0.13508517176729928,
	0.12157665459056935,
	0.10941898913151242,
	0.09847709021836118,
	0.08862938119652505,
	0.07976644307687256,
	0.0717897987691853,
	0.06461081889226677,
	0.058149737003040096,
	0.05233476330273609,
	0.04710128697246248,
	0.04239115827521624,
	0.038152042447694615,
	0.03433683820292515,
	0.030903154382632636,
	0.027812838944369374,
	0.025031555049932437,
	0
     };
  static double expected4[] = {
	0.015625,
	0.5,
	68.59350160232275,
	1.0000000000693148,
  };
  static double expected5[] = {
	9.999999999999997E23,
	10000.0,
	3.981071705534987E-25,
	0.999999999078966,
	0
   };

  @Test
  public void testBadPow(){
    for(int i =0;i < 999;++i){ //ensure compilation
      runbadtest(2.0,expected4,deltaBig);
      runbadtest(0.0001,expected5,deltaSmall);
    }
  }
  void runbadtest(double value,double[] expected,double[] delta){
    AssertJUnit.assertEquals(Math.pow(value,-6.0),expected[0],delta[0]);
    AssertJUnit.assertEquals(Math.pow(value,-1.0),expected[1],delta[0]);
    AssertJUnit.assertEquals(Math.pow(value,6.1),expected[2],delta[0]);
    AssertJUnit.assertEquals(Math.pow(value,0.0000000001),expected[3],delta[0]);
  }
  @Test
  public void testPow(){
    for(int i = 0; i < 999;++i){
      runtest(2.0,expected1,deltaBig);
      runtest(-2.1,expected2,deltaBig);
      runtest(0.9,expected3,deltaSmall);
    }
  }
  void runtest(double value,double[] expected,double[] delta){
    AssertJUnit.assertEquals(Math.pow(value,0.0),expected[0],delta[0]);
    AssertJUnit.assertEquals(Math.pow(value,1.0),expected[1],delta[1]);
    AssertJUnit.assertEquals(Math.pow(value,2.0),expected[2],delta[2]);
    AssertJUnit.assertEquals(Math.pow(value,3.0),expected[3],delta[3]);
    AssertJUnit.assertEquals(Math.pow(value,4.0),expected[4],delta[4]);
    AssertJUnit.assertEquals(Math.pow(value,5.0),expected[5],delta[5]);
    AssertJUnit.assertEquals(Math.pow(value,6.0),expected[6],delta[6]);
    AssertJUnit.assertEquals(Math.pow(value,7.0),expected[7],delta[7]);
    AssertJUnit.assertEquals(Math.pow(value,8.0),expected[8],delta[8]);
    AssertJUnit.assertEquals(Math.pow(value,9.0),expected[9],delta[9]);
    AssertJUnit.assertEquals(Math.pow(value,10.0),expected[10],delta[10]);
    AssertJUnit.assertEquals(Math.pow(value,11.0),expected[11],delta[11]);
    AssertJUnit.assertEquals(Math.pow(value,12.0),expected[12],delta[12]);
    AssertJUnit.assertEquals(Math.pow(value,13.0),expected[13],delta[13]);
    AssertJUnit.assertEquals(Math.pow(value,14.0),expected[14],delta[14]);
    AssertJUnit.assertEquals(Math.pow(value,15.0),expected[15],delta[15]);
    AssertJUnit.assertEquals(Math.pow(value,16.0),expected[16],delta[16]);
    AssertJUnit.assertEquals(Math.pow(value,17.0),expected[17],delta[17]);
    AssertJUnit.assertEquals(Math.pow(value,18.0),expected[18],delta[18]);
    AssertJUnit.assertEquals(Math.pow(value,19.0),expected[19],delta[19]);
    AssertJUnit.assertEquals(Math.pow(value,20.0),expected[20],delta[20]);
    AssertJUnit.assertEquals(Math.pow(value,21.0),expected[21],delta[21]);
    AssertJUnit.assertEquals(Math.pow(value,22.0),expected[22],delta[22]);
    AssertJUnit.assertEquals(Math.pow(value,23.0),expected[23],delta[23]);
    AssertJUnit.assertEquals(Math.pow(value,24.0),expected[24],delta[24]);
    AssertJUnit.assertEquals(Math.pow(value,25.0),expected[25],delta[25]);
    AssertJUnit.assertEquals(Math.pow(value,26.0),expected[26],delta[26]);
    AssertJUnit.assertEquals(Math.pow(value,27.0),expected[27],delta[27]);
    AssertJUnit.assertEquals(Math.pow(value,28.0),expected[28],delta[28]);
    AssertJUnit.assertEquals(Math.pow(value,29.0),expected[29],delta[29]);
    AssertJUnit.assertEquals(Math.pow(value,30.0),expected[30],delta[30]);
    AssertJUnit.assertEquals(Math.pow(value,31.0),expected[31],delta[31]);
    AssertJUnit.assertEquals(Math.pow(value,32.0),expected[32],delta[32]);
    AssertJUnit.assertEquals(Math.pow(value,33.0),expected[33],delta[33]);
    AssertJUnit.assertEquals(Math.pow(value,34.0),expected[34],delta[34]);
    AssertJUnit.assertEquals(Math.pow(value,35.0),expected[35],delta[35]);

  }
}
