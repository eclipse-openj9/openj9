/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.trace.tests.trace_subscriber;

public class TraceQueue {
	public static void main(String[] args) {
		int result = 0;
		int publishers = 0;
		int subscribers = 0;
		
		/* invoked with com.ibm.ras.tests.TraceQueue publishers subscribers */
		if (args.length == 2) {
			try {
				publishers = Integer.parseInt(args[0]);
			} catch (NumberFormatException e) {
				System.err.println("Invalid value specified for publishers, requires an integer greater than 0");
				result = -1;
			}

			try {
				subscribers = Integer.parseInt(args[1]);
			} catch (NumberFormatException e) {
				System.err.println("Invalid value specified for subscribers, requires an integer greater than 0");
				result = -1;
			}			
		}
		
		/* print usage if there's been a problem */
		if (args.length != 2 || result != 0) {
			System.err.println("Usage: com.ibm.ras.tests.TraceQueue publishers subscribers");
			System.exit(-1);
		}
		
		/* try running the test */
		if (!runQueueTests(subscribers, publishers)) {
			System.err.println("FAILED");
			result = -2;
		} else {
			System.out.println("PASSED");
			result = 0;
		}
		
		System.exit(result);
	}
	
	public static native boolean runQueueTests(int subscribers, int publishers);
}