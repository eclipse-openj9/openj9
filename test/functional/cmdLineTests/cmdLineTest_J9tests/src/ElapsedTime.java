
/*
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

import java.util.Timer;
import java.util.TimerTask;

import org.openj9.test.util.TimeUtilities;
import org.testng.AssertJUnit;

public class ElapsedTime {
	public static void main(String[] args) throws Exception {
		testElapsedTime();
	}

	private static void testElapsedTime() throws InterruptedException {
		TimeUtilities tu = new TimeUtilities();
		long millisTimeStart = System.currentTimeMillis();
		long nanoTimeStart = System.nanoTime();

		millisTimeStart = System.currentTimeMillis();
		nanoTimeStart = System.nanoTime();
		Thread.currentThread().sleep(100);
		AssertJUnit.assertTrue(tu.checkElapseTime("testElapsedTime() sleep 100ms", millisTimeStart, nanoTimeStart, 100, 100));

		millisTimeStart = System.currentTimeMillis();
		nanoTimeStart = System.nanoTime();
		tu.timerSchedule("testElapsedTime() timer delayed 100ms", millisTimeStart, nanoTimeStart, 100, 500, 100, 500,
				100);

		millisTimeStart = System.currentTimeMillis();
		nanoTimeStart = System.nanoTime();
		tu.timerSchedule("testElapsedTime() timer delayed 2s", millisTimeStart, nanoTimeStart, 2000, 3000, 2000, 3000,
				2000);

		AssertJUnit.assertTrue(tu.getResultAndCancelTimers());

		System.out.println("PASSED: testElapsedTime()");
	}
}
