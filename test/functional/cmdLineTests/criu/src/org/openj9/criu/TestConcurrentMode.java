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
package org.openj9.criu;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import org.eclipse.openj9.criu.CRIUSupport;
import org.eclipse.openj9.criu.JVMCheckpointException;

public class TestConcurrentMode {

	private static final int USER_HOOK_MODE_PRIORITY_LOW = 1;
	private static final int USER_HOOK_MODE_PRIORITY_HIGH = 2;

	public static void main(String[] args) {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			String testName = args[0];

			switch (testName) {
			case "TestConcurrentModePreCheckpointHookThrowException":
				TestConcurrentModePreCheckpointHookThrowException();
				break;
			case "TestConcurrentModePreCheckpointHookThrowExceptionPriority":
				TestConcurrentModePreCheckpointHookThrowExceptionPriority();
				break;
			case "TestConcurrentModePreCheckpointHookRunOnce":
				TestConcurrentModePreCheckpointHookRunOnce();
				break;
			case "TestConcurrentModePreCheckpointHookPriorities":
				TestConcurrentModePreCheckpointHookPriorities();
				break;
			case "TestConcurrentModePostRestoreHookThrowException":
				TestConcurrentModePostRestoreHookThrowException();
				break;
			case "TestConcurrentModePostRestoreHookThrowExceptionPriority":
				TestConcurrentModePostRestoreHookThrowExceptionPriority();
				break;
			case "TestConcurrentModePostRestoreHookRunOnce":
				TestConcurrentModePostRestoreHookRunOnce();
				break;
			case "TestConcurrentModePostRestoreHookPriorities":
				TestConcurrentModePostRestoreHookPriorities();
				break;
			default:
				throw new RuntimeException("Incorrect test name");
			}
		}
	}

	// concurrent-threaded preCheckpointHook throws an exception
	static void TestConcurrentModePreCheckpointHookThrowException() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowException() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPreCheckpointHook(() -> {
			throw new RuntimeException("TestConcurrentModePreCheckpointHookThrowException() within preCheckpointHook");
		}, CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_LOW);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowException() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowException() after doCheckpoint()");
	}

	// concurrent-threaded preCheckpointHook throws UnsupportedOperationException
	static void TestConcurrentModePreCheckpointHookThrowExceptionPriority() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowExceptionPriority() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPreCheckpointHook(
				() -> CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowExceptionPriority() within preCheckpointHook"),
				CRIUSupport.HookMode.CONCURRENT_MODE, 100);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowExceptionPriority() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookThrowExceptionPriority() after doCheckpoint()");
	}

	// concurrent-threaded preCheckpointHook finishes successfully
	static void TestConcurrentModePreCheckpointHookRunOnce() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookRunOnce() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPreCheckpointHook(
				() -> CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookRunOnce() within preCheckpointHook"),
				CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_LOW);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookRunOnce() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookRunOnce() after doCheckpoint()");
	}

	// concurrent-threaded preCheckpointHook finishes successfully according to the priorities
	static void TestConcurrentModePreCheckpointHookPriorities() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookPriorities() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		final TestResult testResult = new TestResult(true, 0);
		criu.registerPreCheckpointHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with lower priority in CONCURRENT_MODE");
			// check if it is the initial value
			if (testResult.lockStatus.get() != 0) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with lower priority in CONCURRENT_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(1);
			}
		}, CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_LOW);
		criu.registerPreCheckpointHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with higher priority in CONCURRENT_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_LOW in CONCURRENT_MODE
			if (testResult.lockStatus.get() != 1) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with higher priority in CONCURRENT_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(2);
			}
		}, CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_HIGH);
		criu.registerPreCheckpointHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with lower priority in SINGLE_THREAD_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_HIGH in CONCURRENT_MODE
			if (testResult.lockStatus.get() != 2) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with lower priority in SINGLE_THREAD_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(3);
			}
		}, CRIUSupport.HookMode.SINGLE_THREAD_MODE, USER_HOOK_MODE_PRIORITY_LOW);
		criu.registerPreCheckpointHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with higher priority in SINGLE_THREAD_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_LOW in SINGLE_THREAD_MODE
			if (testResult.lockStatus.get() != 3) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The preCheckpointHook with higher priority in SINGLE_THREAD_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(4);
			}
		}, CRIUSupport.HookMode.SINGLE_THREAD_MODE, USER_HOOK_MODE_PRIORITY_HIGH);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookPriorities() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePreCheckpointHookPriorities() after doCheckpoint()");
		if (testResult.testPassed) {
			System.out.println("TestConcurrentModePreCheckpointHookPriorities() PASSED");
		} else {
			System.out.println("TestConcurrentModePreCheckpointHookPriorities() FAILED");
		}
	}

	// concurrent-threaded postRestoreHook throws an exception
	static void TestConcurrentModePostRestoreHookThrowException() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowException() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPostRestoreHook(() -> {
			throw new RuntimeException("TestConcurrentModePostRestoreHookThrowException() within postRestoreHook");
		}, CRIUSupport.HookMode.CONCURRENT_MODE, 1);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowException() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowException() after doCheckpoint()");
	}

	// concurrent-threaded postRestoreHook throws UnsupportedOperationException
	static void TestConcurrentModePostRestoreHookThrowExceptionPriority() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowExceptionPriority() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPostRestoreHook(
				() -> CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookRunOnce() within postRestoreHook"),
				CRIUSupport.HookMode.CONCURRENT_MODE, -1);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowExceptionPriority() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookThrowExceptionPriority() after doCheckpoint()");
	}

	// concurrent-threaded postRestoreHook finishes successfully
	static void TestConcurrentModePostRestoreHookRunOnce() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookRunOnce() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.registerPostRestoreHook(
				() -> CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookRunOnce() within postRestoreHook"),
				CRIUSupport.HookMode.CONCURRENT_MODE, 1);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookRunOnce() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookRunOnce() after doCheckpoint()");
	}

	// concurrent-threaded postRestoreHook finishes successfully according to the priorities
	static void TestConcurrentModePostRestoreHookPriorities() {
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookPriorities() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		final TestResult testResult = new TestResult(true, 0);
		criu.registerPostRestoreHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with lower priority in CONCURRENT_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_LOW in CONCURRENT_MODE
			if (testResult.lockStatus.get() != 3) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with lower priority in CONCURRENT_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(4);
			}
		}, CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_LOW);
		criu.registerPostRestoreHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with higher priority in CONCURRENT_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_LOW in SINGLE_THREAD_MODE
			if (testResult.lockStatus.get() != 2) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with higher priority in CONCURRENT_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(3);
			}
		}, CRIUSupport.HookMode.CONCURRENT_MODE, USER_HOOK_MODE_PRIORITY_HIGH);
		criu.registerPostRestoreHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with lower priority in SINGLE_THREAD_MODE");
			// check if it is the value set by the hook with USER_HOOK_MODE_PRIORITY_HIGH in SINGLE_THREAD_MODE
			if (testResult.lockStatus.get() != 1) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with lower priority in SINGLE_THREAD_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(2);
			}
		}, CRIUSupport.HookMode.SINGLE_THREAD_MODE, USER_HOOK_MODE_PRIORITY_LOW);
		criu.registerPostRestoreHook(() -> {
			CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with higher priority in SINGLE_THREAD_MODE");
			// check if it is the initial value
			if (testResult.lockStatus.get() != 0) {
				testResult.testPassed = false;
				CRIUTestUtils.showThreadCurrentTime("The postRestoreHook with higher priority in SINGLE_THREAD_MODE failed with testResult.lockStatus = "
						+ testResult.lockStatus.get());
			} else {
				testResult.lockStatus.set(1);
			}
		}, CRIUSupport.HookMode.SINGLE_THREAD_MODE, USER_HOOK_MODE_PRIORITY_HIGH);

		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookPriorities() Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		CRIUTestUtils.showThreadCurrentTime("TestConcurrentModePostRestoreHookPriorities() after doCheckpoint()");
		if (testResult.testPassed) {
			System.out.println("TestConcurrentModePostRestoreHookPriorities() PASSED");
		} else {
			System.out.println("TestConcurrentModePostRestoreHookPriorities() FAILED");
		}
	}
}
