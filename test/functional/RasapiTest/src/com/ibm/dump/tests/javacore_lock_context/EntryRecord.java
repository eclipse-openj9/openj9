/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.javacore_lock_context;

/**
 * Class to hold the *expected* entry records we expect to find in the javacore.
 * 
 * @author hhellyer
 *
 */
public class EntryRecord {

	private int entryCount;
	private StackTraceElement stackEntry;
	private String lockName;
	
	public EntryRecord( String lockName, int entryCount ) {
		this.entryCount = entryCount;
		this.lockName = lockName.replace('.', '/');;
		// Get the stack frame for the calling method.
		// (Yes this is evil, this is just a testcase.)
		this.stackEntry = (new Throwable()).getStackTrace()[1]; 
	}

	public String getMethodName() {
		String className = stackEntry.getClassName().replace('.', '/');
		return  className + "." +stackEntry.getMethodName();
	}

	public String getLockName() {
		return lockName;
	}

	public int getEntryCount() {
		return entryCount;
	}
	
}
