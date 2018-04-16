/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.redefineClasses;

import java.util.concurrent.Semaphore;

public class rc012_testRedefineRunningNativeMethod_O1 {
	private Semaphore startSem;
	private Semaphore redefineSem;

	public rc012_testRedefineRunningNativeMethod_O1(Semaphore startSem, Semaphore redefineSem) {
		this.startSem = startSem;
		this.redefineSem = redefineSem;
	}

	public native String meth1();

	// called 1st from meth1() 
	public String meth3() {
		try {
			System.out.println("meth3: release startSem");
			// notify parent to redefine this class
			startSem.release();
			// wait for the parent to tell us it has redefined this class
			System.out.println("meth3: acquire redefineSem");
			redefineSem.acquire();
			System.out.println("meth3: acquired redefineSem");
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	// called 2nd from meth1() 
	public String meth2() {
		return "before";
	}

}

