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

public class rc012_testRedefineManyRunningMethods_R1 {
	private Semaphore sem;

	public rc012_testRedefineManyRunningMethods_R1(Semaphore sem) {
		this.sem = sem;
	}

	private boolean shouldStop() {
		return true;
	}

	private int fib(int n) {
		if (n <= 1) {
			return n;
		} else {
			return fib(n-1)+fib(n-2);
		}
	}

	public String run() {
		while (!shouldStop()) {
			fib((int)(25*Math.random()));
		}
		return "new";
	}
}

