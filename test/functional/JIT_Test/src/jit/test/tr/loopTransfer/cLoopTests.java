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

package jit.test.tr.loopTransfer;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class cLoopTests {

   private static Logger logger = Logger.getLogger(cLoopTests.class);
   //ctor
   public cLoopTests(String s) {
      for (int i = 0; i < byteArrTable.length; i++)
         byteArrTable[i] = (byte)i;
      oArr[0] = new O1();
      oArr[1] = new O2();
   }

@Test
public void test001() {
      boolean b = true;
      int count;
      // warmup
      // setup a caller to get profiling info
      //  
      O o = new O1();
      for (count = 0; count < 15000; count++)
         subTest001(true, o);
      // actual run
      // 
      o = new O2();
      for (count = 0; count < 250; count++)
         subTest001(false, o);
      
      logger.debug("Running test001 ... ");
      AssertJUnit.assertTrue(b);
   }

   private void subTest001(boolean bool, O o) {
      // single loop
      //
      O o1;
      for (int i = 0; i < byteArrA.length; i++)
         byteArrA[i] = 33;
      for (int i = 0; i < byteArrA.length; i++) { 
         byte b = byteArrTable[byteArrA[i] & 0xff];
         // make o1 non-invariant so the 
         // guard is not hoisted out of the loop
         // 
         // this bool is for the warmup run so that 
         // the profiler thinks o1 is of type O1
         //  
         if (bool)
           o1 = o;
         else
           o1 = oArr[i%2];
         // loop transfer here
         // call is guarded 
         // 
         char outputByte = o1.getOutChar();
         if (b == outputByte) { 
            byteArrB[i] = b;
         } else {
            byteArrB[i] = (byte)outputByte;
         }
      }
   }
   
@Test
public void test002() { 
      boolean b = true;
      int count = 0;
      
      O o = new O1();
      for (; count < 750; count++)
         subTest002(true, o);
      
      o = new O2();
      for (count = 0; count < 20; count++)
         subTest002(false, o);
      
      logger.debug("Running test002 ... ");
      AssertJUnit.assertTrue(b);
   }

   private void subTest002(boolean bool, O o) { 
      // double nested loop
      //
      O o1;
      int i = 0;
      for (; i < byteArrA.length/2; i++)
         byteArrA[i] = 33;
      for (i = 0; i < byteArrA.length/2; i++) {
         byte b = byteArrTable[byteArrA[i] & 0xff];
         if (bool)
            o1 = o;
         else
            o1 = oArr[i%2];
         char outputByte = o1.getOutChar();
         for (int j = 0; j < byteArrB.length/2; j++) {
            if (bool)
               o1 = oArr[0];
            else
               o1 = oArr[1];
            char inputByte = o1.getInChar();
            byteArrB[j] = (byte)(inputByte + outputByte);
         } 
      }
   }

   // fields
   public static byte[] byteArrA = new byte[8097];
   public static byte[] byteArrB = new byte[8097];
   public static byte[] byteArrTable = new byte[256];
   public static O[] oArr = new O[2];
} 

// some oddball class hierarchy
//
//            O
//            |  \ 
//            O1  O2
//            
abstract class O {
   public O() { 
      _c = 22;
   }
   public abstract char getOutChar();
   public abstract char getInChar();
   public abstract char getInOutChar();
   public char _c;
}

class O1 extends O {
   public O1() {
      _c = 22+11;
   }
   public char getOutChar() {
      char c = 33;
      return (char) (c + _c);
   }
   public char getInChar() { 
      char c = 44;
      return (char) (c - _c);
   }
   public char getInOutChar() { 
      char c = 11; 
      return (char) (3*c - _c);
   }
}

class O2 extends O {
   public O2() {
      _c = 22+11;
   }
   public char getOutChar() { 
      char c = 44;
      return (char) (c - _c);
   }
   public char getInChar() { 
      char c = 33;
      return (char) (c + _c);
   }
   public char getInOutChar() {
      char c = 22;
      return (char) (2*c + _c);
   } 
}
