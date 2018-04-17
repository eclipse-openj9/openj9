/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jun 6, 2006
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.monitor;

public class EnterExitTest {

	private static final int recursionCount = 65;

	public static void test1(boolean reserve) {

		for (int i = 0; i < recursionCount; i++) {
			EnterExitTest obj = new EnterExitTest();

			if (reserve) {
				Helpers.monitorReserve(obj);
			}
			
			try {
				obj.synchronizedMethod(i);
			} catch (RuntimeException e) {
				System.err.println("synchronizedMethod() failed at depth " + i);
				throw e;
			}
		
			// verify that the last call exited the monitor completely
			try {
				Helpers.monitorExit(obj);
				throw new Error("synchronizedMethod(" + i + ") failed to release monitor on exit");
			} catch (IllegalMonitorStateException e) {
				// expected
			}
		}
	}
	
	public static void test2(boolean reserve) {

		for (int i = 0; i < recursionCount; i++) {
			EnterExitTest obj = new EnterExitTest();

			if (reserve) {
				Helpers.monitorReserve(obj);
			}

			try {
				obj.synchronizedIllegalMonitorState(i);
				throw new Error("synchronizedIllegalMonitorState(" + i + ") failed to throw IllegalMonitorStateException");
			} catch (IllegalMonitorStateException e) {
				// expected
			} catch (RuntimeException e) {
				System.err.println("synchronizedIllegalMonitorState() failed at depth " + i);
				throw e;				
			}
		}
	}
	
	public static void test3(boolean reserve) {

		for (int i = 0; i < recursionCount; i++) {
			EnterExitTest obj = new EnterExitTest();
		
			if (reserve) {
				Helpers.monitorReserve(obj);
			}

			try {
				obj.monitorMethod(i);
			} catch (RuntimeException e) {
				System.err.println("monitorMethod() failed at depth " + i);
				throw e;
			}
		
			// verify that the last call exited the monitor completely
			try {
				Helpers.monitorExit(obj);
				throw new Error("monitorMethod(" + i + ") failed to release monitor on exit");
			} catch (IllegalMonitorStateException e) {
				// expected
			}
		}
	}
	
	public static void test4(boolean reserve) {

		for (int i = 0; i < recursionCount; i++) {
			EnterExitTest obj = new EnterExitTest();
		
			if (reserve) {
				Helpers.monitorReserve(obj);
			}

			try {
				obj.monitorIllegalMonitorState(i);
				throw new Error("monitorIllegalMonitorState(" + i + ") failed to throw IllegalMonitorStateException");
			} catch (IllegalMonitorStateException e) {
				// expected
			} catch (RuntimeException e) {
				System.err.println("monitorIllegalMonitorState() failed at depth " + i);
				throw e;				
			}
		}
	}
	
	/*
	 * Test extremely deep nesting
	 */
	public static void test5(boolean reserve) {
		EnterExitTest obj = new EnterExitTest();

		if (reserve) {
			Helpers.monitorReserve(obj);
		}

		for (int i = 0; i < 1000000; i++) {
			Helpers.monitorEnter(obj);
		}

		for (int i = 0; i < 1000000; i++) {
			Helpers.monitorExit(obj);
		}

		// verify that the last call exited the monitor completely
		try {
			Helpers.monitorExit(obj);
			throw new Error("Deep nesting test failed to release monitor on exit");
		} catch (IllegalMonitorStateException e) {
			// expected
		}
		
	}
	
	public static void main(String[] args) {
		
		// first test without reservation
		test1(false);
		test2(false);
		test3(false);
//		test4(false);
		test5(false);
		
		// then test again with reservation
		test1(true);
		test2(true);
		test3(true);
//		test4(true);
		test5(true);

	}

	private synchronized void synchronizedMethod(int nestingDepth) {
		if (nestingDepth > 0) {
			synchronizedMethod(nestingDepth - 1);
		}
	}

	private synchronized void synchronizedIllegalMonitorState(int nestingDepth) {
		if (nestingDepth > 0) {
			synchronizedIllegalMonitorState(nestingDepth - 1);
		} else {
			Helpers.monitorExit(this);
		}
	}

	private void monitorMethod(int nestingDepth) {
		synchronized(this) {
			if (nestingDepth > 0) {
				monitorMethod(nestingDepth - 1);
			}
		}
	}

	/*
	 * This test can't be used because of the way the compiler generates
	 * synchronized regions. The compiler wraps the monitorexit finally
	 * clause in its own exception handler, causing it to go into an
	 * infinite loop if the monitor is exited early.
	 */
	private void monitorIllegalMonitorState(int nestingDepth) {
//		synchronized (this) {
//			if (nestingDepth > 0) {
//				monitorIllegalMonitorState(nestingDepth - 1);
//			} else {
//				Helpers.monitorExit(this);
//			}
//		}
	}	

	
}

