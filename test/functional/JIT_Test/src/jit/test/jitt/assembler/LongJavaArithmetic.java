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
public class LongJavaArithmetic extends jit.test.jitt.Test {
	private static Logger logger = Logger.getLogger(LongJavaArithmetic.class);
	private long tstLongArithmetic(long P1,long P32767,long P65536,long PM32768
	,long P2147483647,long PM2147483648,long P0,long P127,long PM128,long PM1
	,long P256,long P2,long P4,long P8,long P16,long P4294967295
	,long P9223372036854775807,long PM9223372036854775808) {
		long ans;
		ans = P1 + P0;
		if (ans != 1l) {
			Assert.fail("add1 bad");
		}
		ans = P4294967295 + P1;
		if (ans != 4294967296l) {
			logger.error(ans);
			Assert.fail("add2 bad");
		}	//end if
		//logger.debug(ans);
		ans = ans - 1l;
		if (ans != 4294967295l) {
			logger.error(ans);
			Assert.fail("add3 bad");
		}	//end if
		//logger.debug(ans);	       
//    ans = ans * P2;
//    if ( (ans != 4294967295l * 2l)){// || (ans != 4294967295l * P2) ) {
//        logger.error(ans);
//       	Assert.fail("mul11 bad");
//	}	//end if
//	ans = P9223372036854775807 + 1l;
//    if (ans != PM9223372036854775808) {
//        logger.error(ans);
//      	Assert.fail("add4 bad");
//	}	//end if
		return P9223372036854775807 - 50l;	
	}

	@Test
	public void testLongJavaArithmetic() {
		long result = 0;  
		for (int i = 0; i <= sJitThreshold+1; i++){
			try {//result = this.tstLongArithmetic(1l,0l);
				result = tstLongArithmetic(1l,32767l,65536l,-32768l,2147483647l,-2147483648l,0l,127l,-128l,-1l,256l,2l,4l,8l,16l,4294967295l,9223372036854775807l,-9223372036854775808l);
			}// end try 
			catch (Exception e) {
				logger.error(e);
				Assert.fail("Test.tstLongArithmetic Failed Early");
			};//end catch
			if (result != 	9223372036854775807l - 50)	{
				//logger.error(ans);
				Assert.fail("Test.tstLongArithmetic Failed Bad Return");
			}//end if	
		}//end for	  
	}//end run  
}
