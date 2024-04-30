/*
 * Copyright IBM Corp. and others 2022
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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Field;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.nio.file.Paths;

import openj9.internal.criu.InternalCRIUSupport;

import org.eclipse.openj9.criu.*;

import jdk.internal.misc.Unsafe;

public class DeadlockTest {
	final static TestResult mainTestResult = new TestResult(true, 0);
	final static Object lock = new Object();

	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "CheckpointDeadlock":
			checkpointDeadlock();
			break;
		case "NotCheckpointSafeDeadlock":
			notCheckpointSafeDeadlock();
			break;
		case "MethodTypeDeadlockTest":
			methodTypeDeadlockTest();
			break;
		case "ClinitTest":
			clinitTest();
			break;
		case "ClinitTest2":
			clinitTest2();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}

	}

	public static void checkpointDeadlock() {
		Path path = Paths.get("cpData");
		Object lock = new Object();
		final TestResult testResult = new TestResult(true, 0);

		Thread t1 = new Thread(() -> {
			synchronized (lock) {
				testResult.lockStatus.set(1);
				try {
					Thread.sleep(20000);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		});

		t1.start();

		CRIUSupport criuSupport = new CRIUSupport(path);
		criuSupport.registerPreCheckpointHook(() -> {
			synchronized (lock) {
				System.out.println("Precheckpoint hook inside monitor");
				testResult.testPassed = false;
			}
		});

		while (testResult.lockStatus.get() == 0) {
			Thread.yield();
		}

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(criuSupport, path, true);
			testResult.testPassed = false;
		} catch (JVMCheckpointException e) {
			/*
			An expected exception:
			org.eclipse.openj9.criu.JVMCheckpointException: Exception thrown when running user pre-checkpoint
				at openj9.criu/org.eclipse.openj9.criu.CRIUSupport.checkpointJVM(CRIUSupport.java:526)
				at org.openj9.criu.CRIUTestUtils.checkPointJVM(CRIUTestUtils.java:77)
			Caused by: openj9.internal.criu.JVMCheckpointException: Exception thrown when running user pre-checkpoint
				at java.base/openj9.internal.criu.InternalCRIUSupport.lambda$registerCheckpointHookHelper$2(InternalCRIUSupport.java:699)
				at java.base/openj9.internal.criu.J9InternalCheckpointHookAPI$J9InternalCheckpointHook.runHook(J9InternalCheckpointHookAPI.java:143)
				at java.base/openj9.internal.criu.J9InternalCheckpointHookAPI.runHooks(J9InternalCheckpointHookAPI.java:98)
				at java.base/openj9.internal.criu.J9InternalCheckpointHookAPI.runPreCheckpointHooksSingleThread(J9InternalCheckpointHookAPI.java:107)
				at java.base/openj9.internal.criu.InternalCRIUSupport.checkpointJVMImpl(Native Method)
				at java.base/openj9.internal.criu.InternalCRIUSupport.checkpointJVM(InternalCRIUSupport.java:867)
				at openj9.criu/org.eclipse.openj9.criu.CRIUSupport.checkpointJVM(CRIUSupport.java:524)
			Caused by: openj9.internal.criu.JVMCheckpointException: Blocking operation is not allowed in CRIU single thread mode.
				at org.openj9.criu.DeadlockTest.lambda$checkpointDeadlock$1(DeadlockTest.java:93)
				at java.base/openj9.internal.criu.InternalCRIUSupport.lambda$registerCheckpointHookHelper$2(InternalCRIUSupport.java:697)
			*/
			if (!e.getCause().getCause().getMessage().contains("Blocking operation is not allowed in CRIU single thread mode")) {
				testResult.testPassed = false;
				e.printStackTrace();
			}
		}

		if (testResult.testPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}
		System.exit(0);
	}

	public static void notCheckpointSafeDeadlock() {
		Path path = Paths.get("cpData");
		Object lock = new Object();
		final TestResult testResult = new TestResult(true, 0);

		Thread t1 = new Thread(() -> {
			Runnable run = () -> {
				synchronized (lock) {
					testResult.lockStatus.set(1);
					try {
						Thread.sleep(20000);
					} catch (InterruptedException e) {
						e.printStackTrace();
					}

				}
			};

			InternalCRIUSupport.runRunnableInNotCheckpointSafeMode(run);
		});

		t1.start();

		CRIUSupport criuSupport = new CRIUSupport(path);

		while (testResult.lockStatus.get() == 0) {
			Thread.yield();
		}

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(criuSupport, path, true);
			testResult.testPassed = false;
		} catch (JVMCheckpointException e) {
			if (!e.getMessage().contains("The JVM attempted to checkpoint but was unable to due to code being executed")) {
				testResult.testPassed = false;
				e.printStackTrace();
			}
		}

		if (testResult.testPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}
		System.exit(0);
	}

	public static void methodTypeDeadlockTest() {
		Path path = Paths.get("cpData");
		final TestResult testResult = new TestResult(true, 0);
		Runnable run = () -> {
			testResult.lockStatus.incrementAndGet();
			for (int i = 0; i < 30; i++) {
				URL[] urlArray = { A.class.getProtectionDomain().getCodeSource().getLocation() };
				URLClassLoader loader = new URLClassLoader(urlArray);
				byte[] bytes = getClassBytesFromResource(A.class);
				Class clazz = unsafe.defineClass(A.class.getName(), bytes, 0, bytes.length, loader, null);
				MethodType type = MethodType.methodType(clazz);
			}
		};

		Thread threads[] = new Thread[10];
		for (Thread thread : threads) {
			thread = new Thread(run);
			thread.start();
		}

		while (testResult.lockStatus.get() < 5) {
			Thread.yield();
		}

		URL[] urlArray = { A.class.getProtectionDomain().getCodeSource().getLocation() };
		URLClassLoader loader = new URLClassLoader(urlArray);
		byte[] bytes = getClassBytesFromResource(A.class);
		Class clazz = unsafe.defineClass(A.class.getName(), bytes, 0, bytes.length, loader, null);

		CRIUSupport criuSupport = new CRIUSupport(path);
		criuSupport.registerPreCheckpointHook(()->{
			MethodType type = MethodType.methodType(clazz);
		});

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(criuSupport, path, true);
			testResult.testPassed = true;
		} catch (JVMCheckpointException e) {
			testResult.testPassed = false;
			e.printStackTrace();
		}

		if (testResult.testPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}
		System.exit(0);
	}

	static public byte[] getClassBytesFromResource(Class<?> clazz) {
		String className = clazz.getName();
		String classAsPath = className.replace('.', '/') + ".class";
		InputStream classStream = clazz.getClassLoader().getResourceAsStream(classAsPath);
		ByteArrayOutputStream baos = new ByteArrayOutputStream();

		int read;
		byte[] buffer = new byte[16384];
		byte[] result = null;
		try {
			while ((read = classStream.read(buffer, 0, buffer.length)) != -1) {
				baos.write(buffer, 0, read);
			}
			result = baos.toByteArray();
		} catch (IOException e) {
			throw new RuntimeException("Error reading in resource: " + clazz.getName(), e);
		}
		return result;
	}

	public static void clinitTest() {
		Path path = Paths.get("cpData");

		mainTestResult.testPassed = false;
		mainTestResult.lockStatus.set(0);

		Thread t1 = new Thread(()->{
			new ClinitDeadlock();
		});

		t1.start();

		while (mainTestResult.lockStatus.get() == 0) {
			Thread.yield();
		}

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(path);
			mainTestResult.testPassed = false;
		} catch (JVMCheckpointException e) {
			mainTestResult.testPassed = true;
		} finally {
			synchronized(lock) {
				lock.notify();
			}
		}

		if (mainTestResult.testPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}

		System.exit(0);
	}

	public static void clinitTest2() {
		Path path = Paths.get("cpData");

		mainTestResult.testPassed = false;
		mainTestResult.lockStatus.set(0);

		Thread t1 = new Thread(()->{
			new ClinitDeadlock();
		});

		t1.start();

		while (mainTestResult.lockStatus.get() == 0) {
			Thread.yield();
		}

		try {
			System.out.println("Pre-checkpoint");
			CRIUTestUtils.checkPointJVM(path);
			mainTestResult.testPassed = true;
		} catch (JVMCheckpointException e) {
			mainTestResult.testPassed = false;
		} finally {
			synchronized(lock) {
				lock.notify();
			}
		}

		if (mainTestResult.testPassed) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}

		System.exit(0);
	}

	static class ClinitDeadlock {

		static {
			mainTestResult.lockStatus.set(1);
			synchronized(lock) {
				try {
					System.out.println("Thread waiting");
					lock.wait();
				} catch(InterruptedException e) {
					e.printStackTrace();
				}
			}
		}
	}

	static class A {
		int x;
	}

	private static Unsafe unsafe;

	static {
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			unsafe = (Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			e.printStackTrace();
		}
	}
}
