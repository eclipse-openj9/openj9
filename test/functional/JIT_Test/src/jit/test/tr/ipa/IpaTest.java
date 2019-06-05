/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package jit.test.tr.ipa;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.io.*;
import java.lang.reflect.*;
import org.testng.log4testng.Logger;

// JIT options to use:
// -Xjit:dontInline={IPA_*}
@Test(groups = { "level.sanity","component.jit" })
public class IpaTest
{
    int _iterations = 1000000;
    int _field1 = 0;
    int _field2 = 0;
    boolean _callInterface = false;
    // use synchronized methods to create monitors
    // use final methods to avoid virtual calls
    // use get methods because set methods disable monitor coarsening (write to globals)
    // do not use static methods because the monitors will get different values
    //  and coarsening will not happen
    private final synchronized int getField1() { return _field1;}
    private final synchronized int getField2() { return _field2;}
    
    //This function should not run as a test
    @Test(enabled = false)
    public final synchronized void setField2(int val) {_field2 = val;}
    protected final boolean getCallInterface() { return _callInterface;}
    private final void setCallInterface(boolean val) { _callInterface = val;}
    
    private static Logger logger = Logger.getLogger(IpaTest.class);
   
    //------------------------------
    // Test 0 invalidating assumptions
    //------------------------------
    @Test
    public void hierarchyTest0() {
        IPA_A1 a1Obj = new IPA_C3(); // C3 is derived from A1
        String answer = null;
        // execute a number of times to get it compiled
        for (int i=0; i < _iterations; i++)
            answer = test0(a1Obj);
        logger.debug("Finished test0 step1");
        AssertJUnit.assertEquals("C3.work0", answer);
        // Classes will be extended and assumptions will be violated
        a1Obj = new IPA_C4(); // this will load C4 class and extend A1 hierarchy
        answer = test0(a1Obj);
        logger.debug("Finished test0 step2");
        AssertJUnit.assertEquals("Modified field2", answer);
    }
    private final String test0(IPA_A1 obj) { 
        int bogus = 0;
        // the IF below has the role to create several basic blocks
        // because coarsening does not work within same basic block
        if(getField1()==-1000) // some bogus comparison that will never become true
            bogus++;
        // call some method
        String answer = obj.work0(this);
        // when work0 will be overridden, the new implementation will write 1
        // to the field2, so getField2() should return a 1
        int i = getField2();
        if (i != 0)
            return "Modified field2";
        return answer;
    }

    //-------------------------------
    // Test 1 invalidating assumptions
    //--------------------------------
    @Test
    public void hierarchyTest1() {
        IPA_A1 a1Obj = new IPA_C3(); 
        String answer = null;
        for (int i=0; i < _iterations; i++)
            answer = test1(a1Obj);
        logger.debug("Finished test1 step0");
        AssertJUnit.assertEquals("A2.work1", answer);
        
        setCallInterface(true); // this will make work1 take a different path
        answer = null;
        for (int i=0; i < _iterations; i++)
            answer = test1(a1Obj);
        logger.debug("Finished test1 step1");
        AssertJUnit.assertEquals("C5.work4", answer);
    }
    private final String test1(IPA_A1 obj) { 
        int bogus = 0;
        if (getField1()==-1000)
            bogus++;
        // call some method
        String answer = obj.work1(this);
        int i = getField2();
        return answer;
    }
    
    //----------------------------------------
    // Test recursive calls
    //----------------------------------------
    @Test
    public void recursiveTest() {
        IPA_A1 a1Obj = new IPA_C3(); 
        String answer = null;
        for (int i=0; i < _iterations; i++)
            answer = test2(a1Obj);
        logger.debug("Finished test2 step0");
        AssertJUnit.assertEquals("A1.work2", answer);
    }
    private final String test2(IPA_A1 obj) { 
        int bogus = 0;
        if (getField1()==-1000)
            bogus++;
        // call some method
        String answer = obj.work2(this);
        int i = getField2();
        return answer;
    }
    
    //-------------------------------------
    // Test mutually recursive calls
    //-------------------------------------
    @Test
    public void mutuallyRecursiveTest() {
        IPA_A2 a2Obj = new IPA_C3(); 
        String answer = null;
        for (int i=0; i < _iterations; i++)
            answer = test3(a2Obj);
        logger.debug("Finished test3 step0");
        AssertJUnit.assertEquals("A2.work3", answer);
    }
    private final String test3(IPA_A2 obj) { 
        int bogus = 0;
        if (getField1()==-1000)
            bogus++;
        // call some method
        String answer = obj.work3(this);
        int i = getField2();
        return answer;
    }

    //---------------------------
    // Test large sniffing depth
    //---------------------------
    @Test
    public void largeSniffTest() {
        IPA_C3 c3Obj = new IPA_C3(); 
        String answer = null;
        for (int i=0; i < _iterations; i++)
            answer = test4(c3Obj);
        logger.debug("Finished test4 step0");
        AssertJUnit.assertEquals("C3.work4", answer);
    }
    private final String test4(IPA_C3 obj) { 
        int bogus = 0;
        if (getField1()==-1000)
            bogus++;
        // call some method
        String answer = obj.work4(this);
        int i = getField2();
        return answer;
    }  
}

// create some class hierarchy

//                      A1     I7
//                    /    \  /  \
//                  A2      C5    I8
//                 /  \      \     \
//                C3  C4      A6    C9

//---------------------------------------------------------
abstract class IPA_A1
{
   int recursiveAdd(int n) { return (n<=0 ? 0 : 1 + recursiveAdd(n-1)); }
   abstract String work0(IpaTest o); // no implementation for this one
   abstract String work1(IpaTest o); // no implementation for this one
   String work2(IpaTest o)
   {
      if (recursiveAdd(2) == 2)
         return "A1.work2";
      else
         return "A1.work2";
   }
}

//---------------------------------------------------
// A2 will implement work1, but leave work0 undefined
abstract class IPA_A2 extends IPA_A1
{
   int recurs1(int val) { return (val <= 0 ? 0 : 1+recurs2(val-1)); }
   int recurs2(int val) { return (val <= 0 ? 0 : 1-recurs1(val-1)); }
   // work1 needs to call an interface method
    String work1(IpaTest o)
    {
       if (o.getCallInterface())
       {
          IPA_I7 i7 = new IPA_C5();
          return i7.work4(o);
       }
     return "A2.work1";
    }
   String work3(IpaTest o)
   {
      if (recurs1(10) == 0)
         return "A2.work3";
      else
         return "A2.work3";
   }
}

//--------------------------------------------------
class IPA_C3 extends IPA_A2
{
   int h1() {return h2();}
   int h2() {return h3();}
   int h3() {return h4();}
   int h4() {return h5();}
   int h5() {return h6();}
   int h6() {return h7();}
   int h7() {return h8();}
   int h8() {return h9();}
   int h9() {return h10();}
   int h10() {return 1;}
    String work0(IpaTest o)
    {
        return "C3.work0";
    }
   String work4(IpaTest o)
   {
      if (h1() == 1)
         return "C3.work4";
      else
         return "C3.work4";
   }

}

//--------------------------------------------------
class IPA_C4 extends IPA_A2
{
    String work0(IpaTest o)
    {
       o.setField2(1);
       return "C4.work0";
    }
    String work2(IpaTest o)
    {
        return "C4.work2";
    }
}

//---------------------------------------------------
interface IPA_I7
{
    String work4(IpaTest o);
}

//-------------------------------------------------
class IPA_C5 extends IPA_A1 implements IPA_I7
{
    String work0(IpaTest o)
    {
        return "C5.work0";
    }
    String work1(IpaTest o)
    {
        return "C5.work1";
    }
    public String work4(IpaTest o)
    {
        return "C5.work4";
    }
}

//----------------------------------------------------
abstract class IPA_A6 extends IPA_C5
{
    abstract String work10();
}


