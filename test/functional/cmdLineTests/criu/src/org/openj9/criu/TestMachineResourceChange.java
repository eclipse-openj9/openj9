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

import java.lang.reflect.Field;
import java.nio.file.Path;
import java.util.concurrent.ForkJoinPool;

import org.eclipse.openj9.criu.CRIUSupport;

public class TestMachineResourceChange {
	public static void main(String[] args) throws InterruptedException {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			String testName = args[0];
			new TestMachineResourceChange().test(testName);
		}
	}

	private void testVirtualThreadForkJoinPoolParallelism(boolean isCheckpoint, boolean isRestoreSysProp)
			throws InterruptedException {
		boolean pass = false;

		System.out.println("testVirtualThreadForkJoinPoolParallelism() isCheckpoint = " + isCheckpoint
				+ ", isRestoreSysProp = " + isRestoreSysProp);
		Thread.ofVirtual()
				.start(() -> System.out
						.println("This is a Virtual Thread after CRIU " + (isCheckpoint ? "checkpoint" : "restore")))
				.join();
		try {
			Class vtClz = Class.forName("java.lang.VirtualThread");
			System.out.println("j.l.VirtualThread : " + vtClz);
			Field fieldDefaultScheduler = vtClz.getDeclaredField("DEFAULT_SCHEDULER");
			fieldDefaultScheduler.setAccessible(true);
			System.out.println("fieldDefaultScheduler = " + fieldDefaultScheduler);
			ForkJoinPool fjp = (ForkJoinPool) fieldDefaultScheduler.get(null);
			System.out.println("j.u.c.ForkJoinPool : " + fjp);
			Field fieldParallelism = ForkJoinPool.class.getDeclaredField("parallelism");
			fieldParallelism.setAccessible(true);
			int parallelism = (int) fieldParallelism.get(fjp);
			System.out.println("j.l.VirtualThread.ForkJoinPool.parallelism = " + parallelism);

			String parallelismValue = System.getProperty("jdk.virtualThreadScheduler.parallelism");
			Integer parallelismInteger = Integer.getInteger("jdk.virtualThreadScheduler.parallelism");
			if ((parallelismInteger != null) && !isRestoreSysProp) {
				// This system property only affects j.l.VirtualThread.ForkJoinPool.parallelism
				// at VM startup.
				int parallelismSysProp = parallelismInteger.intValue();
				if (parallelismSysProp == parallelism) {
					System.out.println(
							"PASSED: testVirtualThreadForkJoinPoolParallelism() j.l.VirtualThread.ForkJoinPool.parallelism retrieved equals system property jdk.virtualThreadScheduler.parallelism set - "
									+ parallelism);
				} else {
					System.out.println(
							"FAILED: testVirtualThreadForkJoinPoolParallelism() j.l.VirtualThread.ForkJoinPool.parallelism retrieved is "
									+ parallelism + ", but system property jdk.virtualThreadScheduler.parallelism is "
									+ parallelismSysProp);
				}
			} else {
				int cpuCount = Runtime.getRuntime().availableProcessors();
				if (cpuCount == parallelism) {
					System.out.println(
							"PASSED: testVirtualThreadForkJoinPoolParallelism() j.l.VirtualThread.ForkJoinPool.parallelism retrieved equals Runtime.getRuntime().availableProcessors() - "
									+ parallelism);
				} else {
					System.out.println(
							"FAILED: testVirtualThreadForkJoinPoolParallelism() j.l.VirtualThread.ForkJoinPool.parallelism retrieved is "
									+ parallelism + ", but Runtime.getRuntime().availableProcessors() is " + cpuCount);
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
			System.out.println(
					"FAILED: testVirtualThreadForkJoinPoolParallelism() throws an exception: " + e.getMessage());
		}
	}

	private void test(String testName) throws InterruptedException {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		CRIUTestUtils.showThreadCurrentTime("Before starting " + testName);
		switch (testName) {
		case "testVirtualThreadForkJoinPoolParallelism":
			testVirtualThreadForkJoinPoolParallelism(true, false);
			break;
		case "testVirtualThreadForkJoinPoolParallelismWithOptionsFileV1": {
			String optionsContents = "-Djdk.virtualThreadScheduler.parallelism=5";
			Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);
			criu.registerRestoreOptionsFile(optionsFilePath);
			testVirtualThreadForkJoinPoolParallelism(true, true);
		}
			break;
		case "testVirtualThreadForkJoinPoolParallelismWithOptionsFileV2": {
			String optionsContents = "-Djdk.virtualThreadScheduler.parallelism=9";
			Path optionsFilePath = CRIUTestUtils.createOptionsFile("options", optionsContents);
			criu.registerRestoreOptionsFile(optionsFilePath);
			testVirtualThreadForkJoinPoolParallelism(true, true);
		}
			break;
		default:
			throw new RuntimeException("Unrecognized test name: " + testName);
		}

		System.out.println("Pre-checkpoint");
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		System.out.println("Post-checkpoint");

		switch (testName) {
		case "testVirtualThreadForkJoinPoolParallelism":
			testVirtualThreadForkJoinPoolParallelism(false, false);
			break;
		case "testVirtualThreadForkJoinPoolParallelismWithOptionsFileV1":
		case "testVirtualThreadForkJoinPoolParallelismWithOptionsFileV2":
			testVirtualThreadForkJoinPoolParallelism(false, true);
			break;
		default:
		}

		CRIUTestUtils.showThreadCurrentTime("End " + testName);
	}
}
