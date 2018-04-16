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
package jit.test.jitt.assembler;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongJavaShifts extends jit.test.jitt.Test {
	private static Logger logger = Logger.getLogger(LongJavaShifts.class);
	private long tstLongArithmetic(long P1,long P32767,long P65536,long PM32768
	,long P2147483647,long PM2147483648,long P0,long P127,long PM128,long PM1
	,long P256,long P2,long P4,long P8,long P16,long P4294967295
	,long P9223372036854775807,long PM9223372036854775808){
		long ans;
		ans = P1 << 1;
		if (ans != 2l) {
			Assert.fail("shift1 bad");
		}
		ans = P1 << P1;
		if (ans != 2l) {
			Assert.fail("shift2 bad");
		}
		ans = ans >> 1;
		if (ans != 1l) {
			Assert.fail("shift3 bad");
		}
		ans = P2 >> P1;
		if (ans != 1l) {
			Assert.fail("shift4 bad");
		}
		ans = P4294967295 << P1;
		if (ans != 4294967295l<<1) {
			Assert.fail("shift5 bad");
		}
		ans = ans >> 1;
		if (ans != 4294967295l) {
			Assert.fail("shift5a bad");
		}
		ans = P4294967295 << P1;
		if (ans != 4294967295l * 2l) {
			Assert.fail("shift6 bad");
		}
		ans = PM9223372036854775808 << P1;
		if (ans != (-9223372036854775808l) << 1) {
			Assert.fail("shift7 bad");
		}
		ans = PM9223372036854775808 >> P1;
		if (ans != -9223372036854775808l >> 1) {
			Assert.fail("shift8 bad");
		}
		ans = PM9223372036854775808 >> P1;
		if (ans != -9223372036854775808l >> 1) {
			Assert.fail("shift9 bad");
		}
		ans = P4294967295 << P1;
		if (ans != 4294967295l * 2l) {
			Assert.fail("shift10 bad");
		}
		ans = P4294967295 << P1;
		ans = ans / 2l;
		if (ans != 4294967295l) {
			Assert.fail("shift11 bad");
		}
		ans = PM9223372036854775808 << P1;
		if ((ans != -9223372036854775808l << 1) || (ans > 0)) {
			Assert.fail("shift12 bad");
		}
		ans = PM9223372036854775808 >> P1;
		if ((ans != (-9223372036854775808l >> 1)) || (ans > 0)) {
			logger.error(ans);
			Assert.fail("shift13 bad");
		}

		return P9223372036854775807 - 50l;	
	}

	@Test
	public void testLongJavaShifts() {
		long result = 0;  
		for (int i = 0; i <= sJitThreshold+2; i++){
			try {//result = this.tstLongArithmetic(1l,0l);
				result = tstLongArithmetic(1l,32767l,65536l,-32768l,2147483647l,-2147483648l,0l,127l,-128l,-1l,256l,2l,4l,8l,16l,4294967295l,9223372036854775807l,-9223372036854775808l);
			}// end try 
			catch (Exception e) {
				logger.error(e);
				Assert.fail("Test.tstLongShifts Failed Early");
			};//end catch
			if (result != 	9223372036854775807l - 50)	{
				//logger.error(ans);		    
				Assert.fail("Test.tstLongShifts Failed Bad Return");
			}//end if	
		}//end for	  
	}//end run  
}//end class
