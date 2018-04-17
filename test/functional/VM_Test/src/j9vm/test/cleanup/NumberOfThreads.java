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
package j9vm.test.cleanup;

/* test to ensure that the main thread remains counted as a
 * member of the System ThreadGroup after it has completed.
 */

public class NumberOfThreads {

	public static Thread main;

public static void main(java.lang.String[] args) {
	new NumberOfThreads().run();
}

public void run() {
	
	main = Thread.currentThread();
		
	(new Thread() { public void run() {
		while (true) {
			System.out.println("threads: " + Thread.currentThread().getThreadGroup().activeCount());
			System.out.println("isAlive: " + main.isAlive());
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {}
		}
	}}).start();

	try {
		Thread.sleep(1000);
	} catch (InterruptedException e) {}

		System.out.println("done");
	}
	
}
