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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.CallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MutableCallSite;
import java.lang.invoke.SwitchPoint;
import java.lang.invoke.VolatileCallSite;

/**
 * This test case class contains all multi-threaded tests for VolatileCallSite, MutableCallSite and SwitchPoint 
 * APIs. These tests are required for validating implementation behavior involving concurrent update to 
 * Volatile Call Sites and global invalidation of Switch Points. However, these tests have been separated 
 * out from the specific test case classes of these 3 APIs as, with their current approach, they pose potential 
 * "timeout" based failure scenarios which will not necessarily indicate issues in the API implementation. 
 * These tests will not be included in the automated JSR 292 testing bucket until more determinism is 
 * introduced.
 */
public class JSR292_MultiThreadedTests {
	/**
	 * The test creates a MutableCallSite object, uses 1 separate Worker thread to change the target of the MutableCallSite,
	 * makes a call to MutableCallSite.syncAll() and validates that the change to the target by the worker thread is visible to the "main" thread.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testSyncAll() throws Throwable {
		MethodHandle mh = null;
		MutableCallSite mutableCS = null;

		mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "returnOne", MethodType.methodType(String.class) );
		mutableCS = new MutableCallSite( mh );

		String s = (String) mutableCS.dynamicInvoker().invokeExact();

		AssertJUnit.assertEquals( "1", s );

		Thread workerThread = new WorkerThread_CS( mutableCS );

		workerThread.start();

		/* Sleep so that the worker thread has an opportunity to run */
		for ( int i = 0; i < 4; i++ ) {
			Thread.sleep( 1000 );
			
			if ( workerThread.isAlive() == false ) {
				break;
			}
		}
		
		/*Call to MutableCallSite.syncAll(). We expect update made by worker thread to be visible to the "main" thread now*/
		MutableCallSite.syncAll( new MutableCallSite[]{mutableCS} );

		s = (String) mutableCS.dynamicInvoker().invokeExact();

		boolean updatePerformed = false;

		if ( !s.equals( "2" ) ) {
			System.out.println("WARNING: testSyncAll: MutableCallSite update not visible after 4 seconds");
			System.out.println("WARNING: testSyncAll: joining worker thread");
				
			workerThread.join();
			
			/*Call to MutableCallSite.syncAll(). We expect update made by worker thread to be visible to the "main" thread now*/
			MutableCallSite.syncAll( new MutableCallSite[]{mutableCS} );
			
			s = (String) mutableCS.dynamicInvoker().invokeExact();
			
			if ( s.equals( "2" ) ) {
				updatePerformed = true;
			} else {
				System.out.println("ERROR: testSyncAll: MutableCallSite update made by worker thread not visible in main thread after syncAll");
			}
		} else {
			updatePerformed = true;
		}

		AssertJUnit.assertTrue( updatePerformed );
	}
	
	/**
	 * The test creates a VolatileCallSite, uses 1 separate worker thread to change the target of a volatile call site and 
	 * validates that the update is visible to the 'main' thread.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testVolatility() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "returnOne", MethodType.methodType(String.class) );
		VolatileCallSite volatileCS = new VolatileCallSite( mh );

		String s = (String)volatileCS.dynamicInvoker().invokeExact();
		AssertJUnit.assertEquals( "1", s );

		Thread workerThread = new WorkerThread_CS( volatileCS );
		workerThread.start();

		boolean updatePerformed = false;

		/* Sleep so that the worker thread has an opportunity to run */
		for ( int i = 0; i < 4; i++ ) {
			Thread.sleep( 1000 );
			s = (String) volatileCS.dynamicInvoker().invokeExact();

			if ( s.equals( "2" ) ) {
				updatePerformed = true;
				break;
			}
		}
		
		if ( !updatePerformed ) {
			System.out.println("WARNING: testVolatility: VolatileCallSite not updated after waiting 4 seconds");
			System.out.println("WARNING: testVolatility: joining worker thread");
			
			workerThread.join();
			
			s = (String) volatileCS.dynamicInvoker().invokeExact();
			
			if ( s.equals( "2" ) ) {
				updatePerformed = true;
			} else {
				System.out.println("ERROR: testVolatility: VolatileCallSite update made by worker thread not visible in main thread");
			}
		}
		
		AssertJUnit.assertTrue( updatePerformed );
	}
	
	/**
	 * Tests visibility of a MutableCallSite update made in the main thread in a separate worker thread after syncAll is performed.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testSyncAll_WithoutJoin() throws Throwable {
		MethodHandle K_false = MethodHandles.constant( boolean.class, false );
		MethodHandle K_true = MethodHandles.constant( boolean.class, true );
		
		//Run the following code twice so that we provide hints to the JIT to compile the target method.
		for ( int i = 0 ; i < 2 ; i++ ) {
			MutableCallSite mutableCS = new MutableCallSite( K_false ); 
			
			WorkerThread2_CS workerThread = new WorkerThread2_CS( mutableCS );
			workerThread.start();
			
			while (!workerThread.isStarted) {
				Thread.sleep(10);
			}
			
			mutableCS.setTarget( K_true );
			MutableCallSite.syncAll( new MutableCallSite[]{mutableCS} );
			
			synchronized (workerThread) {
				if (!workerThread.volatileUpdateSeen()) {
					workerThread.wait(30000);
				}
			}
			if (!workerThread.volatileUpdateSeen()) {
				Assert.fail("ERROR: testSyncAll_WithoutJoin: volatile update made by main thread not visible in worker thread");
			}
		}
	}

	/**
	 * Tests visibility of a VolatileCallSite update made in main thread in a separate worker thread
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testVolatility_WithoutJoin() throws Throwable {
		MethodHandle K_false = MethodHandles.constant( boolean.class, false );
		MethodHandle K_true = MethodHandles.constant( boolean.class, true );
		
		//Run the following code twice so that we provide hints to the JIT to compile the target method.
		for ( int i = 0 ; i < 2 ; i++ ) {
			VolatileCallSite volatileCS = new VolatileCallSite( K_false ); 
			
			WorkerThread2_CS workerThread = new WorkerThread2_CS( volatileCS );
			workerThread.start();
			
			while (!workerThread.isStarted) {
				Thread.sleep(10);
			}
			
			volatileCS.setTarget( K_true );
			
			//The test fails if worker thread does not see the target update within 4 seconds
			synchronized (workerThread) {
				if (!workerThread.volatileUpdateSeen()) {
					workerThread.wait(30000);
				}	
			}
			if (!workerThread.volatileUpdateSeen()) {
				Assert.fail("ERROR: testVolatility_WithoutJoin: volatile update made by main thread not visible in worker thread");
			}
		}
	}
	
	/**
	 * Worker thread to aid in MutableCallSite and VolatileCallSite tests where join is used
	 */
	private static class WorkerThread_CS extends Thread {
		private CallSite cs;
		private MethodHandle mh;
		
		public WorkerThread_CS ( CallSite cs ) throws Throwable {
			mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "returnTwo", MethodType.methodType(String.class) );
			this.cs = cs;
		}
		
		public void run() {
			try {
				cs.setTarget( mh );
			} catch( Exception e ) {
				e.printStackTrace();
			}
		}
	}
	
	/**
	 * Worker thread to aid in MutableCallSite and VolatileCallSite tests where join is not used
	 */
	private static class WorkerThread2_CS extends Thread {
		private CallSite cs;
		private volatile boolean volatileUpdateSeen = false ; 
		public volatile boolean isStarted = false;
		
		public WorkerThread2_CS ( CallSite cs ) {  
			this.cs = cs;
		}
		
		public void run() {
			isStarted = true;
			while( true ) {
				for ( int i = 0 ; i < 1000 ; i++ ) {
					for ( int j = 0 ; j < 1000 ; j++ ) {
						for ( int k = 0 ; k < 1000 ; k++ ) {
							try {
								if ( (boolean) cs.dynamicInvoker().invoke() ) {
									synchronized (this) {
										volatileUpdateSeen = true ;
										notifyAll();
									}
									return;
								}
							} catch ( Throwable e ) {
								e.printStackTrace();
							}
						}
					}
				}
			}
		}
		
		public boolean volatileUpdateSeen () {
			return volatileUpdateSeen; 
		}
	}
	
	/**
	 * Tests SwitchPoint invalidation using 2 threads 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testSwitchPoint_Invalidation_Global() throws Throwable {
		SwitchPoint sp = new SwitchPoint();
		AssertJUnit.assertTrue(!sp.hasBeenInvalidated());

		WorkerThread_SP workerThread = new WorkerThread_SP( sp );
		workerThread.start();

		boolean switchPointInvalidated = false;

		/* Sleep so that the worker thread has an opportunity to run */
		for ( int i=0 ; i < 2 ; i++ ) {
			Thread.sleep( 2000 );
			switchPointInvalidated = sp.hasBeenInvalidated();
			if ( switchPointInvalidated ) {
				break;
			}
		}

		if ( !switchPointInvalidated ) {
			System.out.println("WARNING: testSwitchPoint_Invalidation_Global: SwitchPoint not invalidated after waiting 4 seconds");
			System.out.println("WARNING: testSwitchPoint_Invalidation_Global: joining worker thread");
			workerThread.join();
			switchPointInvalidated = sp.hasBeenInvalidated();
		}

		AssertJUnit.assertTrue( switchPointInvalidated );
	}

	/**
	 * Worker thread to aid in SwitchPoint global invalidation test
	 *
	 */
	private static class WorkerThread_SP extends Thread {
		private SwitchPoint sp;
		public WorkerThread_SP( SwitchPoint sp ) {
			this.sp = sp;
		}
		public void run ( ) {
			try {
				SwitchPoint.invalidateAll( new SwitchPoint[]{sp} );
			}catch(Exception e) {
				e.printStackTrace();
			}
		}
	}
}
