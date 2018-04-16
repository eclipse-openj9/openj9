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
import java.lang.NumberFormatException;

@Test(groups = { "level.sanity","component.jit" })
public class BDEncodingTest {
   @Test
   public void testEncoding1() {
       char [] doubleWidthChars = new char[] {
             '\uff10', '\uff11', '\uff12', '\uff13', '\uff14', 
             '\uff15', '\uff16', '\uff17', '\uff18', '\uff19' };
       char [] singleWidthChars = new char[] {
             '\u0030', '\u0031', '\u0032', '\u0033', '\u0034', 
             '\u0035', '\u0036', '\u0037', '\u0038', '\u0039' };
       
       for(int i = 0; i < doubleWidthChars.length; ++i) {
          BigDecimal doubleWidthBD = new BigDecimal(new char[] {doubleWidthChars[i]});
          BigDecimal singleWidthBD = new BigDecimal(new char[] {singleWidthChars[i]});
          AssertJUnit.assertEquals("testEncoding1: [i:" + i + "]", singleWidthBD, doubleWidthBD);
       }
   }
   
   @Test
   public void testEncoding2() {
      char [] doubleWidthChars = new char[] {
            '\uff0b', '\uff11', '\uff12', '\uff0e', '\uff13', 
            '\uff14', '\uff15', '\uff25', '\uff0d', '\uff16', 
            '\uff17', '\uff18', '\uff19', '\uff10', '\uff10'};
      char [] singleWidthChars = new char[] {
            '+', '1', '2', '.', '3',
            '4', '5', 'e', '-', '6',
            '7', '8', '9', '0', '0'};
      boolean caught = false;
      try { 
          BigDecimal doubleWidthBD = new BigDecimal(doubleWidthChars);
          BigDecimal singleWidthBD = new BigDecimal(singleWidthChars);
      } catch (NumberFormatException e) {
         caught = true;
      }
       AssertJUnit.assertTrue("testEncoding2", caught);
   }

   @Test
   public void testEncoding3() {
      char [] doubleWidthChars = new char[] {
            '\u002b', '\uff11', '\uff12', '\u002e', '\uff13',
            '\uff14', '\uff15', '\u0065', '\u002d', '\uff16',
            '\uff17', '\uff18', '\uff19', '\uff10', '\uff10'};
      char [] singleWidthChars = new char[] {
            '+', '1', '2', '.', '3',
            '4', '5', 'e', '-', '6',
            '7', '8', '9', '0', '0'};
      BigDecimal doubleWidthBD = new BigDecimal(doubleWidthChars);
      BigDecimal singleWidthBD = new BigDecimal(singleWidthChars);
      AssertJUnit.assertEquals("testEncoding3", doubleWidthBD, singleWidthBD);
   }
}
