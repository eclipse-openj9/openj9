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
// Test file: LongToFloat.java

/* tests for L2F - Long to Float

-widening primitive conversion
-may result in loss of precision.  IE. may lose some of the LSB of long value to float.  
-uses IEEE 754 round-to-nearest mode.

*/

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongToFloat extends jit.test.jitt.Test {

   @Test
   public void testLongToFloat() {
        int result=0;
        result=tstL2F();

        if (result != 1)
                Assert.fail("LongToFloat->run(): The test did not execute as expected!");

   }

        static int tstL2F() {

		//CASE A:	No lost of precision for long that are not of 20 digits long
		long proper_value_a;
		float widening_conversion_value_a;
		long no_lost;
		
		proper_value_a = 234567L;
		
		widening_conversion_value_a = proper_value_a;
		no_lost = proper_value_a - (long)widening_conversion_value_a;
		
		if (no_lost != 0)
                        Assert.fail("LongToFloat: test case A failed!");
	
		//CASE B: 	Lost of precision will occur since type float will not be precise to 
		//		20 digits
		long proper_value_b;
		float widening_conversion_value_b;
		long lost_precision_b;
	
		proper_value_b = 2345678901234567890L;
		widening_conversion_value_b = proper_value_b; //here converting a long to a float
		lost_precision_b = proper_value_b - (long)widening_conversion_value_b;
	
		if ( lost_precision_b == 0 )
                        Assert.fail("LongToFloat: test case B failed!");

			//there will be a loss of precision because the long value was set to 20 digits
		
		return 1;
	}
	
	
}
