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
// Test FloatDoubleArgTest1.java
// Testing the combination of passing float and double type arguments and testing the return
// values of float and double types 
// For example: argcall1(float, float, float, double), argcall2(float, double, float, double), and argcall3(double, float, float, double)

package jit.test.jitt.os390.linkage;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class FloatDoubleArgTest1 extends jit.test.jitt.Test {


	private static float tstFoo_1(float arg1, float arg2, float arg3, double arg4) {

		if (arg1 != 1F) {
			Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 1 passing");
		}

                if (arg2 != 3F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 2 passing");
                }

	        if (arg3 != 5F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 3 passing");
                }

                if (arg4 != 7D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 4  passing");
                }

		return arg1;

}


        private static double tstFoo_2(float arg1, double arg2, float arg3, double arg4) {

                if (arg1 != 1F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 1 passing");
                }

                if (arg2 != 3D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 2 passing");
                }

                if (arg3 != 5F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 3 passing");
                }

                if (arg4 != 7D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 4  passing");
                }

                return arg4;

}


        private static double tstFoo_3(double arg1, float arg2, float arg3, double arg4) {

                if (arg1 != 1D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 1 passing");
                }

                if (arg2 != 3F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 2 passing");
                }

                if (arg3 != 5F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 3 passing");
                }

                if (arg4 != 7D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 4  passing");
                }

                return arg1;

}



        private static double tstFoo_4(float arg1, double arg2, double arg3, float arg4) {

                if (arg1 != 1F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 1 passing");
                }

                if (arg2 != 3D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 2 passing");
                }

                if (arg3 != 5D) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect float argument 3 passing");
                }

                if (arg4 != 7F) {
                        Assert.fail("FloatDoubleArgTest1->tstFoo: Incorrect double argument 4  passing");
                }

                return arg3;

}

	@Test
	public void testFloatDoubleArgTest1() {
		float float_result=0F;
                double double_result=0D;

		for (int j = 0; j < sJitThreshold; j++) {
			float_result=0F;
			float_result=tstFoo_1(1F,3F,5F,7D);
			if (float_result != 1F)
        	                Assert.fail("FloatDoubleArgTest1->run: Incorrect float return value from tstFoo_1!");


			double_result=0D;
			double_result=tstFoo_2(1F,3D,5F,7D);

                        if (double_result != 7D)
                                Assert.fail("FloatDoubleArgTest1->run: Incorrect double return value from tstFoo_2!");
			

			double_result=0D;
			double_result=tstFoo_3(1D,3F,5F,7D);
		
			if (double_result != 1D)
                                Assert.fail("FloatDoubleArgTest1->run: Incorrect double return value from tstFoo_3!");

                        double_result=0D;
                        double_result=tstFoo_4(1F,3D,5D,7F);

                        if (double_result != 5D)
                                Assert.fail("FloatDoubleArgTest1->run: Incorrect double return value from tstFoo_3!");


		}
	}

}
