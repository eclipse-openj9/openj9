/*
 * Copyright IBM Corp. and others 2025
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
package com.ibm.dump.tests.javacore_vthread_monitor;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

/**
 * Validates that a javacore produced by {@link CreateJavaCoreVThreadMonitor}
 * contains correct monitor information for the unmounted virtual thread.
 *
 * Two properties are checked:
 *
 * <ol>
 *   <li><b>Unmounted Threads section</b> — the vthread's Java callstack must
 *       include a {@code 5XESTACKTRACE} "entered lock" annotation on the
 *       {@code holdMonitorAndPark} frame, proving that
 *       {@code continuation->monitorEnterRecords} are now wired into the stack
 *       walk for unmounted vthreads (Fix 1).</li>
 *   <li><b>Monitor Pool section</b> — the object monitor for the lock held by
 *       the vthread must identify the owner by the vthread's name rather than
 *       the anonymous {@code <detached virtual thread>} placeholder, proving
 *       that {@code J9ObjectMonitor::ownerContinuation} is now followed to
 *       resolve the name (Fix 2).</li>
 * </ol>
 *
 * Usage: {@code java CheckJavaCoreVThreadMonitor <javacore-file>}
 */
public class CheckJavaCoreVThreadMonitor {

	private static final String VTHREAD_NAME    = CreateJavaCoreVThreadMonitor.VTHREAD_NAME;
	/* Javacore uses '/' as package separator, not '.' */
	private static final String LOCK_CLASS_NAME = CreateJavaCoreVThreadMonitor.LOCK.getClass().getName().replace('.', '/');
	private static final String HOLD_METHOD     = "holdMonitorAndPark";

	public static void main(String[] args) {
		if (args.length < 1) {
			System.err.println("Usage: CheckJavaCoreVThreadMonitor <javacore-file>");
			System.exit(1);
		}

		boolean passed = true;
		try (BufferedReader in = new BufferedReader(new FileReader(args[0]))) {
			String[] lines = in.lines().toArray(String[]::new);
			printUnmountedThreadsSection(lines);
			printMonitorPoolSection(lines);
			System.err.println();
			passed &= checkUnmountedThreadSection(lines);
			passed &= checkMonitorPoolSection(lines);
		} catch (FileNotFoundException e) {
			System.err.println("File not found: " + args[0]);
			System.exit(1);
		} catch (IOException e) {
			System.err.println("Error reading javacore: " + e.getMessage());
			System.exit(1);
		}

		System.err.println();
		if (passed) {
			System.out.println("PASSED");
			System.exit(0);
		} else {
			System.out.println("FAILED");
			System.exit(1);
		}
	}

	// -------------------------------------------------------------------------
	// Raw section printers
	// -------------------------------------------------------------------------

	/** Prints the full "Unmounted Threads" section from the javacore. */
	private static void printUnmountedThreadsSection(String[] lines) {
		System.err.println("========================================");
		System.err.println("RAW: Unmounted Threads section");
		System.err.println("========================================");
		boolean inSection = false;
		for (String l : lines) {
			if (l.startsWith("1XMVTHDINFO") && l.contains("Unmounted Threads")) {
				inSection = true;
			}
			if (inSection) {
				System.err.println(l);
				/* The section ends at the separator line that follows NULL NULL. */
				if (l.startsWith("NULL") && l.contains("---")) {
					break;
				}
			}
		}
		if (!inSection) {
			System.err.println("  <section not found — was -XX:+ShowUnmountedThreadStacks used?>");
		}
		System.err.println();
	}

	/** Prints the full "Monitor Pool Dump" section from the javacore. */
	private static void printMonitorPoolSection(String[] lines) {
		System.err.println("========================================");
		System.err.println("RAW: Monitor Pool Dump section");
		System.err.println("========================================");
		boolean inSection = false;
		for (String l : lines) {
			if (l.startsWith("1LKMONPOOLDUMP")) {
				inSection = true;
			}
			if (inSection) {
				System.err.println(l);
				/* Section ends at the next top-level separator. */
				if (!inSection) break;
				if (l.startsWith("NULL") && l.contains("---")) {
					break;
				}
			}
		}
		if (!inSection) {
			System.err.println("  <section not found>");
		}
		System.err.println();
	}

	// -------------------------------------------------------------------------
	// Fix 1 — "entered lock" annotation in the Unmounted Threads stack
	// -------------------------------------------------------------------------

	/**
	 * Scans the "Unmounted Threads" section for our vthread and checks that
	 * somewhere in its callstack there is a {@code 5XESTACKTRACE (entered lock: ...)}
	 * record for {@link CreateJavaCoreVThreadMonitor#LOCK}.
	 *
	 * Note: the entered-lock annotation appears at the yield/park point (where the
	 * vthread unmounted), which may be several frames above {@code holdMonitorAndPark}.
	 */
	private static boolean checkUnmountedThreadSection(String[] lines) {
		System.err.println("--- Checking Unmounted Threads section (Fix 1) ---");

		/* Find the vthread's 3XMVTHDINFO header in the unmounted section. */
		int vthreadLine = -1;
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].startsWith("3XMVTHDINFO") && lines[i].contains("\"" + VTHREAD_NAME + "\"")) {
				vthreadLine = i;
				break;
			}
		}
		if (vthreadLine < 0) {
			System.err.println("FAIL: Could not find unmounted vthread '" + VTHREAD_NAME + "' in 3XMVTHDINFO lines.");
			System.err.println("      (Was the JVM launched with -XX:+ShowUnmountedThreadStacks?)");
			return false;
		}
		System.err.println("Found vthread at line " + (vthreadLine + 1) + ": " + lines[vthreadLine]);

		/* Scan forward through the entire vthread block for the entered-lock annotation. */
		boolean foundHoldMethod = false;
		boolean foundEnteredLock = false;
		for (int i = vthreadLine + 1; i < lines.length; i++) {
			String l = lines[i];
			/* Two consecutive NULLs end the unmounted threads section. */
			if (l.equals("NULL")) {
				break;
			}
			if (l.startsWith("4XESTACKTRACE") && l.contains(HOLD_METHOD)) {
				foundHoldMethod = true;
				System.err.println("Found hold frame at line " + (i + 1) + ": " + l);
			}
			if (l.startsWith("5XESTACKTRACE") && l.contains("entered lock:") && l.contains(LOCK_CLASS_NAME)) {
				foundEnteredLock = true;
				System.err.println("PASS (Fix 1): Found entered lock annotation: " + l);
			}
		}

		if (!foundHoldMethod) {
			System.err.println("FAIL (Fix 1): Frame '" + HOLD_METHOD + "' not found in the vthread's callstack.");
			return false;
		}
		if (!foundEnteredLock) {
			System.err.println("FAIL (Fix 1): No 'entered lock' annotation for '" + LOCK_CLASS_NAME + "' found in the vthread's callstack.");
			return false;
		}
		return true;
	}

	// -------------------------------------------------------------------------
	// Fix 2 — vthread name in the Monitor Pool owner field
	// -------------------------------------------------------------------------

	/**
	 * Scans the "Monitor Pool Dump" section for the monitor associated with
	 * {@link CreateJavaCoreVThreadMonitor#LOCK} and checks that the owner
	 * line names the vthread instead of the anonymous placeholder.
	 */
	private static boolean checkMonitorPoolSection(String[] lines) {
		System.err.println("--- Checking Monitor Pool section (Fix 2) ---");

		/*
		 * Look for the 3LKMONOBJECT line that identifies our lock object.
		 * Javacore uses '/' as package separator, e.g.:
		 *   3LKMONOBJECT       java/lang/Object@0x...: Flat locked by "vthread-monitor-test" ...
		 * or (the old broken output):
		 *   3LKMONOBJECT       java/lang/Object@0x...: <detached virtual thread>, entry count 1
		 */
		for (int i = 0; i < lines.length; i++) {
			String l = lines[i];
			if (l.startsWith("3LKMONOBJECT") && l.contains(LOCK_CLASS_NAME)) {
				System.err.println("Found monitor object line: " + l);
				if (l.contains("<detached virtual thread>")) {
					System.err.println("FAIL (Fix 2): Owner is still '<detached virtual thread>' — vthread name was not resolved.");
					return false;
				}
				if (l.contains("owner \"" + VTHREAD_NAME + "\"") || l.contains("Flat locked by \"" + VTHREAD_NAME + "\"")) {
					System.err.println("PASS (Fix 2): Monitor pool owner correctly names vthread '" + VTHREAD_NAME + "'.");
					return true;
				}
				System.err.println("FAIL (Fix 2): Owner line found but vthread name '" + VTHREAD_NAME + "' is absent: " + l);
				return false;
			}
		}

		System.err.println("FAIL (Fix 2): No 3LKMONOBJECT line for '" + LOCK_CLASS_NAME + "' found in the javacore.");
		return false;
	}
}
