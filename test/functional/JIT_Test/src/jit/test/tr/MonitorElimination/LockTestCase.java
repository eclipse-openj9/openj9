/*******************************************************************************
 * Copyright (c) 2003, 2019 IBM Corp. and others
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
package jit.test.tr.MonitorElimination;

import junit.framework.TestCase;
import junit.framework.Test;
import junit.framework.TestSuite;
import java.io.*;
import java.lang.reflect.*;

class UnresolvedClass {
  static public boolean _test = false;
  public static void doTest() { _test = true; }
}

class LockThread extends Thread
   {
   public final static int _TEST_MIXED            =-1;
   public final static int _TEST_SIMPLESEQ        =0;
   public final static int _TEST_COMPLEXSEQ       =1;
   public final static int _TEST_SIMPLEIF         =2;
   public final static int _TEST_COMPLEXIF        =3;
   public final static int _TEST_SWITCH           =4;
   public final static int _TEST_DEADLOCK         =5;
   public final static int _TEST_TRYSIMPLESEQ     =6;
   public final static int _TEST_TRYCOMPLEXSEQ    =7;
   public final static int _TEST_TRYSIMPLEIF      =8;
   public final static int _TEST_TRYSWITCH        =9;
   public final static int _TEST_UNBALANCED       =10;
   public final static int _TEST_NULLPATHS        =11;

   public final static int _NUMTESTS = _TEST_NULLPATHS+1;

   final int _threadID;
   LockTestDriver _driverObject;
   final Object _anotherObject;
   int _timesToIncrement;
   int _test;
   int _delay;
   volatile int _vol;
   int _nonVol;

   public LockThread(int threadID, int timesToIncrement, LockTestDriver driver)
      {
      _threadID = threadID;
      _anotherObject = new Object();
      _timesToIncrement = timesToIncrement;
      _driverObject = driver;
      _delay = 0;
      _nonVol = 0;
      }

   public void setTest(int test)
      {
      _test = test;
      }

   public void run()
      {
      int sum=0;
      for (int i=0;i < _timesToIncrement;i++)
         {
         try
            {
            int testToDo = _test;
            if (testToDo == _TEST_MIXED)
               testToDo = i % _NUMTESTS;

            switch (testToDo)
               {
               case _TEST_SIMPLESEQ     : sum+=simpleSequential(_driverObject); break;
               case _TEST_COMPLEXSEQ    : sum+=complexSequential(_driverObject, false); break;
               case _TEST_SIMPLEIF      : sum+=simpleIf(_driverObject); break;
               case _TEST_COMPLEXIF     : sum+=complexIf(_driverObject,(i%2)==0,(i%4)==0,(i%8)==0,(i%16)==0); break;
               case _TEST_SWITCH        : sum+=testSwitch(_driverObject,i); break;
               case _TEST_DEADLOCK      : sum+=deadlock(_driverObject); break;
               case _TEST_TRYSIMPLESEQ  : sum+=trySimpleSequential(_driverObject, 0); break;
               case _TEST_TRYCOMPLEXSEQ : sum+=tryComplexSequential(_driverObject, false); break;
               case _TEST_TRYSIMPLEIF   : sum+=tryIf(_driverObject); break;
               case _TEST_TRYSWITCH     : sum+=trySwitch(_driverObject,i); break;
               case _TEST_UNBALANCED    : sum+=unbalanced(_driverObject); break;
               case _TEST_NULLPATHS     : sum+=nullPaths(((i%2)==1)?_driverObject:null); break;
               }

               //System.out.println("Thread " + _threadID + ": " + _driverObject.getCounter());
            }
         catch(Exception e)
            {
            }
         }
      _driverObject.undo(sum);
      //_driverObject.print(_threadID);
      }

   // all of the following methods should return num times increment was called
   private int simpleSequential(LockTestDriver to)
      {
      to.increment();   // +1
      to.increment();   // +2
      to.increment();   // +3
      to.increment();   // +4
      to.increment();   // +5
      return 5;
      }

   private synchronized void incrementNonVol()
      {
      _nonVol++;
      }

   public void foo_virtual()
      {
      if (_nonVol < 0)
         _nonVol = 0;
      if (_delay < 0)
         _delay = 0;
      if (_timesToIncrement < 0)
         _timesToIncrement = 0;
      if (_anotherObject == _driverObject)
         {
         _timesToIncrement = -1;
         _delay = -1;
         _nonVol = -1;
         }
      _nonVol++;
      }

   final public String foo_nonvirtual()
      {
      return "hi";
      }

   int complexSequential(LockTestDriver to, boolean falseCondition)
      {
      to.increment();    // +1

      // virtual call will block on virtual path
      foo_virtual();

      to.increment();    // +2

      if (falseCondition)
         falseCondition = false;

      // virtual call will block on virtual path
      foo_nonvirtual();

      to.increment();    // +3

      // volatile access should prevent coarsening
      _vol++;

      to.increment();    // +4
 
      // unresolved access because condition should always be false
      if (falseCondition)
         System.out.println("UnresolvedClass._test is " + UnresolvedClass._test);
      to.increment();    // +5

      // synchronization on another object should prevent coarsening
      incrementNonVol();
 
      to.increment();    // +6

      // different type of synchronization
      synchronized(this) { _nonVol++; }

      to.increment();    // +7

      return 7;
      }


   private int simpleIf(LockTestDriver to)
      {
      int sum=0;

      sum++; to.increment();
      if (_nonVol > 50)
         {
         sum++; to.increment(); // can coarsen with outer region
         }

      incrementNonVol();

      if (_nonVol > 45)
         {
         sum++; to.increment();
         }
      sum++; to.increment();  // can coarsen with inner region

      incrementNonVol();


      sum++; to.increment();
      if (_nonVol < 0)
         System.out.println("this can't happen");
      else
         _nonVol++;
      sum++; to.increment();  // can coarsen with region above if

      incrementNonVol();

      sum++; to.increment();
      if (_nonVol < 0)
         {
         sum++; to.increment(); // can coarsen up and down
         }
      else
         {
         sum++; to.increment(); // can coarsen up and down
         }
      sum++; to.increment();  // can coarsen with region above if

      return sum;
      }

   private int complexIf(LockTestDriver to, boolean c1, boolean c2, boolean c3, boolean c4)
      {
      int sum=0;

      if (c1)
         {
         if (c2)
            {
            sum++; to.increment();
            if (c3)
               {
               if (c4)
                  {
                  }
               else
                  {
                  }
               }
            else
               {
               if (c4)
                  {
                  incrementNonVol();
                  }
               else
                  {
                  }
               }
            }
         else
            {
            if (c3)
               {
               if (c4) 
                  {
                  sum++; to.increment();
                  }
               else 
                  {
                  foo_virtual();
                  }
               }
            else
               {
               sum++; to.increment();
               if (c4)
                  {
                  sum++; to.increment();
                  }
               else
                  {
                  sum++; to.increment();
                  }
               }
            }
         }
      else
         {
         if (c2)
            {
            sum++; to.increment();
            if (c3)
               {
               _vol++;
               if (c4)
                  {
                  }
               else 
                  {
                  }
               }
            else 
               {
               if (c4)
                  {
                  }
               else
                  {
                  }
               }
            }
         else
            {
            if (c3)
               {
               if (c4)
                  {
                  }
               else
                  {
                  sum++; to.increment();
                  }
               }
            else 
               {
               if (c4)
                  {
                  }
               else 
                  {
                  }
               }
            }
         }
      sum++; to.increment();
      return sum;
      }

   // arrangement of synchronized calls should cause the collectPreds and
   // collectSuccs to bounce around a bit
   private int testSwitch(LockTestDriver to, int x)
      {
      int sum=0;

      try
         {
         boolean b=(x < 100);
         switch (x)
            {
            case 1 : sum++; to.increment();
                     if (b)
                        {
                        sum++; to.increment();
                        throw new Exception();
                        }
                     break;

            case 2 : sum++; to.increment();
                     break;

            case 3 : sum++; to.increment();
                     if (!b)
                        {
                        sum++; to.increment();
                        throw new Exception();
                        }
                     break;
            }
         sum++; to.increment();
         }
      catch (Exception e)
         {
         }
      return sum;
      }

   private int deadlock(LockTestDriver to)
      {
      int sum=0;
      if (to._counter % 2 == 0)
         {
         sum++; to.increment();
         }
      else
         incrementNonVol();

      // if both regions are coarsened across this point
      // deadlock could occur

      if ((to._counter - 1) % 2 == 1)
         {
         sum++; to.increment();
         }
      else
         incrementNonVol();
      return sum;
      }

   private int trySimpleSequential(LockTestDriver to, int zero)
      {
      int sum=0;
      try
         {
         sum++; to.increment();
         }
      catch (Exception e)
         { 
         }

      sum++; to.increment();

      try
         {
         int x = 3 / zero;
         sum++; to.increment();   // shouldn't execute
         }
      catch (Exception e)
         {
         sum++; to.increment();
         }

      sum++; to.increment();

      try
         {
         int x = 5 / zero;
         }
      catch (Exception e)
         {
         sum++; to.increment();
         }

      return sum;
      }

   private int tryComplexSequential(LockTestDriver to, boolean falseCondition)
      {
      try
         {
         to.increment();    // +1

         // virtual call will block on virtual path
         foo_virtual();
         }
      catch(Exception e)
         {
         to.increment();    // won't execute
         }
   
      to.increment();       // +2

      // volatile access should prevent coarsening
      _vol++;

      to.increment();       // +3
 
      // unresolved access because condition should always be false
      if (falseCondition)
         System.out.println("UnresolvedClass._test is " + UnresolvedClass._test);
      to.increment();       // +4

      // synchronization on another object should prevent coarsening
      incrementNonVol();
 
      to.increment();       // +5

      // different type of synchronization
      synchronized(this)
         {
         _nonVol++;
         }

      to.increment();       // +6

      return 6;
      }

   private int tryIf(LockTestDriver to)
      {
      int sum=0;
      int result=0;

      try
         {
         result = sum / _nonVol;  // might cause exception

         sum++; to.increment();
         if (_nonVol > 50)
            {
            sum++; to.increment(); // can coarsen with outer region
            }

         incrementNonVol();

         if (_nonVol > 45)
            {
            sum++; to.increment();
            }
         result += sum / _nonVol;  // might cause exception
         }
      catch (Exception e)
         {
         }

      sum++; to.increment();  // can coarsen with inner region

      incrementNonVol();

      sum++; to.increment();
      try
         {
         result = sum / _nonVol;  // might cause exception
         }
      catch (Exception e)
         {
         if (_nonVol < 0)
            System.out.println("this can't happen");
         else
            incrementNonVol();
         sum++; to.increment();  // can coarsen with regions above and below try
         }

      incrementNonVol();

      sum++; to.increment();
      try
         {
         result = sum / _nonVol;  // might cause exception
         if (_nonVol < 0)
            {
            sum++; to.increment(); // can coarsen up and down
            }
         else
            {
            sum++; to.increment(); // can coarsen up and down
            }
         result = sum / _nonVol;  // might cause exception
         }
      catch (Exception e)
         {
         if (_nonVol < 0)
            {
            sum++; to.increment();
            }
         else
            {
            sum++; to.increment();
            }
         sum++; to.increment();  // can coarsen with region above if
         }

      if (result < 0) // keep result live
         return sum;
      else
         return sum;
      }

   private int trySwitch(LockTestDriver to, int x) throws Exception
      {
      int sum=0;
      int result=0;

      try
         {
         boolean b=(x < 100);
         switch (x)
            {
            case 1 : sum++; to.increment();
                     if (b)
                        {
                        sum++; to.increment();
                        throw new Exception();
                        }
                     break;

            case 2 : try
                        {
                        result = sum / _nonVol;
                        sum++; to.increment();
                        }
                     catch (Exception e)
                        {
                        }
                     break;

            case 3 : try
                        {
                        sum++; to.increment();
                        result = sum / _nonVol;
                        }
                     catch (Exception e)
                        {
                        }
                     break;

            case 4 : try
                        {
                        result = sum / _nonVol;
                        }
                     catch (Exception e)
                        {
                        sum++; to.increment();
                        }
                     break;

            case 5 : try
                        {
                        result = sum / _nonVol;
                        try
                           {
                           result = (sum+1) / _nonVol;
                           sum++; to.increment();
                           result = sum / _nonVol;
                           }
                        catch (Exception e)
                           {
                           result = (sum-1) / _nonVol;
                           }
                        }
                     catch (Exception e)
                        {
                        sum++; to.increment();
                        }
                     break;

            case 6 : sum++; to.increment();
                     if (!b)
                        {
                        sum++; to.increment();
                        throw new Exception();
                        }
                     break;
            }
         sum++; to.increment();
         }
      catch (Exception e)
         {
         }

      if (result < 0)
         return sum;
      else
         return sum;
      }

   private int unbalanced(LockTestDriver to)
      {
      int sum=0;
      try
         {
         // this next synchronized method invocation should cause
         //    an IllegalMonitorStateException because it does:
         //    Lock Unlock Unlock
         // BUT: if this call is coarsened with lower synchronized invoke,
         //    then this invocation will only do Lock Unlock and leave the
         //    object unlocked and throw no exception
         to.unbalancedUnlockUnderLock();

         // error if we get here
         sum += 100;

         // this synchronized method invocation does Lock Lock Unlock
         // which, if we coarsened away the first Lock with upper Unlock,
         // will simply lock the object and then unlock it, throwing no
         // exception
         to.unbalancedLockUnderLock();
         }
      catch (Exception e)
         {
         // we SHOULD get here
         }

      return sum;
      }

   private int nullPaths(LockTestDriver to)
      {
      int sum=0;

      try
         {
         if (to != null)
            {
            sum++; to.increment();
            }

         // can be coarsened across this point, but need guarded MONENTER
         //  on else path from upper if, and guarded MONEXIT on else path
         //  from lower if

         if (to != null)
            {
            sum++; to.increment();
            }
         }
      catch (Exception e)
         {
         sum = 100;
         }

      return sum;
      }
   }


class LockTestDriver extends Thread
   {
   final static int INITIALVALUE=0;

   int _numThreads;
   LockThread[] _threads;
   int _counter;

   public LockTestDriver(int numThreads, int timesToIncrement)
      {
      _numThreads = numThreads;
      _threads = new LockThread[numThreads];
      for (int t=0;t < numThreads;t++)
         {
         _threads[t] = new LockThread(t, timesToIncrement, this);
         }
      }

   public final synchronized int getCounter()
      {
      return _counter;
      }

   public final synchronized void increment()
      {
      _counter++;
      }

   public final synchronized void unbalancedLockUnderLock()
      {
      Unbalanced.unbalancedMonenter(this);
      }

   public final synchronized void unbalancedUnlockUnderLock()
      {
      Unbalanced.unbalancedMonexit(this);
      }

   public synchronized void undo(int amount)
      {
      _counter -= amount;
      //System.out.println("Undid " + amount + " to " + _counter);
      }

   // this method CANNOT be synchronized or deadlocks will cause the whole
   // framework to hang
   public final boolean notZero()
      {
      return (_counter != INITIALVALUE);
      }

   public void print(int threadID)
      {
      System.out.println("Thread " + threadID + " finished with counter=" + _counter);
      }

   public final boolean go(int test)
      {
      final int TIMEOUTSECONDS=60;

      _counter = INITIALVALUE;

      for (int t=0;t < _numThreads;t++)
         {
         _threads[t].setTest(test);
         _threads[t].start();
         }
     
      // make sure we go to sleep and let other thread start changing counter
      try { sleep(1000); } catch (InterruptedException e) {};

      // wait for all the threads to finish, but don't wait forever
      int timeoutCounter=1;
      while (notZero() && timeoutCounter < TIMEOUTSECONDS)
         {
         try
            {
            sleep(1000);
            }
         catch (InterruptedException e)
            {
            }
         //System.out.println("timed out waiting " + _counter);
         timeoutCounter++;
         }

      return (timeoutCounter < TIMEOUTSECONDS);
      }
   }

public class LockTestCase extends ThreadMXBeanTestCase
   {
   final static int NUMTHREADS=1000;
   final static int TIMESTOINCREMENT=10000;

   public LockTestCase(String name)
      {
      super(name);
      }

   protected void setUp() {}

   public void runSimpleSeq()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_SIMPLESEQ));
      }
   public void runComplexSeq()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_COMPLEXSEQ));
      }
   public void runSimpleIf()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_SIMPLEIF));
      }
   public void runComplexIf()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_COMPLEXIF));
      }
   public void runSwitch()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_SWITCH));
      }
   public void runDeadlock()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_DEADLOCK));
      }

   public void runTrySimpleSeq()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_TRYSIMPLESEQ));
      }
   public void runTryComplexSeq()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_TRYCOMPLEXSEQ));
      }
   public void runTryIf()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_TRYSIMPLEIF));
      }
   public void runTrySwitch()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_TRYSWITCH));
      }

   public void runNullPaths()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_NULLPATHS));
      }
   public void runUnbalanced()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_UNBALANCED));
      }

   public void runMixed()
      {
      assertTrue(new LockTestDriver(NUMTHREADS, TIMESTOINCREMENT).go(LockThread._TEST_MIXED));
      }

   public static Test suite()
      {
      TestSuite suite = new TestSuite();

      suite.addTest(new LockTestCase("runSimpleSeq"));
      suite.addTest(new LockTestCase("runComplexSeq"));
      suite.addTest(new LockTestCase("runSimpleIf"));
      suite.addTest(new LockTestCase("runComplexIf"));
      suite.addTest(new LockTestCase("runSwitch"));
      suite.addTest(new LockTestCase("runDeadlock"));

      suite.addTest(new LockTestCase("runTrySimpleSeq"));
      suite.addTest(new LockTestCase("runTryComplexSeq"));
      suite.addTest(new LockTestCase("runTryIf"));
      suite.addTest(new LockTestCase("runTrySwitch"));

      suite.addTest(new LockTestCase("runNullPaths"));
      suite.addTest(new LockTestCase("runUnbalanced"));

      suite.addTest(new LockTestCase("runMixed"));

      return suite;
      }


   public static void main(String[] args)
      {
      junit.textui.TestRunner.run(suite());
      }
   }
