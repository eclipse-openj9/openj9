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
package jit.test.tr.BigDecimal;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.math.BigDecimal;
import java.math.BigInteger;

@Test(groups = { "level.sanity","component.jit" })
public class BDCompareTest {
   @Test
   public void testCompare1() {
      // Make three BigDecimals. Each has the value 9223372036854.775807.
      BigDecimal bd1 = BigDecimal.valueOf(Long.MAX_VALUE, 6);
      BigDecimal bd2 = new BigDecimal("9223372036854.775807");
      BigDecimal bd3 = new BigDecimal(BigInteger.valueOf(Long.MAX_VALUE), 6);

      AssertJUnit.assertEquals("testCompare1: bd1.toString() & bd2.toString()", bd1.toString(), bd2.toString());
      AssertJUnit.assertEquals("testCompare1: bd2.toString() & bd3.toString()", bd2.toString(), bd3.toString());
      AssertJUnit.assertEquals("testCompare1: bd3.toString() & bd1.toString()", bd3.toString(), bd1.toString());

      // Compare them with -999999999.999989.
      BigDecimal val1 = new BigDecimal("-999999999.999989");
      AssertJUnit.assertTrue("testCompare1: val1.compareTo(bd1) [val:" + val1 + "][bd:" + bd1 + "]", val1.compareTo(bd1) < 0);
      AssertJUnit.assertTrue("testCompare1: val1.compareTo(bd2) [val:" + val1 + "][bd:" + bd2 + "]", val1.compareTo(bd2) < 0);
      AssertJUnit.assertTrue("testCompare1: val1.compareTo(bd3) [val:" + val1 + "][bd:" + bd3 + "]", val1.compareTo(bd3) < 0);

      // Now compare them with -999999999.99998 (same value as before,
      // except the last 9 has been removed).
      BigDecimal val2 = new BigDecimal("-999999999.99998");
      AssertJUnit.assertTrue("testCompare1: val2.compareTo(bd1) [val:" + val2 + "][bd:" + bd1 + "]", val2.compareTo(bd1) < 0);
      AssertJUnit.assertTrue("testCompare1: val2.compareTo(bd2) [val:" + val2 + "][bd:" + bd1 + "]", val2.compareTo(bd2) < 0);
      AssertJUnit.assertTrue("testCompare1: val2.compareTo(bd3) [val:" + val2 + "][bd:" + bd1 + "]", val2.compareTo(bd3) < 0);
   }

   @Test
   public void testCompare2() {
      BigDecimal a1 = new BigDecimal( 61.55 );
      BigDecimal a2 = new BigDecimal("61.55");
      BigDecimal b1 = new BigDecimal( 2.554523 );
      BigDecimal b2 = new BigDecimal("2.554523");
      BigDecimal m1 = b1.multiply(new BigDecimal("12")); // a1 x b1
      BigDecimal m2 = b2.multiply(new BigDecimal("12")); // a2 x b2
      
      AssertJUnit.assertEquals("testCompare2: a1.compareTo(b1)", a1.compareTo(b1), 1);
      AssertJUnit.assertEquals("testCompare2: a1.compareTo(b2)", a1.compareTo(b2), 1);
      AssertJUnit.assertEquals("testCompare2: a1.compareTo(m1)", a1.compareTo(m1), 1);
      AssertJUnit.assertEquals("testCompare2: a1.compareTo(m2)", a1.compareTo(m2), 1);
      
      AssertJUnit.assertEquals("testCompare2: a2.compareTo(b1)", a2.compareTo(b1), 1);
      AssertJUnit.assertEquals("testCompare2: a2.compareTo(b2)", a2.compareTo(b2), 1);
      AssertJUnit.assertEquals("testCompare2: a2.compareTo(m1)", a2.compareTo(m1), 1);
      AssertJUnit.assertEquals("testCompare2: a2.compareTo(m2)", a2.compareTo(m2), 1);
   }
}
