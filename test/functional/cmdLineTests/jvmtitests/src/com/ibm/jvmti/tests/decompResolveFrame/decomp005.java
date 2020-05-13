/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package com.ibm.jvmti.tests.decompResolveFrame;

public class decomp005
{
	private static boolean pass;
	private static boolean entered;
	private native boolean triggerDecompile();
	
	public void interp(boolean jitted) throws Throwable {
		if (jitted) {
			if (!triggerDecompile()) {
				System.out.println("FAIL: Failed to trigger decompilation");
				return;
			}
		}
		throw new InterruptedException();
	}

	public synchronized void jitInline(boolean jitted) throws Throwable {
		// Before the fix that spawned this test, the decompilation would result
		// in this method being restarted at bytecode 0. Detect this, and report
		// failure if it occurs.
		if (!entered) {
			entered = true;
			interp(jitted);
		}
		System.out.println("FAIL: jitInline was restarted at bytecode 0 (crash likely to follow)");
		pass = false;
	}

	public boolean jitOuter(boolean jitted) {
		entered = false;
		pass = true;
		try {
			jitInline(jitted);
		} catch(Throwable e) {
			if (jitted) {
				System.out.println("Exception caught in jitOuter: " + e);
			}
			return pass;
		}
		System.out.println("FAIL: Exception not caught in jitOuter");
		return false;
	}

	public boolean testInlinedSyncDecompile() {
		decomp005 o = new decomp005();
		try {
			// Resolve the inlined call (the test runs with -Xjit:count=1)
			if (!o.jitOuter(false)) {
				System.out.println("FAIL: Warmup failed");
				return false;
			}
			// Now compile and run the test
			return o.jitOuter(true);
		} catch(Throwable t) {
			System.out.println("FAIL: Uncaught exception during test: " + t);
			return false;
		}
	}

	public String helpInlinedSyncDecompile() {
		return "decompile at exception catch with an inlined synchronized method"; 
	}
}
