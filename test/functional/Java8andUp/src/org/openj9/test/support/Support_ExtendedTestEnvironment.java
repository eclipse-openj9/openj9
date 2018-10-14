package org.openj9.test.support;

import org.openj9.test.java.lang.Test_Class;
import org.testng.log4testng.Logger;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

public class Support_ExtendedTestEnvironment {
	private static final Logger logger = Logger.getLogger(Support_ExtendedTestEnvironment.class);

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

	public SecurityManager setSecurityManager(final Class clazz) {
		final SecurityManager[] sm = new SecurityManager[1];

		try {
			if (clazz != null) {
				sm[0] = (SecurityManager) clazz.newInstance();
			} else {
				sm[0] = null;
			}
			System.setSecurityManager(sm[0]);
		} catch (IllegalAccessException e) {
			logger.error("Unexpected exception: " + e);
		} catch (InstantiationException e) {
			logger.error("Unexpected exception: " + e);
		}
		return sm[0];
	}
}
