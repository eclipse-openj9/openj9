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
// Test file: AndImmed.java

package jit.test.jitt.immedOp;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class andImmed extends jit.test.jitt.Test {

   @Test
   public void testandImmed() {
 
     //**************************************************************
     //*
     //*  Integer constants AND immediate
     //*
     //**************************************************************
     int i = 10000;
     int ires = 0;

     // int AND Immediate 32int value
     ires = iand( i ); 
     if( ires != 0x300 )
        Assert.fail("andImmed->run: Incorrect result for iand!");
 
     // int AND Immediate 32int value
     ires = iandLH( i ); 
     if( ires != 0x2710 )
        Assert.fail("andImmed->run: Incorrect result for iaddLH!");
 
     //**************************************************************
     //*
     //*  Long constants AND immediate
     //*
     //**************************************************************
     long li = 210000L;
     long lres = 0;

     // long AND Immediate 32int value
     lres = landHigh( li ); 
     if( lres != 0x33450 )
        Assert.fail("andImmed->run: Incorrect result for landHigh!");
 
     // long AND Immediate 32bit value  
     lres = landLow( li );
     if( lres != 0x1450 )
        Assert.fail("addImmed->run: Incorrect result for landLow!");

     // long AND Immediate 32bit value  
     lres = landLow2( li );
     if( lres != 0x1450 )
        Assert.fail("addImmed->run: Incorrect result for landLow2!");

     // long AND Immediate 32bit value -negative
     lres = landMinus( li );
     if( lres != 210000 )
        Assert.fail("addImmed->run: Incorrect result for landLow2!");

     // long AND Immediate 32bit value - use GE
     lres = landHH( li );
     if( lres != 0x33450 )
        Assert.fail("addImmed->run: Incorrect result for landHH!");

     // long AND Immediate 32bit value - use GE
     lres = landHH2( li );
     if( lres != 0x33450 )
        Assert.fail("addImmed->run: Incorrect result for landHH2!");

     // long AND Immediate 32bit value - use GE
     lres = landHL( li );
     if( lres != 0x33450 )
        Assert.fail("addImmed->run: Incorrect result for landHL!");

     // long AND Immediate 32bit value - use GE
     lres = landLH( li );
     if( lres != 0x33450 )
        Assert.fail("addImmed->run: Incorrect result for landLH!");

     // long AND Immediate 32bit value - use GE
     lres = landLL( li );
     if( lres != 0x30400 )
        Assert.fail("addImmed->run: Incorrect result for landLL!");
  }
  // int AND
  static int iand( int i )
  {
    // test NILF - GE enabled - 64bit and 32bit
    int res = i & 0x12344321;
    return res;
  }

  static int iandLH( int i)
  {
    // test NIHH only when GE is disabled,
   //       GE enabled  64 bit - NILF
   //       GE enabled  31 bit - NILF 
   //       GE disabled 64 bit - NILH
   //       GE disabled 31 bit - NILH
    int res = i & 0x2323FFFF;
    return res;
  } 

  static long landHigh( long i )
  { 
    // test NIHF - 64bit GE enabled
    //      NILF - 31bit GE enabled
    long res = i & 0x00000055FFFFFFFFL;
    return res;
  }
   
  static long landLow( long i )
  { 
    // test NILF - GE enabled  
    //        GE disabled 64 bit - literal pool
    long res = i & 0xFFFFFFFF00005678L;     // -4294945160
    return res;
  }  
 
  static long landLow2( long i )
  {
    // long constant that is 31-bit value
    //  31-bit GE enabled  - NILF,
    //         GE disabled - literal pool
    long res = i & 0x12345678L;
    return res;
  } 
  
  static long landMinus( long i )
  {
    // long constant that is neg 31-bit value
    //         GE - literal pool
    long res = i & -1L;
    return res;
  }
  
  static long landHH( long i)
  {
    // test NIHH only when GE is disabled,
    //       GE enabled  64 bit - NIHF
    //       GE enabled  31 bit - NILF
    //       GE disabled 64 bit - NIHH
    //       GE disabled 31 bit - NILH
    long res = i & 0x0101FFFFFFFFFFFFL;
    return res;
  }

  static long landHH2( long i )
  {  
    // long constant that is 31-bit value
    //  31-bit GE enabled  - NILF,
    //         GE disabled - literal pool
    long res = i & 0x7FFFFFFFL;
    return res;
  }

  static long landHL( long i)
  {
    //       GE enabled  64 bit - NIHF
    //       GE enabled  31 bit - NILF
    //       GE disabled 64 bit - NIHL
    //       GE disabled 31 bit - NILL
    // negative number
    long res = i & 0xFFFF0202FFFFFFFFL;  // -279263068553217   xFFFF0202 = -65022, 0202 = 514
    return res;
  }


  static long landLH( long i)
  {
    // test  NILH when GE disabled
    //       GE enabled  64 bit - NILF
    //       GE enabled  31 bit - NILF
    //       GE disabled 64 bit - NILH
    //       GE disabled 31 bit - NILH
    long res = i & 0xFFFFFFFF0303FFFFL;
    return res;
  }

  static long landLL( long i)
  {
    // test NILL when GE is disabled, negative number.
    //       GE enabled  64 bit - NILF
    //       GE enabled  31 bit - NILF
    //       GE disabled 64 bit - NILL
    //       GE disabled 31 bit - NILL
    long res = i & 0xFFFFFFFFFFFF0404L;    // (for xFFFF0404 = -64508 4294902852 )
    return res;
  } 

  static long landLL2( long i )
  { 
    // test  NILL - disabled GE
    // when GE enabled xFFFF0000 is > max 32int, and value doesn't match any the mask,
    // so bitwiseOp return true, use literal pool.
    long res = i & 0xFFFF0444L;   // 4294902852
    return res;
  }

}
