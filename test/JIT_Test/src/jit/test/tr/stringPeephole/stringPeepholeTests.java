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
package jit.test.tr.stringPeephole;

import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })

public class stringPeepholeTests{

      private static Logger logger = Logger.getLogger(stringPeepholeTests.class);
      public static final int numIters = 6000;
      
      public void testFunctionality (){
         // Run tests enough time to force compilation
         for (int i=0; i< numIters; i++) 
            testBigDecimal0ToString();
      }

      public void testBigDecimal0ToString() {
         NumberFormat format = new DecimalFormat("###,###,##0");
         BigDecimal bigDecimalValue = new BigDecimal(new Double(0.000));
         String s = bigDecimalToString(format, bigDecimalValue)
         AssertJUnit.assertEquals(s, "0");
      }

      public String bigDecimalToString(NumberFormat format, BigDecimal bigDecimalValue){
         String s =  format.format(bigDecimalValue.floatValue());
         return s;
      }
   }
