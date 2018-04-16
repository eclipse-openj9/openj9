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
// Test file: addImmed.java
//      Test new add immediate Golden Eagle instructions

package jit.test.jitt.immedOp;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class addImmed extends jit.test.jitt.Test {

   @Test
   public void testaddImmed() {
 
     //**************************************************************
     //*
     //*  Integer constants Add immediate
     //*
     //**************************************************************
     int i = 10000;
     int ires = 0;
     
     for (int j = 0; j < sJitThreshold; j++)
     {

     // int add - Add Immediate 32int value
     ires = addImmed.iadd12345( i ); 
     if( ires != 22345 )
        Assert.fail("addImmed->run: Incorrect result for iadd12345!");
 
     // int add - Add Immediate 32int value
     ires = addImmed.iaddMin( i ); 
     if( ires != -22768 )
        Assert.fail("addImmed->run: Incorrect result for iaddMin!");
 
     // int add - Add Immediate 32int value
     ires = addImmed.iaddMax( i ); 
     if( ires != 42767 )
        Assert.fail("addImmed->run: Incorrect result for iaddMax!");
 
     // int add - Add Immediate 32bit value - use GE
     ires = addImmed.iaddGE50000( i ); 
     if( ires != 60000 )
        Assert.fail("addImmed->run: Incorrect result for iaddGE50000!");

     // int add - Add Immediate 32bit value - use GE
     ires = addImmed.iaddGEMin( i ); 
     if( ires != -2147473647 )
        Assert.fail("addImmed->run: Incorrect result for iaddGEMin!");

     // int add - Add Immediate 32bit value - use GE
     ires = addImmed.iaddGEMax( i ); 
     if( ires != -2147473649 )
        Assert.fail("addImmed->run: Incorrect result for iaddGEMax!");


     //**************************************************************
     //*
     //*  Long constants Add immediate
     //*
     //**************************************************************
     long li = 210000L;
     long lres = 0;

     // long Add - Add Immediate 32bit value  
     lres = addImmed.ladd300001( li );
     if( lres != 510001 )
        Assert.fail("addImmed->run: Incorrect result for ladd300001!");

     
     // long add - Add Immediate 32bit value - use GE
     lres = addImmed.laddGE600001( li );
     if( lres != 810001 )
        Assert.fail("addImmed->run: Incorrect result for laddGE600001!");


     // long add - Add Immediate 32bit value - use GE
     lres = addImmed.laddGEMax( li );
     if( lres != 2147693647L  )
        Assert.fail("addImmed->run: Incorrect result for laddGEMax!");

     }

  }

  // int Add - AHI
  static int iadd12345( int i )
  {
    int res = i + 12345;
    return res;
  }
 
  // int Add - AHI
  static int iaddMin( int i )
  {
    int res = i - 32768;
    return res;
  }

  // int Add - AHI
  static int iaddMax( int i )
  {
    int res = i + 32767;
    return res;
  }
 
  // int Add using Golden Eagle instruction - AFI
  static int iaddGE50000( int i )
  {
    int res = i + 50000;
    return res;
  }
  
  // int Add using Golden Eagle instruction - AFI
  static int iaddGEMin( int i )
  {
    int res = i + (-2147483647);
    return res;
  }

  // int Add using Golden Eagle instruction - AFI
  static int iaddGEMax( int i )
  {
    int res = i + 2147483647;
    return res;
  }
  
  // long Add - AGHI
  static long ladd300001(long i )
  {
    long res = i + 300001L;
    return res;
  }
 
  // long Add using Golden Eagle instruction - AGFI
  static long laddGE600001( long i )
  {
    long res = i + 600001L;
    return res;
  }

  // long Add using Golden Eagle instruction - AGFI
  static long laddGEMax( long i )
  {
    long res = i + 2147483647L;
    return res;
  }

}
