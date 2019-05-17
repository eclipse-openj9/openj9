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
// Test file: signExtendedHighOrderBits.java

package jit.test.jitt.geCasting;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class signExtendedHighOrderBits extends jit.test.jitt.Test {

   @Test
   public void testsignExtendedHighOrderBits() {
 
     //**************************************************************
     //*
     //*  Type casting - sign extended
     //*
     //**************************************************************
     short ress;
     int resi;
     long resl;
     byte resb;

     // int i2s - int to short int - LHR
     ress = i2s( 39000 );
     if( ress != -26536 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2s-39000!");

     ress = i2s( 0x1030 );
     if( ress != 0x1030 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2s-0x1030!");

     // i2s - int to short int
     ress = i2s( 0x102030 );
     if( ress != 0x2030 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2s-0x102030!");

     // i2s - max int to short int
     ress = i2s( 0x7FFF );
     if( ress != 32767 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2s-0x7FFF!");

     // i2s - int to short int
     ress = i2s( 0xffffffff );
     if( ress != -1 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2s-0xffffffff!");

    // i2b - int to byte - negative value - LBR
     resb = i2b( 0x88 );
     if( resb != -120 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2b-0x88!");

     // i2b - int to byte -neg
     resb = i2b( 0xFF88 );
     if( resb != -120 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2b-0xFF88!");

     // i2b - int to byte
     resb = i2b( 0x55 );
     if( resb != 0x55 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2b-0x55!");

     // i2b - min int to byte
     resb = i2b( -32768 );
     if( resb != 0 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2b--32768!");

     // i2b - max int to byte
     resb = i2b( 0x7FFF );
     if( resb != -1 )
        Assert.fail("clearHighOrder->run: Incorrect result for i2b-0x7FFF!");

     // b2i - byte to integer - LBR
     resi = b2i();
     if( resi != 0x50 )
        Assert.fail("clearHighOrder->run: Incorrect result for b2i!");

     // b2l - byte to long - LGBR
     resl = b2l();

     if( resl != 0x50 )
        Assert.fail("clearHighOrder->run: Incorrect result for b2l!");

     // todo: can't get this to go into s2lEvaluator
     // always  s2i first


  }
 
  static short i2s( int i )
  {
    short s = (short)i;
    return s;
  }
 
  // i2b - integer to byte
  static byte i2b( int i )
  {
    byte b = (byte)i;
    return b;
  }

  // b2i evaluator, but not into if case for GE instr because parent is ibload
  //   Vijay: The only byte datatype nodes i know we will create are ibload and x2b where
  //          x can be i,l,s,c
  int b2i()
  {
    int i = 0;
    byte[] bb = new byte[5];
    bb[0] = 0x10;
    bb[1] = 0x20;
    bb[2] = 0x30;
    bb[3] = 0x40;
    bb[4] = 0x50;
    for( int x=0; x < bb.length; x++)
    {
       i = (int)bb[x];
    }
    return i;
  }

  // b2lEvaluator - LGBR
  long  b2l()
  {
    long l = 0;
    byte[] bb = new byte[5];
    bb[0] = 0x10;
    bb[1] = 0x20;
    bb[2] = 0x30;
    bb[3] = 0x40;
    bb[4] = 0x50;
    for( int x=0; x < bb.length; x++)
    {
      l = (long)bb[x];
    }
    return l;
  }



}
