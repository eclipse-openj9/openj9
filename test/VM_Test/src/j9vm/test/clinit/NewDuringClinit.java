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
 * Created on Sep 23, 2005
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.clinit;

public class NewDuringClinit extends Thread {
	public boolean started = false;
	public boolean initialized = false;
	public String error = null;
	
	public static NewDuringClinit instance = new NewDuringClinit();
	
	static {
		
		new Thread(instance).start();
		
		try {
			synchronized (instance) {
				while (instance.started == false) {
					instance.wait();
				}
			}
			Thread.sleep(5000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		instance.initialized = true;
	}
	
	public void run() {
		synchronized (this) {
			started = true;
			notifyAll();
		}
		
		if (initialized) {
			error = "Class was already initialized before new was reached";
			return;
		}
		
		new NewDuringClinit();
		
		if (!initialized) {
			error = "Class was not initialized after new was executed";
			return;
		}

	}

	public static void test() {
		try {
			instance.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		if (instance.error != null) {
			System.out.println(instance.error);
			throw new RuntimeException(instance.error);
		}
	}
	
}
