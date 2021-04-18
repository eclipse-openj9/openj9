/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
package j9vm.test.classunloading.classestoload;

import j9vm.test.classunloading.FinalizationIndicator;
import j9vm.test.classunloading.testcases.UnloadedStackTraceTest;

public class ClassToLoadWithException {
	private static FinalizationIndicator indicator = new FinalizationIndicator( ClassToLoadWithException.class.getName());
	
	public ClassToLoadWithException() throws UnloadedStackTraceTest.MyError {
		/* by doing this 10000 times, we give the jIT a good chance to compile the helper */
		for (int i = 0; i < 10000; i++) {
			helper(i);
		}
	}
	
	private static void helper(int i) throws UnloadedStackTraceTest.MyError {
		if (i == 9999) {
			throw new UnloadedStackTraceTest.MyError("This Exception's stack trace refers to a class which is about to be unloaded");
		}
	}
	
}