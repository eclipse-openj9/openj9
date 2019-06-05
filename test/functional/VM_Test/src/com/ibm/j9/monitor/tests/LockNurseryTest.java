package com.ibm.j9.monitor.tests;

/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.io.IOException;
import java.io.InputStream;
import com.ibm.j9.monitor.tests.utilClasses.*;

public class LockNurseryTest extends TestCase {
	
	public static int NO_LOCKWORD = -1;
	public static long LOCKWORD_NO_OWNERS = 0;
	public static long HEADER_SIZE = TestNatives.getHeaderSizeWrapper();
	
//	public class J9NoLockwordClass {
//
//	}
//	
//	public class J9NoLockwordClassLockwordRemovedX extends Object {
//		public J9NoLockwordClassLockwordRemovedX() {};
//		public void doNothing(){};
//	}
//	
//	public class J9NoLockwordClassLockwordRemovedThenAddedX extends J9NoLockwordClassLockwordRemovedX {
//		public J9NoLockwordClassLockwordRemovedThenAddedX() {};
//		public synchronized void doNothing2() {};
//	}
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args no valid arguments
	 */
	public static void main (String[] args) {
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(LockNurseryTest.class);
	}
	
	public static void generateGarbageAndCollect(){
/*		System.out.println("starting to generate garbage");
		for (int i=0;i<10000000;i++){
			new LockNurseryTest();
		}
		System.out.println("Finished generating garbage, requesting gc");
		System.gc();
		System.gc();
		System.gc();
		System.out.println("GC complete ?");*/
	}
	
	/**
	 * tests the case were we have an object which has a lock word
	 */
	public void testObjectWithLockword(){
		TestNatives natives = new TestNatives();
		Object lockword = new Object();
		assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
		assertTrue("lock offset should be right after header", natives.getLockWordOffset(lockword) == HEADER_SIZE);
		synchronized(lockword){
			System.out.println("holding lock Object which always has lockword");
		}
		generateGarbageAndCollect();
	}
	
	/**
	 * tests the case were we have an object which does not have a lock word
	 */
	public void testObjectWithoutLockword(){
		TestNatives natives = new TestNatives();
		Object lockword = new J9NoLockwordClass();
		assertTrue("lockword should be " + NO_LOCKWORD + ":" + (int)natives.getLockWordOffset(lockword), (int)natives.getLockWordOffset(lockword) == NO_LOCKWORD);
		synchronized(lockword){
			System.out.println("holding lock on object without lockword");
		}
		generateGarbageAndCollect();
	}
	
	
	/**
	 * tests the case were we have an object which parent had lockword but subclass does not 
	 */
	public void testObjectWithoutLockwordLockwordRemoved(){
		TestNatives natives = new TestNatives();
		Object lockword = new J9NoLockwordClassLockwordRemoved();
		assertTrue("lockword should be " + NO_LOCKWORD + ":" + (int)natives.getLockWordOffset(lockword), (int)natives.getLockWordOffset(lockword) == NO_LOCKWORD);
		synchronized(lockword){
			System.out.println("holding lock on object for which we removed the lockword");
		}
		generateGarbageAndCollect();
	}
	
	/**
	 * tests the case were we hold lock on array
	 */
	public void testLockOnArray(){
		TestNatives natives = new TestNatives();
		Object lockword = new Integer[10];
		assertTrue("lockword should be " + NO_LOCKWORD + ":" + (int)natives.getLockWordOffset(lockword), (int)natives.getLockWordOffset(lockword) == NO_LOCKWORD);
		synchronized(lockword){
			System.out.println("holding lock on an array");
		}
		generateGarbageAndCollect();
	}
	
	/**
	 * tests the case were had lockword in object but that was removed and then we added one back
	 */
	public void testObjectWithoutLockwordLockwordRemovedThenAdded(){
		TestNatives natives = new TestNatives();
		Object lockword = new J9NoLockwordClassLockwordRemovedThenAdded();
		assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
		assertTrue("lock offset should be right after header", natives.getLockWordOffset(lockword) == HEADER_SIZE);
		synchronized(lockword){
			System.out.println("holding lock on object for which we removed the lockword, then added back");
		}
		generateGarbageAndCollect();
	}
	
	/**
	 * tests the case the lockword went into backfill at the end of the object
	 */
	public void testLockwordInBackfill(){
		if (TestNatives.getLockwordSize() == 4){
			TestNatives natives = new TestNatives();
			Object lockword = new J9ClassLockwordInBackfillAtEnd();
			System.out.println("lockword position =" + natives.getLockWordOffset(lockword));
			assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
			assertTrue("lock offset should be right after first int", natives.getLockWordOffset(lockword) == HEADER_SIZE + 4);
			synchronized(lockword){
				System.out.println("holding lock on for which backfill was at the end");
			}
			generateGarbageAndCollect();
		} else {
			System.out.println("Not doing backfill at end test as lockword is larger than backfill size");
		}
	}
	
	/**
	 * tests the case the lockword went into backfill in the middle of the object
	 */
	public void testLockwordInBackfillInMiddle(){
		if (TestNatives.getLockwordSize() == 4){
			TestNatives natives = new TestNatives();
			Object lockword = new J9ClassLockwordInBackfillInMiddle();
			System.out.println("lockword position =" + natives.getLockWordOffset(lockword));
			assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
			assertTrue("lock offset should be right after first int", natives.getLockWordOffset(lockword) == HEADER_SIZE + 4);
			synchronized(lockword){
				System.out.println("holding lock on for which backfill was in the middle");
			}
			generateGarbageAndCollect();
		} else {
			System.out.println("Not doing backfill in middle test as lockword is larger than backfill size");
		}
	}
	
	/**
	 * tests the case the lockword in inherited from its superclass
	 */
	public void testLockwordInherited(){
		TestNatives natives = new TestNatives();
		Object lockword = new J9ClassLockwordInBackfillInMiddle();
		Object parentLockword = new J9InheritedLockword();
		System.out.println("lockword position =" + natives.getLockWordOffset(lockword));
		assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
		assertTrue("lock offset should be right after first int", natives.getLockWordOffset(lockword) == natives.getLockWordOffset(parentLockword));
		synchronized(lockword){
			System.out.println("holding lock on for which inherited lockword");
		}
		generateGarbageAndCollect();
	}
	
	
	/**
	 * tests that we can lock on a class
	 */
	public void testClassLockword(){
		TestNatives natives = new TestNatives();
		Object lockword = TestNatives.class;
		// currently we expect no lockword 
		assertTrue("lockword should be 0", natives.getLockWordValue(lockword) == LOCKWORD_NO_OWNERS);
		synchronized(lockword){
			System.out.println("holding lock on class object");
		}
		generateGarbageAndCollect();
	}

}
