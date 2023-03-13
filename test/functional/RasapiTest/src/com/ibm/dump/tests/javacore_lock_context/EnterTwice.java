/*******************************************************************************
 * Copyright IBM Corp. and others 2016
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dump.tests.javacore_lock_context;

public class EnterTwice extends StackCreator {

	private boolean ready = false;

	private class Lock {}
	
	Lock lock = new Lock();
		
	@Override
	public void run() {
		Thread.currentThread().setName(this.getClass().getSimpleName() + " Thread");
		enterMethod();
	}

	private void enterMethod() {
		synchronized (lock) {
			try {
				synchronized (lock) {
					entryRecords.add(new EntryRecord(lock.getClass().getName(), 2));
					ready = true; // We have taken our own lock.
					Thread.sleep(1000*60*60); // Sleep for an hour. (The test will exit before then.)
				}
			} catch (InterruptedException e) {
			} 
		}
	}

	@Override
	public boolean isReady() {
		return ready;
	}


}
