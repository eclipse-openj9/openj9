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
package jit.test.tr.liveMonitor;

import org.testng.annotations.Test;

@Test(groups = { "level.sanity","component.jit" })
public class monTests {

   //ctor
   public monTests(String s) {
      initialize();
   }
   public static final int SZ = 10000;
   public static final int LEN = 512;
   private void initialize() {
      numExceptions = 0;
      arr = new int[LEN];
      for (int i = 0; i < LEN; i++)
         arr[i] = i + (56*i+23)/2;
      isCapable = true;
      myCount = 0;
      lock = new Object();
   }

   public int numExceptions;
   public int [] arr;
   public boolean isCapable;
   public int myCount;
   public Object lock;
   public Integer intValue;
   public int quantity;

   @Test
   public void test001() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncMethod();
         } catch (Exception e) {
         }
      }
   }

   @Test
   public void test002() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testStaticSyncMethod(this);
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test003() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncMethodNoUserCatch();
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test004() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncBlock();
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test005() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncMethodInlined();
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test006() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            if ((i % 5) == 0) isCapable = false;
            else isCapable = true;
            testSyncBlockForCoarsening(i);
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test007() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncBlockForCoarseningLocally(aa, bb);
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test008() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSimpleReadMonitorBlock();
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test009() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncBlockDLT();
         } catch (Exception e) {
         }
      }
   }
   @Test
   public void test010() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncBlockAndMethodDLT();
         } catch (Exception e) {
         }
      }
   }
   @Test(enabled = false)
   public void test011() {
      A aa = new A(); B bb = new B();
      for (int i = 0; i < SZ; i++) {
         try {
            testSyncMethodDLT(this);
         } catch (Exception e) {
         }
      }
   }




   //java "-Xjit:{*testSyncMethod*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation" montests
   private synchronized void testSyncMethod() throws Exception {
      try {
         if (arr.length != 5) {
            myCount = arr.length;
            return;
         }
         if (lock != null)
            return;
         Object o = this.clone();
         for (int i = 0; i < 100; i++)
           doWork(i); 
         return;
      } catch (Exception e) {
         registerExceptions();
         return;
      }
   }


   //java "-Xjit:{*testStaticSyncMethod*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation" montests
   private static synchronized Object testStaticSyncMethod(monTests m) throws Exception {
      try {
         if (m.arr.length != 5) {
            m.myCount = m.arr.length;
            return m.lock;
         }
         for (int i = 0; i < 100; i++)
           m.doWork(i); 
         return m.lock;
      } catch (Exception e) {
         m.registerExceptions();
         return m.lock;
      }
   }

  
   
   //java "-Xjit:{*testSyncMethod*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation,dontInline={*testSyncMethod*}" montests
   // no catch-all block when user catch
   private synchronized void testSyncMethodNoUserCatch() throws Exception {
      //try {
         Object o = this.clone();
         for (int i = 0; i < 100; i++)
           doWork(i); 
     // } catch (Exception e) {
     //    registerExceptions();
         //return;
     // }
   }



   //java "-Xjit:{*testSyncBlock*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation" montests  
   private void testSyncBlock() throws Exception {
      try {
         Object o = this.clone();
         synchronized (this) {
         Object o2 = this.clone();
         for (int i = 0; i < 100; i++)
           doWork(i); 
         }
      } catch (Exception e) {
         registerExceptions();
         //return;
      }
   }   

   //java "-Xjit:{*testSyncMethodInlined*}(log=tlog,tracefull,optdetails),dontInline={*testSyncMethodInlined*|*doWork*},disableasynccompilation" montests
   private void testSyncMethodInlined() throws Exception {
      try {
         ///Object o = this.clone();
         ///syncDoTheWork();
         syncBlockDoTheWork();
      } catch (Exception e) {
         registerExceptions();
         //return;
      }
   } 

   // callee contains/not contains a catch
   private synchronized void syncDoTheWork() throws Exception { 
      try {
         int i = getOtherIndex();
         //for (int i = 0; i < 100; i++)
         for (; i < 100; i++)
            doWork(i); 
      } catch (Exception e) {
         registerExceptions();
         throw e;
      }
   }

   // callee contains/not contains a catch
   private void syncBlockDoTheWork() throws Exception { 
      try {
         Object o = this.clone();
         synchronized(o) {
            for (int i = 0; i < 100; i++)
               doWork(i); 
         }
      } catch (Exception e) {
         registerExceptions();
         throw e;
      }
   }




   //java "-Xjit:{*testSyncBlockForCoarsening*}(log=tlog,tracefull,optdetails,traceredundantmonitorelimination),disableasynccompilation,disableColdBlockMarker,disableColdBlockOutlining" montests
   private void testSyncBlockForCoarsening(int i) throws Exception {
      i = getOtherIndex();
      if (i < 200) {
         i = i + getValue(i);
      } else {
         i = i + 5;
      }
      i += myCount + getOtherIndex();
      for (; i < 500 ; i++)
         doWork(i); 
   } 


   //java "-Xjit:{*testSyncBlockForCoarsening*}(log=tlog,tracefull,optdetails,traceredundantmonitorelimination),disableasynccompilation,disableColdBlockMarker,disableColdBlockOutlining" montests
   private int testSyncBlockForCoarseningLocally(A a, B b) throws Exception {
      this.lock = a.lock;
      this.intValue = a.intValue;
      this.quantity = b.getTheValue(); // sync method should be inlined
      int index = this.myCount;
      String s = b.getStrings(index); // sync method should be inlined
      String s2 = b.getTheString(); // another sync method should be inlined
      return s.length()+s2.length();
   }
   
   private void testSyncBlockDLT() throws Exception {
      Object o = this.lock;
      synchronized(o) {
         int i = 0;
         i+= getOtherIndexSync();
      }
   }

   private synchronized void testSyncBlockAndMethodDLT() throws Exception {
      Object o = this.lock;
      synchronized(o) {
         int i = 0;
         i+= getOtherIndexSync();
      }
   }



   private static synchronized void testSyncMethodDLT(monTests m) {
      try {
         synchronized (m) {
            while (true) {
               synchronized (m) {
                  m.doWork(1);
               }
            }
         }
      } catch (Exception e) {
         m.registerExceptions();
         e.printStackTrace();
      }
   }

   private final synchronized int getOtherIndex() {
      ///return myCount+45+getItNow();
      return myCount+45;
   }
   
   private final int getOtherIndexSync() {
      ///return myCount+45+getItNow();
      synchronized(this.lock) {
         return myCount+45;
      }
   }


   private final synchronized int getValue(int k) {
      return myCount+k;
   }

   private int getItNow() {
      return 50;
   }

   // java "-Xjit:{*testSimple*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation" montests
   // no ilgen transformation here because the method below is not synchronized
   //
   private int testSimpleReadMonitorBlock() throws Exception {
      try {
         ///Object o = this.clone();
         synchronized (this) {
            return myCount;
         }
      } catch (Exception e) {
         registerExceptions();
      }
      return 0;
   }


   // java "-Xjit:{*testSimple*}(log=tlog,tracefull,optdetails,disableinlining),disableasynccompilation" montests
   private synchronized int testSimpleReadMonitorMethod() throws Exception {
            return myCount;
   }



   private void doWork(int t) throws Exception {
      // bubble sort
      boolean flag = true;
      for (int i = 0; i < arr.length && flag; i++) {
         flag = false;
         for (int j = 0; j < arr.length-1; j++) {
            if (arr[j+1] < arr[j]) { //swap
               int temp = arr[j+1];
               arr[j+1] = arr[j];
               arr[j] = temp;
               flag = true;
            }
         }
      if (i == arr.length-1) throw new Exception();
      }
   }

   private void registerExceptions() { 
      ++numExceptions;
   }

   private int getInt() { return ++myCount; }

}

class A {
   public A() {
      intValue = new Integer(6023);
      lock = new Object();
   }

   public Integer intValue;
   public Object lock;
}

class B {
   public B() {
      theValue = 31414;
      theString = "idontknowjava";
      arrStrings = new String[LEN];
      for (int i = 0; i < LEN; i++)
         arrStrings[i] = new String(""+i);
   }

   public static final int LEN = 512;
   public int theValue;
   public String theString;
   public String [] arrStrings;
   
   public synchronized int getTheValue() { return theValue; }      
   public synchronized String getTheString() {
      return new String(theString);
   }

   public synchronized String getStrings(int index) {
      return theString; //arrStrings[index];
   }

}
