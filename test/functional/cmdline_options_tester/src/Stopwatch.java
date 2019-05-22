/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.util.*;
import java.io.*;
import java.text.*;

/**
 * STOP watch to calculate time spent in milliseconds in each test
 * ToDo: Move to System.nanoTime() instead of System.currentTimeMillis() once test source 
 * is built with 1.5 or higher.
 * Currently millisecond calculation entirely depends on OS update of timer values
 * which happens only once in few tens of milliseconds. So getting millisecond directly from os may 
 * not be accurate.
 * 
 */
public class Stopwatch {

	private long startTime = -1;
	private long stopTime = -1;
	private boolean running = false;
	private Calendar calendar = Calendar.getInstance();
	TimeZone timeZone = calendar.getTimeZone();
	DateFormat df = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");

	public Stopwatch start() {
		System.out.println("Test start time: " + df.format(calendar.getTime()) +  " " + timeZone.getDisplayName());
		startTime = System.currentTimeMillis();
		running = true;
		return this;
	}

	public Stopwatch stop() {
		stopTime = System.currentTimeMillis();
		running = false;
		return this;
	}

	/** getTimeSpent will return the elapsed time in milliseconds 
	 * if the watch has never been started then return zero
	 */
	public long getTimeSpent() {
		if (startTime == -1) {
			return 0;
		}
		if (running) {
			return System.currentTimeMillis() - startTime;
		} else {
			return stopTime - startTime;
		}
	}
}
