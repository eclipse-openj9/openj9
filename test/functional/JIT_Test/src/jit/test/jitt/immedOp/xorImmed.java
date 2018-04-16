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
// Test file: xorImmed.java

package jit.test.jitt.immedOp;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class xorImmed extends jit.test.jitt.Test {

   @Test
   public void testxorImmed() {
 
     //**************************************************************
     //*
     //*  Integer constants XOR immediate
     //*
     //**************************************************************
     int i = 10000;
     int ires = 0;

     // int XOR Immediate 32int value
     ires = ixor( i ); 
     if( ires != 0x4176 )
        Assert.fail("xorImmed->run: Incorrect result for ixor!");

     // int XOR Immediate 32int value
     i = 32767;
     ires = imxor( i );
     if( ires != -32768 )
        Assert.fail("xorImmed->run: Incorrect result for imxor1!");

      // int XOR Immediate 32int value
     i = -32768;
     ires = imxor( i );
     if( ires != 32767 )
        Assert.fail("xorImmed->run: Incorrect result for imxor2!");

     //**************************************************************
     //*
     //*  Long constants XOR immediate
     //*
     //**************************************************************
     long li = 210000L;
     long lres = 0;

     // long XOR Immediate 32int value - use GE
     lres = lxor( li ); 
     if( lres != 0x567800036228L )
        Assert.fail("xorImmed->run: Incorrect result for lxor!");
 
     // long XOR Immediate 32int value - use GE
     li = 0x7FFFFFFFL;
     lres = lxorMax( li ); 
     if( lres != 2146290601L )
        Assert.fail("xorImmed->run: Incorrect result for lxorMax!");

     // long XOR Immediate 32int value - use GE
     li = 0x7FFFFFFFL;
     lres = lmxor( li );
     if( lres != -2147483648L )
        Assert.fail("xorImmed->run: Incorrect result for lmxor!");

 
  }
  
  // int OR
  static int ixor( int i )
  {
    // test XILF
    int res = i ^ 0x6666;
    return res;
  }

  // int OR
  static int imxor( int i )
  {
    // test XILF
    int res = i ^ -1;
    return res;
  }

  // long XOR
  static long lxor( long i )
  { 
    // long constant > 32 bit value - use literal pool
    long res = i ^ 0x0000567800005678L;
    return res;
  }

  // long XOR
  static long lxorMax( long i )
  {
    // long constant  is 32bit value,
    //      GE enabled  64 bit - XILF
    //      GE disabled 64 bit - literal pool
    //      GE enabled  31 bit - XILF
    //      GE disabled 31 bit - literal pool
    long res = i ^ 0x123456L;
    return res;
  }

  static long lmxor( long i ) 
  {
    // test XILF
    long res = i ^ -1L;
    return res;
  }

 
}
