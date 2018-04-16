/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package jit.test.tr.chtable;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;
import java.util.*;

import jit.test.tr.chtable.util.*;

@Test(groups = { "level.sanity","component.jit" })
public class VirtualGuardTest implements Notifiable {
	private static final int SECONDS_5  =  5 * 1000;
	private static final int SECONDS_6  =  6 * 1000;
	private static final int SECONDS_10 = 10 * 1000;
	
	private static final int TASK_LOAD_B = 1;
	private static final int TASK_FIN    = 2;

	class A {
		public int method() {
			return 'A';
		}
	}
	
	class B extends A {
		public int method() {
			return 'B';
		}
	}

	private volatile A           f;
	private Timer       timer;
	private volatile boolean     keepOnGoing;

	/*
	 * @see TestCase#setUp()
	 */
	@BeforeMethod
	protected void setUp() throws Exception {			
		f = new A();
		timer = new Timer();
	}
	
	@AfterMethod
	protected void tearDown() throws Exception {		
		timer.cancel();
		timer = null;	
	}
	
	private char bar() {
		char state = 'A';
		while (keepOnGoing) {								
			int result = goo();
			switch (result) {
				case 4*'A':
					AssertJUnit.assertTrue(state == 'A');
					break;
				case 4*'B':					
					state = 'B';
					break;
				default:
					AssertJUnit.assertTrue("switching to ?", state == 'A' && result > 4*'A' && result < 4*'B');
					state = '?';
					break;
			}			
		}
			
		return state;	
	}
	
	private int goo() {
		return (f.method() + f.method() + f.method() + f.method());
	}
	
	@Test
	public void testNOPing(){
			
		keepOnGoing = true;	
		
		// load B after 6 seconds			
		timer.schedule(new NotifyTask(TASK_LOAD_B, this), SECONDS_6);
		// schedule to be woken up after 10 seconds
		timer.schedule(new NotifyTask(TASK_FIN, this), SECONDS_10);
		// run bar until woken up
		char result = bar();
		// make sure that class B has been loaded
		AssertJUnit.assertEquals(result, 'B');
	}
	
	//Should not be run as a test, therefore it is disabled
	@Test(enabled = false)
	public void wakeUp(int event) {
		switch (event) {
			case TASK_LOAD_B:
				f = new B(); break;
			case TASK_FIN:
				keepOnGoing = false; break;			
		}
		
	}

}
