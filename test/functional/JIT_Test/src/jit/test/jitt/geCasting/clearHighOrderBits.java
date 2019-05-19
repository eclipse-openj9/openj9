/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
// Test file: clearHighOrderBits.java

package jit.test.jitt.geCasting;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class clearHighOrderBits extends jit.test.jitt.Test {

   @Test
   public void testclearHighOrderBits() {
 
     //**************************************************************
     //*
     //*  Type Casting - no sign extended.
     //*
     //**************************************************************
     char resc;
     int resi;
     long resl;
     byte resb;

     // i2c - int to character - no truncation
     resc = i2c( 0x7d );
     if( resc != 0x7d )
        Assert.fail("clearHighOrder->run: Incorrect result for i2c-0x7d!");

     // i2c - int to character truncated
     resc = i2c( 0xff4e00 );
     if( resc != '\u4e00' )
        Assert.fail("clearHighOrder->run: Incorrect result for i2c-0xff4e00!");

     // i2c - int to character truncated
     resc = i2c( 0xffff4e00 );
     if( resc != '\u4e00' )
        Assert.fail("clearHighOrder->run: Incorrect result for i2c-0xffff4e00!");

     // bu2i- byte to integer
     resi = bu2i( 0x50 );
     if( resi != 0x50 )
        Assert.fail("clearHighOrder->run: Incorrect result for bu2i-0x50!");

     // bu2i- byte to integer
     resi = bu2i( 0x2250 );
     if( resi != 0x50 )
        Assert.fail("clearHighOrder->run: Incorrect result for bu2i-0x2250!");

     // bu2i- byte to integer
     resi = bu2i( 0x66778899 );
     if( resi != 0x99 )
        Assert.fail("clearHighOrder->run: Incorrect result for bu2i-0x66778899!");


     // su2i - short to integer - zero extension
     resi = su2i(0x10);
     if( resi != 0x10 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2i-0x10!");

     // su2i - short to integer - zero extension
     resi = su2i(0x9910);
     if( resi != 0x9910 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2i-0x9910!");

     // su2i - short to integer - zero extension
     resi = su2i(0xff9910);
     if( resi != 0x9910 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2i-0xff9910!");


     // su2l- short to long
     resl = su2l( 0x99);
     if( resl != 0x99 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2l-0x99!");

     // su2l- short to long
     resl = su2l( 0xff99);
     if( resl != 0xff99 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2l-0xff99!");

     // su2l- short to long
     resl = su2l( 0xff778899);
     if( resl != 0x8899 )
        Assert.fail("clearHighOrder->run: Incorrect result for su2l-0xff778899!");

     // su2l- short to long - max short
     resl = su2l( 0x7FFFFFFF );
     if( resl != 0xFFFF )
        Assert.fail("clearHighOrder->run: Incorrect result for su2l-0x7FFFFFFF!");

     // bu2s- byte to short - zero extension - NILF
     // NOTE: can't get optimizer to generate this today.

     // bu2l- byte to long - LGBR

  }
  
  // Test i2bEvaluator - case x2c NILH
  static char i2c( int i )
  {
    char c = (char)i;
    return c;
  }

  // bu2i - byte to integer zero extension - NILF
  static int bu2i( int i )
  {
    byte b = (byte)i;
    return (int)b & 0xFF;
  }

  // su2i - short integer to integer - zero extension NILH
  static int su2i( int x )
  {
    // cast to short and then back to int to get it go into su2iEvaluator.
    short s = (short) x;
    return (int)s & 0xFFFF;
  }

  // su2l - short to long zero extension LLGHR
  static long su2l( int i )
  {
    short s = (short)i;
    return (long)s & 0xFFFF;
  }

}
