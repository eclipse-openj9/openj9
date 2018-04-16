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
// Test file: cmpImmed.java
//      Test new compare immediate Golden Eagle instructions

package jit.test.jitt.immedOp;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class cmpImmed extends jit.test.jitt.Test {

   @Test
   public void testcmpImmed() {

     long ires = 0;
     long lres = 0;

     int i=30000;
     long li = 33000L;

     //**************************************************************
     //*
     //*  Integer constants Compare immediate
     //*
     //**************************************************************
     // int Compare Immediate - CHI
     ires = icmp( i );
     if( ires != 31000 )
        Assert.fail("cmpImmed->run: Incorrect result for icmp!");

     // int Compare Immediate - CFI - Golden Eagle
     i = 45000;
     ires = icmp_ge( i );
     if( ires != 46000 )
        Assert.fail("cmpImmed->run: Incorrect result for icmp_ge!");

     //**************************************************************
     //*
     //*  Long constants Compare immediate
     //*
     //**************************************************************
     // long Compare Immediate 32bit value - CGHI
     li = 31000L;
     lres = lcmp( li );
     if( lres != 32000L )
        Assert.fail("cmpImmed->run: Incorrect result for lcmp!");

        li = 100000L;
        lres = lcmp_ge(li );
        if( lres != 101000 )
           Assert.fail("cmpImmed->run: Incorrect result for lcmp_ge!");

     //**************************************************************
     //*
     //*  Long constants Load
     //*
     //**************************************************************
     li = 0x2100000000L;
     lres = lload_ge( li );
     if( lres != 0x2100000010L )
        Assert.fail("cmpImmed->run: Incorrect result for lload_ge!");

   }
 
  
  // int Compare - CHI
  static int icmp( int i )
  {
    int res =0;
    if( i > 29000 )
    {
       // test load LHI
       res = i + 1000;
    }
    return res;
  }
 
  
  // int Compare - CFI
  static int icmp_ge( int i )
  {
    int res =0;
    if( i > 40000 )
    {
       // test load IILF
       res = i + 1000;
    }
    return res;
  }
 
  // long Compare - CGHI 
  static long lcmp( long i )
  {
     // test CGHI
     long res =0;
     if( i > 30000L )
     {
         res = i + 1000;
     }
     else 
        res = 1000;

     return res;      
  }
  
  // long Compare using Golden Eagle instructions
  static long lcmp_ge( long i )
  {
      long res;
      // test CGFI, CLGFI
      long big =1000L;
    
      if( i > 90000L )
      {
          res = big + i;
      }
      else
          res = 1000;
      return res;
  }

  // long load constant using Golden Eagle instruction - LLIHF
  long lload_ge( long i)
  {
     // long big = 0x2100000000L;
      long big = i;
      long res = 0;
      if( i > 1000L )
      {
         res =  i + 0x010L;

      }
      else
      {
         res = 1000;
      }  
    
      return res;
  }

}
