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
package jit.test.tr.VPTypeTests;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.text.ParseException;
import org.testng.log4testng.Logger;

// import some good class hierarchies
import java.io.*;
import java.util.zip.*;
import java.util.Iterator;
import java.util.Random;
import java.util.Vector;

//TODO: collapse subtests into tests?
@Test(groups = { "level.sanity","component.jit" })
public class cTypeTests {

   //ctor
   private static Logger logger = Logger.getLogger(cTypeTests.class);
   @Test
   public void test001() {
      // defect 85007
      boolean bool = false;
      O1 o1 = new O1(314);
      // iaload<vft-symbol>
      //    aload O1
      Class c1 = (Class)o1.getClass(); 
      Class c2 = null;
      try {
         // return type is java/lang/Class
         c2 = Class.forName("jit.test.tr.VPTypeTests.O1");
      }
      catch (Exception e) {
         logger.debug(e.toString());
      }
     
      // ifacmpeq
      //    aload <O1, non-null, classobject>
      //    aload <java.lang.Class, non-null>
      if (c1 == c2)
         bool = true;
      logger.debug("Running test1 ... ");
      AssertJUnit.assertTrue(bool);
   }
   
   @Test
   public void test002() {
      // defect 89089
      O1 o1 = new O1(314);
      // iaload<vft-symbol>
      //    aload O1
      Class c1 = (Class)o1.getClass();
      // instanceof
      //    aload    <O1, non-null, classobject>
      //    loadaddr <java.lang.Class, non-null, classobject>
      boolean b = c1 instanceof Class;
      logger.debug("Running test2 ... ");
      AssertJUnit.assertTrue(b);
   }

   @Test
   public void test003() {
      boolean bool = false;
      O1 o1 = new O1(314);
      O1 o2 = new O2();
      Class c1 = o2.m2(o1); // returns O1
      Object c2 = o1.getClass();
      // iaload<vft-symbol>
      //    aload O1
      bool = ((Class)c2).isAssignableFrom(c1);
      logger.debug("Running test3 ... ");
      AssertJUnit.assertTrue(bool);
   }
   
   @Test
   public void test004() {
      // defect 93032
      boolean bool = true;
      O1 o1 = new O1(314);
      // iaload<vft-symbol>
      //    aload O1
      Object c2 = o1.getClass();
      // ificmpeq
      //    instanceof 
      //       aload    <O1, non-null, classobject>
      //       loadaddr <O2, non-null, classsobject>
      //    iconst    
      //    
      // instanceof should fail
      // if classobject is set on aload, its actually of 
      // type java.lang.Class & java.lang.Class is not 
      // an instanceof O2 
      if (c2 instanceof O2) 
         bool = false;
      
      AssertJUnit.assertTrue(bool);
      
      bool = true;
      // ificmpeq
      //    instanceof 
      //       aload    <O1, non-null, classobject>
      //       loadaddr <O1, non-null, classobject>
      //    iconst
      //    
      // instanceof should fail
      // if classobject is set on aload, its actually of 
      // type java.lang.Class & java.lang.Class is not 
      // an instanceof O1 
      if (c2 instanceof O1) 
         bool = false;
      logger.debug("Running test4 ... ");
      AssertJUnit.assertTrue(bool);
   }
   
   @Test
   public void test005() {
      // defect 93032
      boolean bool = false;
      O1 o1 = new O1();
      // iaload<vft-symbol>
      //    aload
      Object c1 = o1.getClass();
       try {
         Class c2 = Class.forName("jit.test.tr.VPTypeTests.O3");
      } catch (Exception e) {
         logger.debug(e.toString());
      }
      try {
         // checkcast
         //    aload    <O1, non-null, classobject>
         //    loadaddr <O1, non-null, classobject>
         // 
         // checkcast throws a ClassCastException
         // if classobject is set on aload, its actually of 
         // type java.lang.Class & java.lang.Class cannot be 
         // cast to O1
         O2 o2 = ((O1)c1).o; 
         bool = false;
      } catch (Exception e) {
         bool = true;    
      }
      logger.debug("Running test5 ... ");
      AssertJUnit.assertTrue(bool);
   }

   @Test
   public void test006() {
      boolean bool = false;
      Object o = new O1(98);
      // iaload<vft-symbol>
      //    aload O1
      Object c1 = o.getClass();
      // ificmpeq
      //    aload    <O1, non-null, classobject>
      //    loadaddr <java.lang.Class, non-null, classobject>
      if (c1 instanceof Class) // instanceof should succeed
         bool = true;
      
      logger.debug("Running test6 ... ");
      AssertJUnit.assertTrue(bool);
   }
  
   @Test
   public void test007() {
      try {
         // init some class hierarchy
         InputStream o = new ZipInputStream(System.in);
         subtest007(o);
      } catch (Exception e) {
    	 logger.debug(e.toString());
      }
   }
 
   private void subtest007(InputStream o) {
      // defect 89722
      boolean bool1 = false;
      boolean bool2 = false;
      InputStream is = (InputStream)o;
      Class c = null;
      try {
         c = Class.forName("java.io.FilterInputStream");
      } catch (Exception e) {
    	 logger.debug(e.toString());
      }
      Class ci = o.getClass();
      // walk a class hierarchy
      do {
         if (is == null || is instanceof CheckedInputStream)
            break;
         for (; ci != null && (ci != c); ci = ci.getSuperclass())
            ;
         if (ci != c)
            break;
         else {
            bool1 = true;
            // checkcast should succeed
            Object o1 = ((FilterInputStream)o);
            bool2 = true;
            break;
         }
      } while (true);
      
      logger.debug("Running test7a ... ");
      AssertJUnit.assertTrue(bool1);
      logger.debug("Running test7b ... ");
      AssertJUnit.assertTrue(bool2);
   }
  
   @Test
   public void test008() {
      {
         O1 o = new O1(98);
         subtest008(o);
      }
   }

   private void subtest008(Object o) {
      boolean bool = false;
      Class c = o.getClass();
      try {
         if (o instanceof O1) {
            // checkcast
            //    aload    <O1, non-null>
            //    loadaddr <java.lang.Class, non-null, classobject>
            Class c1 = (Class)o; //checkcast should fail 
            bool = false;
         }
      } catch (Exception e) {
         bool = true;
      } 
      logger.debug("Running test8 ... ");
      AssertJUnit.assertTrue(bool);
   } 

   @Test
   public void test009() {
      // defect 90268
      boolean bool1 = false;
      boolean bool2 = false;
      Object oa, ob;
      O1 o1 = new O2();
      // this condition will be true
      // ie. the result of this call is 'null'
      if (o1.m4(true) == null) {
         oa = null; // constraint is <null>
         ob = null;
      }
      else {
         oa = new O3(0); // constraint is <O3, non-null>
         ob = new O5();
      }
      // merge point
      // constraint for oa is obtained by merging
      // <O3, non-null> and <null> which results in
      //               <O3> 
      // ie. null/non-null'ness is not known
      O1 o2 = new O2();
      
      // the result of this call is 'null'
      // ie. the condition will be true
      // also, constraints at this point
      // ifacmpeq
      //     aload O2
      //     aload O3
      if (o2.m4(true) == oa)
         bool1 = true;
      
      
      logger.debug("Running test9a ... ");
      AssertJUnit.assertTrue(bool1);
      
      // redo the same test; but now 
      // lhs and rhs of the branch are both final classes
      // so the type constraints will be 'fixed'
      O3 o4 = new O4();
      if (o4.m2(true) == ob)
         bool2 = true;
         
      logger.debug("Running test9b ... ");
      AssertJUnit.assertTrue(bool2);
   }

   @Test
   public void test010() {
      // defect 90637
      boolean bool1 = false;
      boolean bool2 = true;
      try {
         Object o = new Object(); // gets global constraint <fixed: Object, non-null>
         O2 o2 = new O2(); // global <fixed: O2, non-null>
         IC1 ic1 = (IC1)o; // <unresolved: IC1, non-null>, should cause a class-cast exception
         bool2 = false;    // should not get here 
         o2 = (O2)o;       // rest of this block should have gone away..
      } catch(Exception e) {
         bool1 = true;
      } 
      logger.debug("Running test10a ... ");
      AssertJUnit.assertTrue(bool1);
      
      logger.debug("Running test10b ... ");
      AssertJUnit.assertTrue(bool2);
   }
   
   @Test
   public void test011() {
      boolean bool = true;
      // TBD:
      // repeat above test with other class 
      // as unresolved arrays, cloneable, serializable or object types
      // to test my fix to 90637
      logger.debug("Running test11 ... ");
      AssertJUnit.assertTrue(bool);
   }

   @Test
   public void test012() {
      {
         String [] s = new String[5];
         subtest012(s);
      }
   }

   private void subtest012(String s[]) {
      boolean bool = false;
      Double d = new Double(1.1);
      Object o = d;
      o = s;
      try {
      Float f = (Float)o;
      } catch (Exception e) {
         bool = true;
      }
      logger.debug("Running test12 ... ");
      AssertJUnit.assertTrue(bool);
   }

   @Test
   public void test013() {
      // was defect
      boolean bool1 = false;
      Object oa;
      O1 o1 = new O2();
      // this condition will be true
      // ie. the result of this call is 'null'
      if (o1.m4(true) == null) {
         oa = null; // constraint is <null>
      }
      else {
         oa = new O3(0); // constraint is <O3, non-null>
      }
      // merge point
      // constraint for oa is obtained by merging
      // <O3, non-null> and <null> which results in
      //               <O3> 
      // ie. null/non-null'ness is not known
      O1 o2 = new O2();
      
      // the result of this call is 'null'
      // ie. the condition will be true
      //if (o2.m4(true) == oa)
      if (oa == null) {
         if ((O2 [])o2.m5(true) == null) {
            bool1 = true;
         }
      }
      
      logger.debug("Running test13 ... ");
      AssertJUnit.assertTrue(bool1);
   }

   @Test
   public void test014() {
      // defect 85007 - another variation 
      // where constraint for c2 is now only j/l/Class 
      // after a merge point
      // 
      boolean bool = false;
      O1 o1 = new O1(314);
      // iaload<vft-symbol>
      //    aload
      Class c1 = (Class)o1.getClass(); 
      Class c2;
      // this condition will be true
      // ie. the result of this call is 'null'
      if (o1.m4(true) != null) {
         c2 = null;
      } 
      else {
         try {
            c2 = Class.forName("jit.test.tr.VPTypeTests.O1");
         }
         catch (Exception e) {
        	logger.debug(e.toString());
            return;
         }
      }
      // ifacmpeq
      //    aload <O2, non-null, classobject>
      //    aload <java.lang.Class>
      if (c1 == c2)
         bool = true;
      logger.debug("Running test14 ... ");
      AssertJUnit.assertTrue(bool);
   }

//this test should be run with -Xjit:disableInlining
   @Test
   public void test015() {
      // defect 105496 raised by a customer PMR
      // test merging of constraints
      // the merge should account for the fact that one could 
      // have an inconsistency between  a back edge store constraint 
      // and the constraint flowing out from an exit edge, especially 
      // if the exit condition is something like :
      // for (...) {
      //    ..
      //    if (x == 0) 
      //       break;
      //    else
      //       ...
      // } 
      // 
      // x != 0 flows as a store constraint on back edge
      // x == 0 is flowing on the exit edge.
      //
	  logger.debug("Running test15 ... ");
      int i = 0;
      for (i = 0; i < 10; i++) {
         firstTimeL3 = true;
         firstTimeL2 = true;
         int j = subTest015(10);
         AssertJUnit.assertTrue((j == 20));
      }
   }

   private int subTest015(int i) {
      int j = 0;
      int k = 1;
      int l = stl;
      //int l2 = stl;
      //int l3 = stl;
      int answer = 10;
      if (l == 0) {
         for (;j < i;) {
            if (l3() == 0) {
               if (l2() == 0) {
                  answer = 2;
                  if (k == 1)
                     return 10;
               }
               break;
            }
            else
               k = 0;
            j++;
         }
      }
      return 20;
   }
   
   private static int l3() {
      if (firstTimeL3) {
         firstTimeL3 = false; 
         return 1;
      }
      return 0;
   }

   private static int l2() {
      /*
      if (firstTimeL2) {
         firstTimeL2 = false; 
         return 1;
      }
      */
      return 0;
   }
   
   @Test
   public void test016() {
      // defect 107713 customer PMR
      // test merging of store constraints
      //
      String cron = "10/20 * * * *";
      try {
         int elementLength = parseExpression(cron);
         logger.debug("Running test16 ... ");
         AssertJUnit.assertTrue(elementLength == 9);
      } catch (Exception e) {
         logger.debug(e.toString());
         AssertJUnit.assertTrue(false);
      }
   }

   private static int parseExpression(String expression) throws ParseException {
      try {
         // Reset all the lookup data
         for (int i = 0; i < lookup.length; lookup[i++] = 0) {
             lookupMin[i] = Integer.MAX_VALUE;
             lookupMax[i] = -1;
         }
 
         // Create some character arrays to hold the extracted field values
         char[][] token = new char[NUMBER_OF_CRON_FIELDS][];
         // Extract the supplied expression into another character array
         // for speed
         int length = expression.length();
         char[] expr = new char[length];
         expression.getChars(0, length, expr, 0);
 
         int field = 0;
         int startIndex = 0;
         boolean inWhitespace = true;
 
         // Extract the various cron fields from the expression
         for (int i = 0; (i < length) && (field < NUMBER_OF_CRON_FIELDS); i++) {
             boolean haveChar = (expr[i] != ' ') && (expr[i] != '\t');
             if (haveChar) {
                 // We have a text character of some sort
                 if (inWhitespace) {
                     startIndex = i; // Remember the start of this token
                     inWhitespace = false;
                 }
             }
   
             if (i == (length - 1)) { // Adjustment for when we reach the end of the expression
                i++;
             }
 
             if (!(haveChar || inWhitespace) || (i == length)) {
                // We've reached the end of a token. Copy it into a new char array
                token[field] = new char[i - startIndex];
                System.arraycopy(expr, startIndex, token[field], 0, i - startIndex);
                inWhitespace = true;
                field++;
             }
         }
 
         if (field < NUMBER_OF_CRON_FIELDS) {
            throw new ParseException("Unexpected end of expression while parsing \"" + expression + "\". Cron expressions require 5 separate fields.", length);
         }
 
         // OK, we've broken the string up into the 5 cron fields, now lets add
         // each field to their lookup table.
         int elementLength = 0;
         for (field = 0; field < NUMBER_OF_CRON_FIELDS; field++) {
             startIndex = 0;
             
             boolean inDelimiter = true;
             // We add each comma-delimited element separately.
             elementLength += token[field].length;
         }   
         ///System.out.println("elementLength = "+elementLength);
         return elementLength;
      }  catch (Exception e) {
            if (e instanceof ParseException) {
               throw (ParseException) e;
            } else {
               throw new ParseException("Illegal cron expression format (" + e.toString() + ")", 0);
            }
        }    
   }

   @Test
   public void test017() {
      // pmr
      O1 o1 = new O1(6032);
      Class c1 = (Class)o1.getClass();
      subTest017(c1);
   }
   private void subTest017(Object o) {
      boolean b = true;
      // instanceof
      //    aload o  <java/lang/Object>  (maybe classObject)           VPClassType
      //    loadaddr <fixed java/lang/Class, non-null, classObject>    VPClass 
      // test case5 Object typeIntersect 
      if (o instanceof Class) {
            // at this point, constraint of o
            // o <java/lang/Object, non-null> (non-null because instanceof succeeds)
         if (o == Class.class)
            b = false; 
      } 
      logger.debug("Running test17 ... ");
      AssertJUnit.assertTrue(b);
   }

   // mirror image of test017
   @Test
   public void test022() {
      // pmr
      O1 o1 = new O1(6032);
      Class c1 = (Class)o1.getClass();
      subTest022(c1);
   }
   private void subTest022(Object o) {
      // pmr
      boolean b = true;
      if (o != null) {
         // instanceof 
         //    aload o  <java/lang/Object, non-null>                    VPClass (maybe classObject)
         //    loadaddr <fixed java/lang/Class, non-null, classObject>  VPClass
         // test case2 Object typeIntersect
         if (o instanceof Class) {
            if (o == Class.class)
               b = false;
         }
      }
      logger.debug("Running test22 ... ");
      AssertJUnit.assertTrue(b);
   }

   @Test
   public void test018() {
      // pmr 
      boolean b = true;
      Object o1 = new O1(3141);
      // instanceof
      //    aload o  <fixed O1, non-null>                               VPClass
      //    loadaddr <fixed java/lang/Class, non-null, classObject>     VPClass
      // test case1 typeIntersect - should fail the conditionals and go directly to classTypesCompatible
      // (which in turn will return null thereby failing the intersection)
      if (o1 instanceof Class) {
            // class types are incompatible, typeintersect fails
            // instanceof gets folded
         Class [] c = ((Class)o1).getClasses();
         b = false;
      }
      logger.debug("Running test18 ... ");
      AssertJUnit.assertTrue(b);
   }

   @Test
   public void test019() {
      // pmr
      boolean b = true;
      O1 o1 = new O1(980);
      Class c = o1.getClass();
      // instanceof
      //    aload c  <fixed O1, non-null, classObject>                  VPClass
      //    loadaddr <fixed java/lang/Class, non-null, classObject>     VPClass
      // test case2 typeIntersect
      if (c instanceof Class) {
            // class types are compatible, but should not propagate
            // the java/lang/Class to aload c
         if (c == Class.class)
            b = false;
      }
      logger.debug("Running test19 ... ");
      AssertJUnit.assertTrue(b);
   }
   
   @Test
   public void test020() {
      Object o = new Object();
      subTest020(o);
   }
   private void subTest020(Object o) {
      // pmr
      boolean b = true;
      Class c = o.getClass();
      if (c instanceof Class) {
         if (c == Class.class)
            b = false;
      }
      logger.debug("Running test20 ... ");
      AssertJUnit.assertTrue(b);
   }

   @Test
   public void test021() {
      Object o = new Object();
      Class c = o.getClass();
      subTest021(o, c);
   }
   private void subTest021(Object o1, Object o2) {
      // pmr
      boolean b = true;
      Class c1 = o1.getClass();
      Class c2 = (Class)o2;
      if (c2 != null) {
         // ifacmpeq
         //     aload c1 <O1, classObject>      VPClass
         //     aload c2 <Class, non-null>      VPClass (but no classobject property set)
         // test case3 typeIntersect
         if (c1 == c2) {
            if (c1 == Class.class)
               b = false;
         }
      }
      logger.debug("Running test21 ... ");
      AssertJUnit.assertTrue(b);
   }


   @Test
   public void test023() {
      Object o = new Object();
      Class c = o.getClass();
      subTest023(c, c);
   }
   private void subTest023(Object o1, Object o2) {
      // pmr
      boolean b = true;
      Class c2 = (Class)o2;
      if (c2 != null) {
         // ifacmpeq
         //     aload o1 <java/lang/Object>             VPClassType
         //     aload c2 <java/lang/Class, non-null>    VPClass (but no classobject property set)
         // test case5 typeIntersect
         if (o1 == c2) {
            if (o1 == Class.class)
               b = false;
         }
      }
      logger.debug("Running test23 ... ");
      AssertJUnit.assertTrue(b);
   }   

   // mirror image of test023
   @Test
   public void test024() {
      Object o = new Object();
      Class c = o.getClass();
      subTest024(c, c);
   }
   private void subTest024(Object o1, Object o2) {
      // pmr
      boolean b = true;
      Class c2 = (Class)o2;
      if (c2 != null && o1 != null) {
         // ifacmpeq
         //     aload o1 <java/lang/Object, non-null>   VPClass
         //     aload c2 <java/lang/Class, non-null>    VPClass (but no classobject property set)
         // test case3 Object in typeIntersect
         if (o1 == c2) {
            if (o1 == Class.class)
               b = false;
         }
      }
      logger.debug("Running test24 ... ");
      AssertJUnit.assertTrue(b);
   }   
   
   @Test
   public void test025() {
      // defect 137273
      // there are 2 problems here:
      // a) instanceof is not optimized away when it should be
      // b) classObject property is lost at merge points as explained below
      //
      //run this test with the following options:
      // "-Xjit:count=1,optlevel=warm,limit={*test025*}(log=tlog,traceglobalvp,tracefull),disableasynccompilation,disabledynamiclooptransfer,disablelocalreordering,disableinlining,disablelocalvp" c137273
      // 
      boolean b = false;
      try {
      Object c = Class.forName("java.lang.String");
      // aload c <classObject> global constraint
      if (c != null) {
         if (c instanceof Integer) {
            int v = ((Integer)c).intValue();
            if (v > 0)    // to --> subTest004 path constraints:
               b = true;  // aload c <classObject, non-null, Integer>
         } else {
            logger.debug("Running test25 ... ");
            return;
         }
      }
      // merge constraints
      // <null>
      // <classObject, non-null, Integer>
      // the bug was this merge returns
      // <Integer> only [when it should not lose the classObject property]
      // 
      subTest025(c); // now constraints for c are:
                     // local: <Integer>
                     // global: <classObject>
                     // so cannot intersect global & local constraints
      
      }catch(Exception e){}

      logger.debug("Running test25 ... ");
      return;
   }
   private int subTest025(Object o) { if (o != null) return o.hashCode(); else return 0;}

   //test store relationships
   //
   @Test
   public void test026() {
        int loopnum = 100000;
         String[] strPattern = {",A", ",AA,B", ",AAA,BB,C", ",AAAA,BBB,CC,D", ",AAAAA,BBBB,CCC,DD,E"};
         String[] strWk = { "" };

        for (int i01=0; i01 < loopnum; i01++) {
               String title1 = "execute(" + String.format("%1$06d", i01) + ")";
               String str = (title1 + strPattern[i01%5]);

               strWk = subTest026(str, ",");
               if (strWk[strWk.length - 1].length() != 1) {
                     AssertJUnit.assertTrue(false); // Failure
                     for (int i = 0; i < strWk.length; i++) {
                           logger.error(strWk[i]);
                     }
                     return;
               }
            }
        //all passed
        AssertJUnit.assertTrue(true);
   }

   private String[] subTest026(String str, String endChar) {
        int len = str.length();
        int index = 0;
        int begin = 0, count = 0;
        while ((index = str.indexOf(endChar, index)) >= 0) {
            index++;
            count++;
        }
        String[] result = new String[count + 1];
        if (count == 0) {
            result[0] = str;
            return result;
        }
        count = 0;
        while ((index = str.indexOf(endChar, index)) >= 0) {
            result[count++] = str.substring(begin, index);
            begin = index + 1;
            index++;
        }
        index++;
        result[count] = str.substring(begin, len); // incorrectly propagate begin as '0'
                                                   // because we ignore the def inside the loop
        return result;
   }

   @Test
   public void test027() {
      for (int i = 0; i < 1000; i++)
         subTest027(i);
      subTest027(1000);
      if (array027[5] != 1000)
         AssertJUnit.assertTrue(false); //failed
      else
         AssertJUnit.assertTrue(true);
   }

   private void subTest027(int x) {
      int i = 0; // first vn

      // need a while-loop
      while (i < 3) { // first vn
         foo(array027, i);
         i = i+1;    // second vn
      }
      /*
      do {
         foo(a, i);
         i = i+1;
      } while (i < 3);
      */

      // first use of i outside loop
      //
      array027[i+1] = x*3; // i+1 same vn as increment in loop ;
                    // incompatible with store constraint
      if (x < 5000) {
         foo(array027, i);
      }
      i = i+2;
      array027[i] = x; // merge point
   }

   private void foo(int [] a, int k) {
      // do something with this array
      //
      for (int i = 1; i < a.length; i++)
         a[i] = a[i-1];
   }


   @Test
   public void test028() {
      initialize028(); 
      for (int i = 0; i < 1000; i++)
         subTest028();
   }

   private void initialize028() {
      for (int i = 0; i < 100; i++) {
         if ((i%2) == 0)
            map028.add(new A());
         else if ((i%3) == 0)
            map028.add(new B());
         else if ((i%7) == 0)
            map028.add(new C());
      }
   }

   private void subTest028() {
      Iterator it = elements();
      A objects[] = new A[100];
      A obj;
      int count = 0;
      while (it.hasNext()) {
         obj = (A) it.next();
         if (obj.getId() == id028) {
            objects[count++] = obj; // single def; 
                                    // but can be multiple defs created by the compiler via vguard tail duplication
                                    // type is definitely A
         }
      }
   }
 
   private Iterator elements() {
      return map028.listIterator();
   }

   @Test
   public void test029() {
      for (int i = 0; i < 300000; i++) {
         subTest0129(N/2, false);
         subTest0229(N/2, false);
         subTest0329(N/2, 1, false);
      }
      logger.debug("result " + subTest0129(N, true));
      logger.debug("result " + subTest0229(N, true));
      logger.debug("result " + subTest0329(N, 0, true));
   }

   private int subTest0129(int n, boolean printIt) {
      int i = 0;
      int j = 0;
      int q = -1;
      int val = 0;
      for (; i < n; i++) {
         for (; j < i; j++) {
            array029[i] = i+j;
            if (j > n/2) break;
            q = m2(i*5);
            if (q != -1) break;
         }
         val = q;
      }
      if (printIt)
    	 logger.debug("val is " + val);
      return i+q;
   }

   private int m2(int n) {
      return (n/2 * 6023);
   }

   private boolean m3(int n) {
      return ((n > 0) ? true:false);
   }

   private int m4(int q) {
      return ((q == 0) ? 0:-1);
   }
   private int subTest0229(int n, boolean printIt) {
      int x = 0;
      for (int i = 2; i < n; i++) {
         x = m2(i);
         if (x == 0)
            break;
      }
      if (printIt) {
         if (x == 0)
            logger.debug("test failed with x= " + x);
         else
            logger.debug("test passed with x= " + x);
      }
      return x;
   }
   private int subTest0329(int n, int q, boolean printIt) {
      int x = -1;
      int val = m2(n);
      for (int j = 0; j < 5; j++) {
         x = -1;
         for (int i = 0; i < n; i++) {
            array029[i] = val;
            if (m3(i)) break;
            x = m4(q);
            if (x != -1) {
               i = n;
            } else {
               val = m2(n);
            }
         }
         val += x;
         if (printIt)
            logger.debug("value of x : " + x);
      }
      return x;
   }
   
   @Test
   public void test030() {
      for (int i = 0; i < 10000; i++)
         subTest030(10, 20, true, bArr);
      boolean result = subTest030(20, 10, false, aArr);
   }

   private void initialize030() {
      for (int i = 0; i < SZ; i++) {
         aArr[i] = (byte)i;
         bArr[i] = (byte)i;
      }
   }

   private boolean subTest030(int p1, int p2, boolean flag, byte [] byteArr) {
      boolean result = false;
      int t = (p1 + p2) + (-1);
      if (flag != false) {
         if (t >= (aArr.length + bArr.length)) {
            if (byteArr[p1] == 60) {
               if (byteArr[p1+1] == 109) {
                  if (byteArr[p1+2] == 99) {
                     if (byteArr[p1+3] == 100) {
                        if (byteArr[p1+4] == 62) {
                           if (byteArr[p1+5] == 60) {
                              if (byteArr[p1+6] == 77) {
                                 if (byteArr[p1+7] == 115) {
                                    if (byteArr[p1+8] == 100) {
                                       if (byteArr[p1+9] == 62) {
                                          if (byteArr[p1+10] == 106) {
                                             if (byteArr[p1+11] == 106) {
                                                if (byteArr[p1+12] == 106) {
                                                   if (byteArr[p1+13] == 106) {
                                                      if (byteArr[t+11] == 60) {
                                                         if (byteArr[t+10] == 47) {
                                                            if (byteArr[t+9] == 77) {
                                                               if (byteArr[t+8] == 115) {
                                                                  if (byteArr[t+7] == 100) {
                                                                     if (byteArr[t+6] == 62) {
                                                                        if (byteArr[t+5] == 99) {
                                                                           if (byteArr[t+4] == 47) {
                                                                              if (byteArr[t+3] == 109) {
                                                                                 if (byteArr[t+2] == 99) {
                                                                                    if (byteArr[t+1] == 100) {
                                                                                       if (byteArr[p1+14] == 110) {
                                                                                          if (byteArr[p1+15] == 111) {
                                                                                             if (byteArr[p1+16] == 110) {
                                                                                                if (byteArr[p1+17] == 101) {
                                                                                                   if (t != p1+Y.length()+(-1))
                                                                                                      result = true;
                                                                                                   else 
                                                                                                      result = false;
                                                                                                }
                                                                                             }
                                                                                          }
                                                                                       } 
                                                                                    }
                                                                                 }
                                                                              }
                                                                           }
                                                                        }
                                                                     }
                                                                  }
                                                               }
                                                            }
                                                         }
                                                      }
                                                   }
                                                }
                                             }
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
      return result;
   }


   public static Random rand028 = new Random();
   public static int id028 = (new Random()).nextInt();
   public static Vector<A> map028 = new Vector<A>();
   public static int LEN = 100;
   public static int [] array027 = new int[LEN];
   public static final int SZ = 256;
   public static byte [] aArr = new byte[SZ];
   public static byte [] bArr = new byte[SZ];
   public static String Y = new String("gutentag");

   public static int N = 1000;
   public static int [] array029 = new int[N];

   
   public static int stl = 0;
   static boolean firstTimeL3 = true;
   static boolean firstTimeL2 = true;
   private static long[] lookup = { 
         Long.MAX_VALUE, Long.MAX_VALUE, Long.MAX_VALUE, 
         Long.MAX_VALUE, Long.MAX_VALUE 
   };
   private static final int NUMBER_OF_CRON_FIELDS = 5;
   private static int[] lookupMax = {-1, -1, -1, -1, -1};
   private static int[] lookupMin = {
         Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE,
         Integer.MAX_VALUE, Integer.MAX_VALUE
   };
   private static final int DAY_OF_MONTH = 2;
   private static final int MONTH = 3;
   private static final int DAY_OF_WEEK = 4;   
}

// some oddball class hierarchy
// IC------          IC1
// |  \    \
// O1  O3   O5
// |    \
// O2    O4
// 
interface IC {
   public int field = 98;
   public void m1();
}

class O1 implements IC {
   public O1() {
   }
   public O1(int i) {
      f2 = i;
      o = new O2();
   }
   public void m1() {
      System.out.println("in O1.m1");
   }
   public Class m2() {
      return getClass();
   }
   public Class m2(O1 o1) {
      return null;
   }
   public void m3() 
      throws Exception {
      throw new Exception();
   }
   public O1 m4(boolean b) {
      return null;
   }
   public Object[] m5(boolean b) {
      return null;
   }
   public int f2;
   public O2 o;
   public static final String ocstring = "j9jitkicksbutt";
}

class O2 extends O1 {
   public O2() {
      b = true;
   }
   public O2(boolean b) {
      this.b = b;
   }
   public void m1() {
      System.out.println("in O2.m1");
   }
   public Class m2(O1 o1) {
      return o1.o.getClass().getSuperclass();
   }
   public O1 m4(boolean b) {
      if (b == this.b)
         return null;
      else
         return new O2(false);
   }
   public Object[] m5(boolean b) {
      if (b == this.b)
         return null;
      else
         return new O2[5];
   }
   private boolean b;
}

class O3 implements IC {
   public O3(int i) {
      f2 = i;
   }
   public void m1() {
      System.out.println("in O3.m1");
   }
   public Object m2(boolean b) {
      return null;
   }
   public int f2;
}

final class O4 extends O3 {
   public O4() { 
      super(67);
   }
   public Object m2(boolean b) {
      if (!b)
         return new O4();
      else
         return null;
   }
}

final class O5 implements IC {
   public O5() {}
   public void m1() {
      System.out.println("in O5.m1");
   }
}

class A {
   int id;
   A() {
      Random r = new Random();
      id = r.nextInt();
   }
   public int getId() { return id;}
}
class B extends A {
   int id_two;
   B() {
      super();
      id_two = id << 1;
   }
   public int getId() {return id_two;}
}
class C extends B {
   int id_three;
   C() {
      super();
      id_three = id_two << 1;
   }
   public int getId() {return id_three;}
}
