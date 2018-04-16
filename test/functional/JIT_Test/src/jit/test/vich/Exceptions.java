/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class Exceptions {

private static Logger logger = Logger.getLogger(Exceptions.class);
Timer timer;

public Exceptions() {
	timer = new Timer ();
}

@Test(groups = { "level.sanity","component.jit" })
public void testExceptions() {
	for (int depth = 0; depth <= 100; depth += 10) {
		timer.reset();
		for (int i = 0; i < 10000; i++)
			try {
			testException(depth);
		} catch (Exception e) {
		}
		timer.mark();
		logger.info("10000 Exception Throws of depth " + Integer.toString(depth) + " = " + Long.toString(timer.delta()));
	}
	return;
}
/**
 * @exception java.lang.Exception The exception description.
 */
public void testException (int i ) throws Exception {
	if (i == 0) throw new Exception();
	testException (i - 1);
	return;
}
}
