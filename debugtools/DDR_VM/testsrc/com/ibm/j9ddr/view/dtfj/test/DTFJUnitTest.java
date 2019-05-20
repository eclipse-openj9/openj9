/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.test;

import static org.junit.Assert.*;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.J9DDRClassLoader;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

public abstract class DTFJUnitTest {
	public static final String COMPARATOR_PACKAGE = "com.ibm.j9ddr.view.dtfj.comparators.";
	
	static ImageFactory ddrImageFactory;
	static ImageFactory jextractImageFactory;
	static JavaRuntime ddrJavaRuntime;
	static JavaRuntime jextractJavaRuntime;
	static ImageAddressSpace ddrAddressSpace;
	static ImageProcess ddrProcess;
	static Image ddrImage;
	static ImageAddressSpace jextractAddressSpace;
	static ImageProcess jextractProcess;
	static Image jextractImage;
	
	static final Map<File, TestContext> jextractContexts = new HashMap<File, TestContext>();
	static final Map<File, TestContext> ddrContexts = new HashMap<File, TestContext>();
	
	static DTFJComparator javaObjectComparator;
	static DTFJComparator javaHeapComparator;
	static DTFJComparator javaStackFrameComparator;
	static DTFJComparator javaClassComparator;
	static DTFJComparator javaClassLoaderComparator;
	static DTFJComparator javaThreadComparator;
	static DTFJComparator javaFieldComparator;
	static DTFJComparator javaMethodComparator;
	static DTFJComparator javaReferenceComparator;
	static DTFJComparator javaMonitorComparator;
	static DTFJComparator javaVMInitArgsComparator;
	static DTFJComparator javaVMOptionComparator;
	static DTFJComparator managedRuntimeComparator;
	static DTFJComparator imageRegisterComparator;
	static DTFJComparator imageStackFrameComparator;
	static DTFJComparator imageModuleComparator;
	static DTFJComparator imageSymbolComparator;
	static DTFJComparator imageComparator;
	static DTFJComparator imageSectionComparator;
	static DTFJComparator imageThreadComparator;
	static DTFJComparator imagePointerComparator;
	static DTFJComparator imageAddressSpaceComparator;
	static DTFJComparator imageProcessComparator;
	
	// DDR Objects to test
	static List<Object> ddrTestObjects;
	
	// Clones of DDR Objects to test.  These should pass equals test
	static List<Object> ddrClonedObjects;

	// jextract reference objects
	static List<Object> jextractTestObjects;
	
	public static class InvalidObjectException extends Exception {
		private static final long serialVersionUID = -4982484667941686491L;
		
		public InvalidObjectException() {
		}

		public InvalidObjectException(String message) {
			super(message);
		}
	}
	
	public static void clearTestObjects() {
		ddrTestObjects = null;
		ddrClonedObjects = null;
		jextractTestObjects = null;
	}
	
	public void initTestObjects() {
		if (ddrTestObjects == null) {
			ddrTestObjects = new ArrayList<Object>();
			ddrClonedObjects = new ArrayList<Object>();
			jextractTestObjects = new ArrayList<Object>();
				
			// Load test objects
			loadTestObjects(ddrJavaRuntime, ddrTestObjects, jextractJavaRuntime, jextractTestObjects);
				
			// Load clone objects
			loadTestObjects(ddrJavaRuntime, ddrClonedObjects, jextractJavaRuntime, new ArrayList<Object>());
		}
		//if both ddr and jextract return 0 objects then skip the test to make sure there are some ddr objects to test
		if(!((ddrTestObjects.size() == 0) && (jextractTestObjects.size() == 0))) {
			assertTrue("No ddr objects to test.  Check implementation of DTFJUnitTest.loadTestObjects()", ddrTestObjects.size() > 0);
		}
	}
	
	
	protected abstract void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects);
	public abstract void setup();

	public static void init() {
		initFactories();
		initComparators();
		clearTestObjects();
	}

	private static void fixClassPath() {
		URL ddrURL = null;
		try {
			String java_home = System.getProperty("java.home", "");
			ddrURL = new File(java_home, "lib/ddr/j9ddr.jar").toURI().toURL();
		} catch (MalformedURLException e) {
			// This can only happen with a typo in source code
			e.printStackTrace();
		}

		ClassLoader loader = ClassLoader.getSystemClassLoader();
		Class<?> loaderClazz = loader.getClass();

		try {
			while (loaderClazz != null && !loaderClazz.getName().equals("java.net.URLClassLoader")) {
				loaderClazz = loaderClazz.getSuperclass();
			}
			
			if (loaderClazz == null) {
				throw new UnsupportedOperationException("Application class loader is not an instance of URLClassLoader.  Can not initialize J9DDR");
			}

			Method addURLMethod = loaderClazz.getDeclaredMethod("addURL", URL.class);
			addURLMethod.setAccessible(true);
			addURLMethod.invoke(loader, ddrURL);
		} catch (SecurityException e) {
			e.printStackTrace();
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
	}

	private static void initFactories() {
		fixClassPath();
		ddrImageFactory = new J9DDRImageFactory();
		Class<?> jxfactoryclass = null;

		try {
			jxfactoryclass = Class.forName("com.ibm.dtfj.image.j9.ImageFactory");
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
			fail("Could not create a jextract based implementation of ImageFactory");
		}

		try {
			jextractImageFactory = (ImageFactory) jxfactoryclass.newInstance();
		} catch (InstantiationException e1) {
			e1.printStackTrace();
			fail("Could not create a jextract based implementation of ImageFactory");
		} catch (IllegalAccessException e1) {
			e1.printStackTrace();
			fail("Could not create a jextract based implementation of ImageFactory");
		}

		try {
			//allow DDR to specify it's own core file so that jextract can use zip files to work around a defect with zip handling on linux
			File coreFile = new File(System.getProperty("ddr.core.file.name", System.getProperty("core")));
			TestContext ctx = getDDRContext(coreFile);
			ddrJavaRuntime = ctx.getRuntime();
			ddrAddressSpace = ctx.getSpace();
			ddrProcess = ctx.getProcess();
			ddrImage = ctx.getImage();
			
			coreFile = new File(System.getProperty("core"));
			ctx = getJExtractContext(coreFile);
			jextractJavaRuntime = ctx.getRuntime();
			jextractAddressSpace = ctx.getSpace();
			jextractProcess = ctx.getProcess();
			jextractImage = ctx.getImage();
		} catch (IOException e) {
			e.printStackTrace();
			fail(String.format("Could not retrieve JavaRuntime: %s", e.getMessage()));
		}
	}

	private static TestContext getJExtractContext(File coreFile) throws IOException {
		TestContext context = jextractContexts.get(coreFile.getAbsoluteFile());

		if (context == null) {
			context = buildContext(jextractImageFactory, coreFile);
			jextractContexts.put(coreFile.getAbsoluteFile(), context);
		}

		return context;
	}

	private static TestContext getDDRContext(File coreFile) throws IOException {
		TestContext context = ddrContexts.get(coreFile.getAbsoluteFile());

		if (context == null) {
			context = buildContext(ddrImageFactory, coreFile);
			ddrContexts.put(coreFile.getAbsoluteFile(), context);
		}

		return context;
	}

	private static void initComparators() {
		try {
			Class<?> clazz;

			J9DDRClassLoader ddrClassLoader = (J9DDRClassLoader) ddrJavaRuntime.getClass().getClassLoader();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageThreadComparator");
			imageThreadComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImagePointerComparator");
			imagePointerComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageAddressSpaceComparator");
			imageAddressSpaceComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageProcessComparator");
			imageProcessComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaObjectComparator");
			javaObjectComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaHeapComparator");
			javaHeapComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageSectionComparator");
			imageSectionComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaStackFrameComparator");
			javaStackFrameComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaClassLoaderComparator");
			javaClassLoaderComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaClassComparator");
			javaClassComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaThreadComparator");
			javaThreadComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaFieldComparator");
			javaFieldComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaMethodComparator");
			javaMethodComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaReferenceComparator");
			javaReferenceComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageRegisterComparator");
			imageRegisterComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageStackFrameComparator");
			imageStackFrameComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ManagedRuntimeComparator");
			managedRuntimeComparator = (DTFJComparator) clazz.newInstance();

			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaMonitorComparator");
			javaMonitorComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaVMInitArgsComparator");
			javaVMInitArgsComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "JavaVMOptionComparator");
			javaVMOptionComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageModuleComparator");
			imageModuleComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageSymbolComparator");
			imageSymbolComparator = (DTFJComparator) clazz.newInstance();
			
			clazz = ddrClassLoader.loadClass(COMPARATOR_PACKAGE + "ImageComparator");
			imageComparator = (DTFJComparator) clazz.newInstance();
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
			fail("Unable to initialize test");
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			fail("Unable to initialize test");
		} catch (InstantiationException e) {
			e.printStackTrace();
			fail("Unable to initialize test");
		}
	}

	private static TestContext buildContext(ImageFactory imageFactory, File coreFile) throws IOException {
		TestContext ctx = new DTFJUnitTest.TestContext();
		Image image = imageFactory.getImage(coreFile);
		ctx.setImage(image);

		Iterator<?> addressSpaceIt = image.getAddressSpaces();
		while (addressSpaceIt.hasNext()) {
			Object asObj = addressSpaceIt.next();
			
			if (asObj instanceof CorruptData) {
				System.err.println("Corrupt AddressSpace returned: " + asObj);
			} else if (asObj instanceof ImageAddressSpace) {
				ImageAddressSpace as = (ImageAddressSpace) asObj;
				ctx.setSpace(as);

				Iterator<?> processIterator = as.getProcesses();

				while (processIterator.hasNext()) {
					Object processObj = processIterator.next();

					if (processObj instanceof CorruptData) {
						System.err.println("Corrupt ImageProcess returned: " + asObj);
					} else if (processObj instanceof ImageProcess) {
						ImageProcess process = (ImageProcess) processObj;
						ctx.setProcess(process);

						Iterator<?> runtimeIterator = process.getRuntimes();

						while (runtimeIterator.hasNext()) {
							Object runtimeObj = runtimeIterator.next();

							if (runtimeObj instanceof CorruptData) {
								System.err.println("Corrupt ImageProcess returned: " + asObj);
							} else if (runtimeObj instanceof JavaRuntime) {
								JavaRuntime runtime = (JavaRuntime) runtimeObj;
								ctx.setRuntime(runtime);
								return ctx;
							} else {
								throw new ClassCastException("Unexpected type from Runtime iterator: " + runtimeObj.getClass() + ": " + runtimeObj);
							}
						}
					} else {
						throw new ClassCastException("Unexpected type from Process iterator: " + processObj.getClass() + ": " + processObj);
					}
				}
			} else {
				throw new ClassCastException("Unexpected type from AddressSpace iterator: " + asObj.getClass() + ": " + asObj);
			}
		}
		throw new RuntimeException("Could not find a Java Runtime");
	}
	
	/**
	 * Fills each list with objects from the respective iterator.
	 * Does not add CorruptData objects from the Iterator
	 * If one iterator returns a CorruptData insures that the other iterator returns a CorruptData
	 * 
	 * Sorts the output if a sort order is provided.  
	 * 
	 * @param ddrList
	 * @param ddrIterator
	 * @param jextractList
	 * @param jextractIterator
	 * @param sortOrder
	 */
	public static void fillLists(List<Object> ddrList, Iterator<?> ddrIterator, List<Object> jextractList, Iterator<?> jextractIterator, Comparator<? super Object> sortOrder) {
		List<Object> ddrObjects = new ArrayList<Object>();
		List<Object> jextractObjects = new ArrayList<Object>();
		int extraDDREntries = 0;

		while (ddrIterator.hasNext()) {
			if (!jextractIterator.hasNext()) {
				while (ddrIterator.hasNext()) {
					ddrIterator.next();
					extraDDREntries++;
				}
				assertEquals("DDR returned more elements than jextract: ", ddrObjects.size(), ddrObjects.size() + extraDDREntries);
			}
			Object ddrObject = ddrIterator.next();
			Object jextractObject = jextractIterator.next();
		
			ddrObjects.add(ddrObject);
			jextractObjects.add(jextractObject);
		}
		
		assertFalse("jextract returned more elements than ddr: " + ddrObjects.size(), jextractIterator.hasNext());

		Object[] ddrObjectsArray = ddrObjects.toArray();
		Object[] jextractObjectsArray = jextractObjects.toArray();

		if (sortOrder != null) {
			Arrays.sort(ddrObjectsArray, sortOrder);
			Arrays.sort(jextractObjectsArray, sortOrder);
		}

		for (int i = 0; i < ddrObjectsArray.length; i++) {
			Object ddrObject = ddrObjectsArray[i];
			Object jextractObject = jextractObjectsArray[i];
			
			if (!(ddrObject instanceof CorruptData) && !(jextractObject instanceof CorruptData)) {
				ddrList.add(ddrObject);
				jextractList.add(jextractObject);
				continue;
			}
			
			if (ddrObject instanceof CorruptData && jextractObject instanceof CorruptData) {
				continue;
			}
			
			fail("If DDR object is a CorruptData then jextract object must also be a CorruptData");
		}
		assertEquals("jextract and ddr returned iterators of different sizes", jextractList.size(), ddrList.size());
	}
	
	/**
	 * Fills each list with a limited number of objects from the respective iterator.
	 * Does not add CorruptData objects from the Iterator
	 * If one iterator returns a CorruptData insures that the other iterator returns a CorruptData
	 * 
	 * Sorts the output if a sort order is provided.  
	 * 
	 * @param ddrList
	 * @param ddrIterator
	 * @param jextractList
	 * @param jextractIterator
	 * @param sortOrder
	 * @param maxCount the maximum number of items to populate the lists with
	 */
	public static void fillLists(List<Object> ddrList, Iterator<?> ddrIterator, List<Object> jextractList, Iterator<?> jextractIterator, Comparator<? super Object> sortOrder, int maxCount) {
		List<Object> ddrObjects = new ArrayList<Object>();
		List<Object> jextractObjects = new ArrayList<Object>();

		while (ddrIterator.hasNext()) {
			assertTrue("DDR returned more elements than jextract", jextractIterator.hasNext());
			Object ddrObject = ddrIterator.next();
			Object jextractObject = jextractIterator.next();

			ddrObjects.add(ddrObject);
			jextractObjects.add(jextractObject);
		}

		assertFalse("jextract returned more elements than ddr", jextractIterator.hasNext());

		Object[] ddrObjectsArray = ddrObjects.toArray();
		Object[] jextractObjectsArray = jextractObjects.toArray();

		if (sortOrder != null) {
			Arrays.sort(ddrObjectsArray, sortOrder);
			Arrays.sort(jextractObjectsArray, sortOrder);
		}

		for (int i = 0; (i < ddrObjectsArray.length) && (i < maxCount); i++) {
			Object ddrObject = ddrObjectsArray[i];
			Object jextractObject = jextractObjectsArray[i];

			if (!(ddrObject instanceof CorruptData) && !(jextractObject instanceof CorruptData)) {
				ddrList.add(ddrObject);
				jextractList.add(jextractObject);
				continue;
			}

			if (ddrObject instanceof CorruptData && jextractObject instanceof CorruptData) {
				continue;
			}
			
			fail("If DDR object is a CorruptData then jextract object must also be a CorruptData");
		}
		assertEquals("jextract and ddr returned iterators of different sizes", jextractList.size(), ddrList.size());
	}
	
	private static class TestContext {
		private ImageAddressSpace space;
		private ImageProcess process;
		private JavaRuntime runtime;
		private Image image;
		public ImageAddressSpace getSpace() {
			return space;
		}
		public void setImage(Image image) {
			this.image = image;
		}
		public Image getImage() {
			return image;
		}
		public void setSpace(ImageAddressSpace space) {
			this.space = space;
		}
		public ImageProcess getProcess() {
			return process;
		}
		public void setProcess(ImageProcess process) {
			this.process = process;
		}
		public JavaRuntime getRuntime() {
			return runtime;
		}
		public void setRuntime(JavaRuntime runtime) {
			this.runtime = runtime;
		}
	}

	protected static final void equalsTest(List<Object> ddrList, List<Object> ddrClonedList, List<Object> jextractList) {
		equalsTest(ddrList, ddrClonedList, jextractList, false);
	}
	
	protected static final void equalsTest(List<Object> ddrList, List<Object> ddrClonedList, List<Object> jextractList, boolean supressCloneTest) {
		
		// Test veracity of test.
		assertNotNull("ddr list is null", ddrList);
		assertNotNull("jextract list is null", jextractList);
		assertEquals("ddr list size different from the jextract list size", ddrList.size(), jextractList.size());
	
		// Need to suppress this test if dealing with cached DTFJ objects
		if (!supressCloneTest) {
			assertNotNull("ddr cloned list is null", ddrClonedList);
			assertEquals("ddr list size different from the ddr cloned list size", ddrList.size(), ddrClonedList.size(), Math.abs(ddrList.size() - ddrClonedList.size()));
			// Make sure the original and clone are NOT the same Object  (No reason they should be ... but can't be too careful)
			for (int i = 0; i < ddrList.size(); i++) {
				assertNotSame(ddrList.get(i), ddrClonedList.get(i));
			}
		}

		// Equals test
		assertEquals(ddrClonedList, ddrList);
		
		// DTFJ objects can never be equal to DTF objects created by a different ImageFactory
		for (int i = 0; i < ddrList.size(); i++) {
			assertFalse(String.format("Element [%s] - DTFJ objects can never be equal to DTF objects created by a different ImageFactory", i), ddrList.get(i).equals(jextractList.get(i)));
		}
	}
	
	protected static final void hashCodeTest(List<Object> ddrList, List<Object> ddrCloneList) {
		// Test veracity of test.
		assertNotNull("ddr list is null", ddrList);
		assertNotNull("ddr cloned list is null", ddrCloneList);
		assertEquals("ddr list size different from the ddr cloned list size", ddrList.size(), ddrCloneList.size());

		for (int i = 0; i < ddrList.size(); i++) {
			assertEquals(ddrCloneList.get(i).hashCode(), ddrList.get(i).hashCode());
		}
	}

	public static final String getVMoption(JavaRuntime rt, String optionName) {
		try {
			JavaVMInitArgs args = rt.getJavaVMInitArgs();
			Iterator<?> options = args.getOptions();
			while (options.hasNext()) {
				JavaVMOption option = (JavaVMOption) options.next();
				String data = option.getOptionString();
				if (data.startsWith(optionName)) {
					return data.substring(optionName.length());
				}
			}
		} catch (Exception e) {
			throw new IllegalArgumentException("Cannot determine the value of " + optionName, e);
		}
		return "default"; //no policy specified so using the default
	}

}
