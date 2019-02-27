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
package jit.test.tr.chtable;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;
import java.util.Timer;


import jit.test.tr.chtable.util.Notifiable;
import jit.test.tr.chtable.util.NotifyTask;

@Test(groups = { "level.sanity","component.jit" })
public class InnerPreexistence implements Notifiable {
	
	public static final int TASK_LOAD_C = 1;

	private Timer timer;
	private volatile boolean keepOnGoing = true;
	private A someA;
	private B someB;
	private B otherB;

	@BeforeMethod
	protected void setUp() throws Exception {
		someA = new A();
		someB = new B();
		timer = new Timer();
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		timer.cancel();
		timer = null;
	}

	@Test
	public void testInnerPreexistence() {
		timer.schedule(new NotifyTask(TASK_LOAD_C, this), 8000);
		while (keepOnGoing)
			foo();
		AssertJUnit.assertTrue(foo() == 'c');
	}

	//Should not be run as a test, therefore it is disabled
	@Test(enabled = false)
	public void wakeUp(int eventId) {
		someB = new C();
                dummySync(); // to ensure correct ordering on weak memory systems
		keepOnGoing = false;
	}

        private synchronized void dummySync() {}
	
	class A { char bar(B b) { return b.goo(); } }
	class B { char goo() { return 'b'; } }
	class C extends B { char goo() { return 'c'; } }
	
	public long x = 0;
	char foo() {
		// spend some time here to get to scorching
		for (int i = 0; i < 100; ++i)
			x++;
		
		if (x == 0)
			someB = null; // never reached
		
		return someA.bar(someB);
	}
}
