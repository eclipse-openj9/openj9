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
// Test file: orImmed.java

package jit.test.jitt.immedOp;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class orImmed extends jit.test.jitt.Test {

   @Test
   public void testorImmed() {
 
     //**************************************************************
     //*
     //*  Integer constants OR immediate
     //*
     //**************************************************************
     int i = 10000;
     int ires = 0;

     // int OR Immediate 32int value
     ires = ior( i ); 
     if( ires != 0x11112732 )
        Assert.fail("orImmed->run: Incorrect result for ior!");
 
     // int OR Immediate 32int value
     ires = iorLH( i ); 
     if( ires != 0x43212710 )
        Assert.fail("orImmed->run: Incorrect result for iorLH!");
 
     // int OR Immediate 32int value
     ires = iorLL( i ); 
     if( ires != 0x7778 )
        Assert.fail("orImmed->run: Incorrect result for iorLL!");
 
     //**************************************************************
     //*
     //*  Long constants OR immediate
     //*
     //**************************************************************
     long li = 210000L;
     long lres = 0;

     // long OR Immediate 32int value - use GE
     lres = lorHigh( li ); 
     if( lres != 0x4000800033450L )
        Assert.fail("orImmed->run: Incorrect result for lorHigh!");
 
     // long OR Immediate 32bit value  - use GE
     lres = lorLow( li );
     if( lres != 0x7fffffff )
        Assert.fail("addImmed->run: Incorrect result for lorLow!");

     // long OR Immediate 32bit value  
     lres = lorLow2( li );
     if( lres != 0x37654 )
        Assert.fail("addImmed->run: Incorrect result for lorLow2!");

     // long OR Immediate 32bit value
     lres = lorLowMinus( li );
     if( lres != -1L )
        Assert.fail("addImmed->run: Incorrect result for lorLowMinus!");

     // long OR Immediate 32bit value
     lres = lorLowMinus2( li );
     if( lres != -19376L )
        Assert.fail("addImmed->run: Incorrect result for lorLowMinus2!");

     // long OR Immediate 16bit value
     lres = lorHH( li );
     if( lres != 0x5005000000033450L )
        Assert.fail("addImmed->run: Incorrect result for lorHH!");

     // long OR Immediate 16bit value
     lres = lorHL( li );
     if( lres != 0x511500033450L )
        Assert.fail("addImmed->run: Incorrect result for lorHL!");

     // long OR Immediate 16bit value
     lres = lorLH( li );
     if( lres != 0x52273450 )
        Assert.fail("addImmed->run: Incorrect result for lorLH!");

     // long OR Immediate 16bit value
     lres = lorLH2( li );
     if( lres != 0x1133450 )
        Assert.fail("addImmed->run: Incorrect result for lorLH2!");

     // long OR Immediate 16bit value
     lres = lorLL( li );
     if( lres != 0x37775 )
        Assert.fail("addImmed->run: Incorrect result for lorLL!");
     
     // long OR Immediate 16bit value
     lres = lorLL2( li );
     if( lres != 0x33674 )
        Assert.fail("addImmed->run: Incorrect result for lorLL2!");

  }
  
  // int OR
  static int ior( int i )
  {
    // test OILF
    int res = i | 0x11112222;
    return res;
  }

  static int iorLH( int i)
  {
    // test OILH - when GE disabled.
    //      GE enabled  64 bit - OILF
    //      GE enabled  31 bit - OILF
    //      GE disabled 64 bit - OILH
    //      GE disabled 31 bit - OILH
    int res = i |  0x43210000;
    return res;
  } 

  static int iorLL( int i )
  {
    // test OILL - when GE disabled.
    // When GE enabled, OILF
    //      GE disabled 64-bit OILL
    //      GE disabled 31-bit OILL
    int res = i |  0x00005678;
    return res;
  }
 
  static long lorHigh( long i )
  {
    // test OIHF when GE enabled.
    //      GE enabled 64 bit - OIHF
    //      GE enabled 31 bit - OILF
    long res = i |  0x0004000800000000L;
    return res;
  }

  static long lorLow( long i )
  {
    // test OILF - GE enabled only.
    //      GE enabled 64 bit - OILF
    //      GE enabled 31 bit - OILF
    long res = i | 0x000000007FFFFFFFL;
    return res;
  }

  static long lorLow2( long i )
  {
   // long constant that is 31-bit value
    // GE enabled 64 bit - OILF
    // GE enabled 31 bit - OILF
    // GE disabled - literal pool
    long res = i | 0x7654L;
    return res;
  } 
 
  static long lorLowMinus( long i )
  {
   // long constant that is 31-bit value with non-zero in high 32 bits
    // GE enabled 64 bit - None
    // GE enabled 31 bit - None
    // GE disabled - literal pool
    long res = i | -1L;
    return res;
  }

  static long lorLowMinus2( long i )
  {
   // long constant that is 31-bit value with non-zero in high 32 bits
    // GE enabled 64 bit - None
    // GE enabled 31 bit - None
    // GE disabled - literal pool
    long res = i | -32768L;
    return res;
  }

  static long lorHH( long i)
  {
    // test OIHH - when GE disabled
    // when GE enabled  64 bit - OIHF
    //      GE enabled  31 bit - OILF
    //      GE disabled 64 bit - OIHH
    //      GE disabled 31 bit - OILH
    long res = i | 0x5005000000000000L;
    return res;
  }

  static long lorHL( long i)
  {
    // test OIHL - when GE disabled.
    // When GE enabled  64 bit - OIHF
    //      GE enabled  31 bit - OILF
    //      GE disabled 64-bit - OIHL
    //      GE disabled 31-bit - OILL
    long res = i |  0x0000511500000000L;
    return res;
  }


  static long lorLH( long i)
  {
    // test OILH - when GE disabled.
    //      GE enabled  64 bit - OILF
    //      GE enabled  31 bit - OILF
    //      GE disabled 64 bit - OILH
    //      GE disabled 31 bit - OILH
    long res = i |  0x0000000052250000L;
    return res;
  }

  static long lorLH2( long i)
  {
    // test OILF - a 31 bit value,
    //      GE enabled  64 bit - OILF
    //      GE disabled 64 bit - OILH
    long res = i |  0x01110000L;
    return res;
  }

  static long lorLL( long i)
  {
    // test OILL - when GE disabled.
    // When GE enabled, OILF
    //      GE disabled 64-bit OILL
    //      GE disabled 31-bit OILL
    long res = i |  0x0000000000005335L;
    return res;
  } 

  static long lorLL2( long i )
  {
    // test OILF - 32 bit value, can never be a OILL
    //      GE enabled 64 bit - OILF
    long res = i |  0x00001234L;
    return res;
  }

}
