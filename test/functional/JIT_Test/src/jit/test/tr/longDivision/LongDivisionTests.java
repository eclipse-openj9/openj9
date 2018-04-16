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
package jit.test.tr.longDivision;

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class LongDivisionTests {

	private long A,B,ZERO;
	private long[] divisors = new long[41],dividents = new long[41],result = new long[41];
	
	@BeforeMethod
	protected void setUp(){
		A =	 Long.MAX_VALUE;
                B =	 Long.MAX_VALUE/49 + 1;
                ZERO =	 0L;
                
                dividents[0]=	7384309401819754091L;	divisors[0]=	1536397675547406568L;	result[0]=	4L;
		dividents[1]=	2560603750867503097L;	divisors[1]=	5836528183376026101L;	result[1]=	0L;
		dividents[2]=	-2455616426507708959L;	divisors[2]=	-5083262891204977499L;	result[2]=	0L;
		dividents[3]=	7885947710280542703L;	divisors[3]=	-471226283257679002L;	result[3]=	-16L;
		dividents[4]=	-8648709530015712998L;	divisors[4]=	-8615627520672399285L;	result[4]=	1L;
		dividents[5]=	-342525033587836407L;	divisors[5]=	3833733087200377137L;	result[5]=	0L;
		dividents[6]=	-435356538208409640L;	divisors[6]=	-6778139931678084573L;	result[6]=	0L;
		dividents[7]=	-2309669402240766693L;	divisors[7]=	-706403047494620673L;	result[7]=	3L;
		dividents[8]=	-5021724527835333854L;	divisors[8]=	-4961311159828202151L;	result[8]=	1L;
		dividents[9]=	-938631030445469314L;	divisors[9]=	2741112851095135134L;	result[9]=	0L;
		dividents[10]=	-5190022690496103996L;	divisors[10]=	7346439812345649788L;	result[10]=	0L;
		dividents[11]=	-6407736261104419952L;	divisors[11]=	-1295318538235010175L;	result[11]=	4L;
		dividents[12]=	6023706352528002252L;	divisors[12]=	3806604616759703554L;	result[12]=	1L;
		dividents[13]=	-2336799299688807886L;	divisors[13]=	3965700834230081342L;	result[13]=	0L;
		dividents[14]=	-1729792076267568114L;	divisors[14]=	-1833460530131021118L;	result[14]=	0L;
		dividents[15]=	-3523359047654929592L;	divisors[15]=	-936800776312439538L;	result[15]=	3L;
		dividents[16]=	3793768268809710241L;	divisors[16]=	6629139860534926968L;	result[16]=	0L;
		dividents[17]=	8375195861584881724L;	divisors[17]=	-8304662523267328944L;	result[17]=	-1L;
		dividents[18]=	-1573041107709301920L;	divisors[18]=	6447601537203502660L;	result[18]=	0L;
		dividents[19]=	-5083912204655836548L;	divisors[19]=	-3566718470172041722L;	result[19]=	1L;
		dividents[20]=	-2451528478814845590L;	divisors[20]=	-246792063789986517L;	result[20]=	9L;
		dividents[21]=	-7528146448550324232L;	divisors[21]=	-6172323896332751530L;	result[21]=	1L;
		dividents[22]=	1706381906280698019L;	divisors[22]=	6798429387495372437L;	result[22]=	0L;
		dividents[23]=	-2721522098009602139L;	divisors[23]=	-7825943825160615926L;	result[23]=	0L;
		dividents[24]=	-10529301729652175L;	divisors[24]=	1246242108670287565L;	result[24]=	0L;
		dividents[25]=	5821491942405025276L;	divisors[25]=	-9109683537823787247L;	result[25]=	0L;
		dividents[26]=	8446770510559748351L;	divisors[26]=	690873774317132677L;	result[26]=	12L;
		dividents[27]=	4951010104436199803L;	divisors[27]=	5557977112485095164L;	result[27]=	0L;
		dividents[28]=	8247503120997849661L;	divisors[28]=	385393848010718287L;	result[28]=	21L;
		dividents[29]=	778195877541863203L;	divisors[29]=	-3869624809833449343L;	result[29]=	0L;
		dividents[30]=	4328157203288925662L;	divisors[30]=	-5041389744961101020L;	result[30]=	0L;
		dividents[31]=	7169178854159005432L;	divisors[31]=	-1092142680105426692L;	result[31]=	-6L;
		dividents[32]=	-4387961678942549224L;	divisors[32]=	-2074006060263707423L;	result[32]=	2L;
		dividents[33]=	2941012629607155845L;	divisors[33]=	7374189984136100585L;	result[33]=	0L;
		dividents[34]=	482444831761945832L;	divisors[34]=	-3584574423969830459L;	result[34]=	0L;
		dividents[35]=	8763680236672843605L;	divisors[35]=	-768417150999140757L;	result[35]=	-11L;
		dividents[36]=	-4355607472378837365L;	divisors[36]=	2717748215635535349L;	result[36]=	-1L;
		dividents[37]=	3093122281065152379L;	divisors[37]=	-2961857912616101431L;	result[37]=	-1L;
		dividents[38]=	-5906387468006137850L;	divisors[38]=	-8175579569256365575L;	result[38]=	0L;
		dividents[39]=	-3930482635609001461L;	divisors[39]=	3968897460202439147L;	result[39]=	0L;
		dividents[40]=	-4007633151114153312L;	divisors[40]=	2322778656431872828L;	result[40]=	-1L;

	}
	
	@Test
	public void testDivisionRicky() {
		long C = A/B;

		AssertJUnit.assertTrue(C ==	48L);
	}

	@Test
	public void testDivByZero() {
		long C = 0L;
		
		try {
			C = A/ZERO;
		} catch (ArithmeticException e) {
			C = 1L;
		}
		
		AssertJUnit.assertTrue(C == 1L);
		
	}

	@Test
	public void testDivisionRandom() {
		for (int i=0;i<dividents.length; i++) {
			AssertJUnit.assertTrue((dividents[i]/divisors[i]) == result[i]);
		}
	}

}
