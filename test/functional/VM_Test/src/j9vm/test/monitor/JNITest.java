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

/**
 * 
 * @author PBurka
 *
 * Tests JNI APIs for conformance. 
 * 
 * See VMDESIGN 941.
 */
public class JNITest {

	private static class MyException extends RuntimeException {
		private static final long serialVersionUID = -4340306089710938052L;
	};
	
	/*
	 * 1) MonitorExit with a correctly acquired monitor (0)
	 */
	public static void test1() {
		Object obj = new Object();
		int rc;
		
		rc = Helpers.monitorEnter(obj);
		if (rc != 0) {
			throw new Error("Unexpected return code from MonitorEnter(): " + rc);
		}
		rc = Helpers.monitorExit(obj);
		if (rc != 0) {
			throw new Error("Unexpected return code from MonitorExit(): " + rc);
		}
	}
	
	/*
	 * 2) MonitorExit with an unowned monitor (-1 and IllegalMonitorStateException)
	 */
	public static void test2() {
		Object obj = new Object();
		
		try {
			int rc = Helpers.monitorExit(obj);
			throw new Error("MonitorExit() failed to throw an IllegalMonitorStateException. rc = " + rc);
		} catch (IllegalMonitorStateException e) {
			// expected
		}
		
		if (Helpers.getLastReturnCode() >= 0) {
			throw new Error("MonitorExit() failed to return a negative value. rc = " + Helpers.getLastReturnCode());
		}
	}

	/*
	 * 3) MonitorExit with a correctly acquired monitor and a pending exception (0)
	 */
	public static void test3() {
		Object obj = new Object();
		int rc;
		
		rc = Helpers.monitorEnter(obj);
		if (rc != 0) {
			throw new Error("Unexpected return code from MonitorEnter(): " + rc);
		}
		try {
			rc = Helpers.monitorExitWithException(obj, new MyException());
			throw new Error("monitorExitWithException() failed to throw MyException. rc = " + rc);
		} catch (MyException e) {
			// expected
		}

		if (Helpers.getLastReturnCode() != 0) {
			throw new Error("MonitorExit() failed to return 0. rc = " + Helpers.getLastReturnCode());
		}
	}

	/*
	 * 4) MonitorExit with an unowned monitor and a pending exception (-1 and IllegalMonitorStateException)
	 */
	public static void test4() {
		Object obj = new Object();
		
		try {
			int rc = Helpers.monitorExitWithException(obj, new MyException());
			throw new Error("MonitorExit() failed to throw an IllegalMonitorStateException. rc = " + rc);
		} catch (IllegalMonitorStateException e) {
			// expected
		} catch (MyException e) {
			throw new Error("MonitorExit() failed to replace pending exception with IllegalMonitorStateException");
		}

		if (Helpers.getLastReturnCode() >= 0) {
			throw new Error("MonitorExit() failed to return a negative value. rc = " + Helpers.getLastReturnCode());
		}
	}

	public static void main(String[] args) throws InterruptedException {
		test1();
		test2();
		test3();
		test4();
	}

}
