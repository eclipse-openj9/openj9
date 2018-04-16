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
import jit.test.tr.chtable.util.Notifiable;
import jit.test.tr.chtable.util.NotifyTask;

@Test(groups = { "level.sanity","component.jit" })
public class PreexistenceTests implements Notifiable {
	private static final int TASK_LOAD_B = 1;
	private static final int TASK_LOAD_C = 2;
	private static final int TASK_FIN    = 3;
	
	private Timer timer;
	private volatile boolean keepOnGoing;
	private volatile A aField;
	
	class A {
		int method1() { return 'A'; }
		int method2() { return 'a'; }
	}
	
	class B extends A {
		int method1() { return 'B'; }
	}
	
	class C extends A {
		int method1() { return 'C'; }
		int method2() { return 'c'; }
	}
	
	@BeforeMethod
	protected void setUp() throws Exception {
		aField = new A();
		timer = new Timer();
	}
	
	@AfterMethod
	protected void tearDown() throws Exception {
		timer.cancel();
		timer = null;
	}
	
	private int callMethod1(A a) {
		// spend some time here -- so that we are sampled
		for (int i = 1 << 6; i > 0; --i)
			a.method1();
		return a.method1();
	}
	
	private int callMethod2(A a) {
		// spend some time here -- so that we are sampled
		for (int i = 1 << 6; i > 0; --i)
			a.method2();
		return a.method2();
	}
	
	// two copies - each called once to make sure that they are not compiled
	// leaving callMethod1 and callMethod2 not-inlined - and hence allowing 
	// preexistence
	//
	private void go1() {
		while (keepOnGoing) {
			callMethod1(aField);
			callMethod2(aField);
		}
	}
	private void go2() {
		while (keepOnGoing) {
			callMethod1(aField);
			callMethod2(aField);
		}
	}
	
	@Test
	public void testClassAssumption() {
		timer.schedule(new NotifyTask(TASK_LOAD_B, this), 5000);
		keepOnGoing = true;
		go1(); // until B gets loaded
		AssertJUnit.assertTrue(callMethod1(aField) == 'B');
		AssertJUnit.assertTrue(callMethod2(aField) == 'a');
	}
	
	@Test
	public void testMethodAssumption() {
		timer.schedule(new NotifyTask(TASK_LOAD_C, this), 5000);	
		keepOnGoing = true;
		go2(); // until C gets loaded
		AssertJUnit.assertTrue(callMethod1(aField) == 'C');
		AssertJUnit.assertTrue(callMethod2(aField) == 'c');
	}

	/* (non-Javadoc)
	 * @see jit.test.tr.chtable.Interruptable#wakeUp()
	 * Should not be run as a test, therefore it is disabled
	 */
	@Test(enabled = false)
	public void wakeUp(int event) {
		switch (event) {
			case TASK_LOAD_B:
				aField = new B(); break;
			case TASK_LOAD_C:
				aField = new C(); break;
		}
		keepOnGoing = false;
	}
}
