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
 * See defect 97658
 * This test checks that the VM doesn't crash if a thread with a unicode name is created.
 * Some components (such as trace) monitor the creation of new threads and may handle
 * certain characters incorrectly
 */
package j9vm.test.threadname;

public class UnicodeThreadNameTest {

	public void run() throws InterruptedException {
		String name = "";
		
		for (int i = 0; i < 10000; i++) {
			Thread myThread = new Thread(name) {
	        	public void run() {
	        	}
			};
			myThread.start();
			myThread.join();
			
			// a Japanese character which requires a multi-byte encoding
			name = name + "\u30b9";
		}

		

	}

	public static void main(String[] args) throws Throwable {
		new UnicodeThreadNameTest().run();

	}

}
