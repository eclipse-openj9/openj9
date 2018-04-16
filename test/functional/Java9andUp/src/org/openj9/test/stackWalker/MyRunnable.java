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
package org.openj9.test.stackWalker;

import static org.testng.Assert.assertEquals;

import java.lang.StackWalker.Option;

import org.testng.log4testng.Logger;

public class MyRunnable implements Runnable {
	protected static Logger logger = Logger.getLogger(StackWalkerTest.class);
	String runnableClassName;

	static void logMessage(String message) {
		logger.debug(message);
	}
	@Override
	public void run() {
		logMessage("Dumping stack");
		for (StackTraceElement e: Thread.currentThread().getStackTrace()) {
			logMessage("Stack trace element:" +e.getClassName()
			+"."+e.getMethodName()+" classloader="+e.getClassLoaderName());
			runnableClassName = getClass().getName();
			if (e.getClassName().equals(runnableClassName)) {
				assertEquals(e.getClassLoaderName(), StackWalkerTest.TEST_MYLOADER_INSTANCE, "Wrong classloader name");
			}
		}
		
		StackWalker sw = StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE);
		
		sw.forEach(f->{
			StackTraceElement e = f.toStackTraceElement();
			if (e.getClassName().equals(runnableClassName)) {
				assertEquals(e.getClassLoaderName(), StackWalkerTest.TEST_MYLOADER_INSTANCE, "Wrong classloader name");
			}
		});
	}

}
