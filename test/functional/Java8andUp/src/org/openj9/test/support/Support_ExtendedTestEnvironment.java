package org.openj9.test.support;

/*******************************************************************************
 * Copyright IBM Corp. and others 2010
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

public class Support_ExtendedTestEnvironment {
	private static Support_ExtendedTestEnvironment environment;

	public void execute(Runnable toRun) {
		toRun.run();
	}

	public Thread getThread(Runnable r, String name) {
		Thread t;
		t = new Thread(r, name);
		return t;
	}

	public Thread getThread(Runnable r) {
		Thread t;
		t = new Thread(r);
		return t;
	}

	public static Support_ExtendedTestEnvironment getInstance() {
		if (environment == null) {
			environment = new Support_ExtendedTestEnvironment();
		}
		return environment;
	}
}
