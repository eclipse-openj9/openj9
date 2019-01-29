/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;

import java.io.IOException;
import java.io.InputStream;
import java.lang.StackWalker.Option;
import java.lang.StackWalker.StackFrame;
import java.lang.module.ModuleDescriptor;
import java.lang.module.ModuleDescriptor.Version;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.Module;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;

import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.extended" })
@SuppressWarnings("nls")
public class StackWalkerTest {
	static final String TEST_MYLOADER_INSTANCE = "TestMyloaderInstance";
	protected static Logger logger = Logger.getLogger(StackWalkerTest.class);
	private String testName;

	@Test
	public void testSanity() {
		sanityTest(StackWalker.getInstance());
	}

	@Test
	public void testOptions() {
		Option[] valueList = Option.values();
		assertEquals(valueList.length, 3, "wrong number of values");
		for (Option opt: valueList) {
			Option val = Option.valueOf(opt.toString());
			logMessage("Option: "+opt.toString()+" value="+val);
		}
	}

	@Test
	public void testForEach() {
		StackWalker myWalker = StackWalker.getInstance();
		final HashSet<String> methodNames = new HashSet<>();
		childMethod1(myWalker, methodNames);
		myWalker.forEach(f -> methodNames.add(f.getMethodName()));
		assertTrue(methodNames.contains("childMethod1"), "childMethod1 missing");
		assertTrue(methodNames.contains("childMethod2"), "childMethod2 missing");
		methodNames.clear();
		myWalker.forEach(f -> methodNames.add(f.getMethodName()));
		assertFalse(methodNames.contains("childMethod1"), "childMethod1 missing");
		assertFalse(methodNames.contains("childMethod2"), "childMethod2 missing");
	}

	@Test
	public void testGetCallerClass() {
		Class<?> callerClass = CallerClassTester.doGetCallerClass();
		logMessage("caller class = "+callerClass.getName());
		assertEquals(callerClass, this.getClass(), "caller class wrong");
	}

	@Test
	public void testGetInstance() {
		StackWalker myWalker = StackWalker.getInstance();
		sanityTest(myWalker);
	}

	@Test
	public void testStackTop() {
		StackWalker myWalker = StackWalker.getInstance();
		StackFrame result = myWalker.walk(s -> s.findFirst().orElse(null));
	}

	void sanityTest(StackWalker myWalker) {
		logMessage("Methods from getStackTrace");
		for (StackTraceElement e: Thread.currentThread().getStackTrace()) {
			String fullMethodNameFromElement = getFullMethodNameFromElement(e);
			logMessage(fullMethodNameFromElement);
		}
		List<StackFrame> frameList = myWalker.walk(s -> s.collect(Collectors.toList()));
		logMessage("Methods from StackWalker");
		for (StackFrame f: frameList) {
			logMessage(getFullMethodNameFromFrame(f)+(f.isNativeMethod() ? "(native)": ""));
		}
		logMessage("---------------------");
		HashSet<String> nativeElements = new HashSet<>();
		logMessage("Native methods from getStackTrace");
		for (StackTraceElement e: Thread.currentThread().getStackTrace()) {
			String fullMethodNameFromElement = getFullMethodNameFromElement(e);
			if (e.isNativeMethod()  && !fullMethodNameFromElement.equals("java.lang.Thread.getStackTraceImpl")  && !isInvokeFrame(e)) {
				logMessage(fullMethodNameFromElement);
				nativeElements.add(fullMethodNameFromElement);
			}
		}
		logMessage("Native methods from StackWalker");
		List<StackFrame> nativeMethodList = StackWalker.getInstance().walk(s -> s.filter(f ->f.isNativeMethod()).collect(Collectors.toList()));
		for (StackFrame f: nativeMethodList) {
			String methodName = getFullMethodNameFromFrame(f);
			logMessage(methodName);
		}
		assertEquals(nativeMethodList.size(), nativeElements.size(), "wrong number of native frames");
		for (StackFrame f: nativeMethodList) {
			String methodName = getFullMethodNameFromFrame(f);
			assertTrue(nativeElements.contains(methodName), methodName+"  missing");
		}
	}

	@Test
	public void testGetInstanceStackWalkerOption() {

		boolean exceptionThrown = false;
		try {
			StackWalker.getInstance().getCallerClass();
		} catch (UnsupportedOperationException e) {
			logMessage(e.getMessage()+" thrown");
			exceptionThrown = true;
		}
		assertTrue(exceptionThrown, "Expected UnsupportedOperationException not thrown");
		try {
			logMessage("get caller class");
			StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass();
		} catch (Exception e) {
			fail("Unexpected exception "+e);
		}

	}

	@Test
	public void testGetInstanceSetStackWalkerOption() {
		Set<String> methodNames = StackWalker.getInstance().walk(s -> s.map(t->getFullMethodNameFromFrame(t)).collect(Collectors.toSet()));
		assertFalse(methodNames.contains("java.lang.reflect.Method.invoke"), "java.lang.reflect.Method.invoke not removed");
		HashSet<Option> opts = new HashSet<>();
		opts.add(Option.SHOW_REFLECT_FRAMES);
		methodNames = StackWalker.getInstance(opts).walk(s -> s.map(t->getFullMethodNameFromFrame(t)).collect(Collectors.toSet()));
		assertTrue(methodNames.contains("java.lang.reflect.Method.invoke"), "reflection methods not included");
	}

	@Test
	public void testGetInstanceSetStackWalkerOptionint() {
		HashSet<Option> opts = new HashSet<>();
		opts.add(Option.SHOW_HIDDEN_FRAMES);
		Set<String> methodNames = StackWalker.getInstance(opts, 27).walk(s -> s.map(t->getFullMethodNameFromFrame(t)).collect(Collectors.toSet()));
		assertTrue(methodNames.contains("java.lang.reflect.Method.invoke"), "reflection methods not included");
	}

	@Test
	public void testThreads() {
		StackWalker myWalker = StackWalker.getInstance();
		Thread testThreads[] = {new ThreadTester1(myWalker), new ThreadTester2(myWalker), new ThreadTester1(myWalker), new ThreadTester2(myWalker)};
		for(Thread t: testThreads) {
			logMessage("Starting thread");
			t.start();
		}
		for(Thread t: testThreads) {
			try {
				logMessage("Joining thread");
				t.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
				fail("Unexpected exception "+e);
			}
		}
	}

	@Test
	public void testGetCallerClassUsingReflection() {
		try {
			Method getCallerMeth = StackWalker.class.getMethod("getCallerClass", null);

			Class<?> callerClass = CallerClassTester.doGetCallerClass(getCallerMeth);
			logMessage("caller class = "+callerClass.getName());
			assertEquals(callerClass, this.getClass(), "caller class wrong");
		} catch (NoSuchMethodException | SecurityException e) {
			e.printStackTrace();
			fail("unexpected exception");
		}
	}

	@Test
	public void testRecursiveStackWalk() {
		final StackWalker walker = StackWalker.getInstance();
		walker.forEach(s1-> {logMessage(s1.getMethodName()+" do recursive walk"); walker.forEach(s2->logMessage(s2.getMethodName()));logMessage("--------------------");});
	}

	static class CallerClassTester {
		static Class<?> doGetCallerClass() {
			return StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass();
		}

		public static Class<?> doGetCallerClass(Method getCallerMeth) {
			try {
				StackWalker instance = StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE);
				Class<?> result = (Class<?>) getCallerMeth.invoke(instance, null);
				return result;
			} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
				e.printStackTrace();
				fail("unexpected exception");
				return null;
			}
		}
	}
	
	@Test
	public void testClassLoaderName() {
		ClassLoader myLoaderInstance = new MyLoader(TEST_MYLOADER_INSTANCE, ClassLoader.getSystemClassLoader());
		try {
			Class<?> klasse = Class.forName(getClass().getPackageName()+".MyRunnable", true,	myLoaderInstance);
			assertEquals(myLoaderInstance.getName(), TEST_MYLOADER_INSTANCE, "incorrect classLoader name");
			Runnable r = (Runnable) klasse.newInstance();
			r.run();
		} catch (ClassNotFoundException | IllegalAccessException | InstantiationException e) {
			fail("unexpected exception", e);
		}
	}

	@Test
	public void testModule() {
			Thread t = new Thread(()->dumpModules());  /* force a class from java.base to be on the stack */
			t.start();
	}

	private void dumpModules() {
		HashSet<Option> opts = new HashSet<>(Arrays.asList(new Option[] {Option.RETAIN_CLASS_REFERENCE, Option.SHOW_HIDDEN_FRAMES, Option.SHOW_REFLECT_FRAMES}));
		List<StackFrame> frameList = (StackWalker.getInstance(opts)).walk(s -> s.collect(Collectors.toList()));
		for (StackFrame f: frameList)  {
			Module frameModule = f.getDeclaringClass().getModule();

			if ((null != frameModule)) {
				ModuleDescriptor d = frameModule.getDescriptor();
				if (null != d) {
					StackTraceElement e = f.toStackTraceElement();
					String modName = d.name();
					String expectedName = e.getModuleName();
					if (null != modName) {
						assertEquals(modName, expectedName, "Wrong module name");
					}
					Optional<Version> modVersion = d.version();
					if (modVersion.isPresent()) {
						String actualVersion = modVersion.get().toString();
						String expectedVersion = e.getModuleVersion();
						assertEquals(actualVersion, expectedVersion, "Wrong module version");
					}
					if (f.getDeclaringClass() == Thread.class) {
						assertEquals("java.base", expectedName, "Wrong module name");
					}
				}
			}
		}
	}
	
	void childMethod1(StackWalker myWalker, HashSet<String> methodNames) {
		childMethod2(myWalker, methodNames);
	}
	private void childMethod2(StackWalker myWalker, HashSet<String> methodNames) {
		myWalker.forEach(f -> methodNames.add(f.getMethodName()));
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		logMessage("\n------------------------------------------\nstarting " + testName);
	}

	private boolean isInvokeFrame(StackTraceElement e) {
		boolean isInvoke = e.getMethodName().startsWith("invoke") && e.getClassName().contains("reflect");
		return isInvoke;
	}

	void logMessage(String message) {
		logger.debug(message);
	}

	private String getFullMethodNameFromFrame(StackFrame f) {
		return f.getClassName()+"."+f.getMethodName();
	}

	private String getFullMethodNameFromElement(StackTraceElement e) {
		return e.getClassName()+"."+e.getMethodName();
	}
	class ThreadTester1 extends Thread {
		StackWalker myWalker;
		ThreadTester1(StackWalker myWalker) {
			this.myWalker = myWalker;
		}
		@Override
		public void run() {
			doTest1(myWalker);
		}
		private void doTest1(StackWalker walker) {
			sanityTest(walker);
		}	
	}
	class ThreadTester2 extends Thread {
		StackWalker myWalker;
		ThreadTester2(StackWalker myWalker) {
			this.myWalker = myWalker;
		}
		@Override
		public void run() {
			doTest2(myWalker);
		}
		private void doTest2(StackWalker walker) {
			sanityTest(walker);
		}	
	}

}

class MyLoader extends ClassLoader {

	public MyLoader(String name, ClassLoader parent) {
		super(name, parent);
	}

	@Override
	protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
		if (!name.contains("MyRunnable")) {
			return getParent().loadClass(name);
		}
		try {
			String resName = name.replaceAll("\\.", "/")+".class";
			final int MAXFILESIZE = 100000;
			InputStream s = getSystemClassLoader().getResourceAsStream(resName);
			byte[] classFileBytes = new byte[MAXFILESIZE];
			int nRead = 0;
			nRead = s.read(classFileBytes);
			Class<?> k = defineClass(name, classFileBytes, 0, nRead);
			return k;
		} catch (IOException e) {
			throw new ClassNotFoundException(name);
		}
	}

}
