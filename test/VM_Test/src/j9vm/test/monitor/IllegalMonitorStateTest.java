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
 * Tests that wait and notify throw IllegalMonitorState, especially
 * when the lock has been reserved but is not owned.
 */
public class IllegalMonitorStateTest {

	public static void testWait(boolean reserve) {
		Object obj = new Object();
		
		if (reserve) {
			Helpers.monitorReserve(obj);
		}
		
		try {
			obj.wait(1);
			throw new Error("wait failed to throw IllegalMonitorStateException");
		} catch (IllegalMonitorStateException e) {
			// expected
		} catch (InterruptedException e) {
			throw new Error("Unexpected InterruptedException");
		}
	}
	

	public static void testNotify(boolean reserve) {
		Object obj = new Object();
		
		if (reserve) {
			Helpers.monitorReserve(obj);
		}
		
		try {
			obj.notify();
			throw new Error("notify failed to throw IllegalMonitorStateException");
		} catch (IllegalMonitorStateException e) {
			// expected
		}
	}
	
	public static void testNotifyAll(boolean reserve) {
		Object obj = new Object();
		
		if (reserve) {
			Helpers.monitorReserve(obj);
		}
		
		try {
			obj.notifyAll();
			throw new Error("notifyAll failed to throw IllegalMonitorStateException");
		} catch (IllegalMonitorStateException e) {
			// expected
		}
	}

	public static void main(String[] args) {
		testWait(false);
		testNotify(false);
		testNotifyAll(false);

		testWait(true);
		testNotify(true);
		testNotifyAll(true);

	}
	
}
