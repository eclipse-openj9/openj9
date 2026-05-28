/*
 * Copyright IBM Corp. and others 2026
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
 * Assisted-by: IBM Bob
 */
package org.openj9.test;

public class JFRCmdLinePropertiesTest {
	public static void main(String[] args) {
		String filename = System.getProperty("openj9.jfr.cmdline.startflightrecording.filename");
		String delayNanos = System.getProperty("openj9.jfr.cmdline.startflightrecording.delay");
		String durationNanos = System.getProperty("openj9.jfr.cmdline.startflightrecording.duration");

		System.out.println("filename=" + (filename != null ? filename : "null"));
		System.out.println("delay=" + (delayNanos != null ? delayNanos + " nanoseconds" : "null"));
		System.out.println("duration=" + (durationNanos != null ? durationNanos + " nanoseconds" : "null"));

		// Run workload to keep application alive for 2 minutes
		System.out.println("Starting workload for 2 minutes...");
		long startTime = System.currentTimeMillis();

		// Create WorkLoad with parameters tuned for ~2 minute execution
		// numberOfThreads=10, sizeOfNumberList=5000, repeats=20
		WorkLoad workload = new WorkLoad(10, 5000, 20, false);
		workload.runWork();

		long totalTime = System.currentTimeMillis() - startTime;
		System.out.println("Workload completed. Total time: " + (totalTime / 1000) + " seconds");
	}
}
