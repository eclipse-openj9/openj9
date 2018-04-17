/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.invoker;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Hashtable;

import org.openj9.test.invoker.util.DummyClassGenerator;
import org.openj9.test.invoker.util.GeneratedClassLoader;
import org.openj9.test.invoker.util.InterfaceGenerator;
import org.openj9.test.invoker.util.ClassGenerator;
import com.ibm.jvmti.tests.redefineClasses.rc001;

@Test(groups = { "level.sanity" })
public class MutateSplitTableRetransformationTest {
	public static Logger logger = Logger.getLogger(MutateSplitTableRetransformationTest.class);

	private static final String PKG_NAME = "org/openj9/test/invoker";
	private static final int NUM_CLASS_VERSIONS = 7;

	String className = PKG_NAME + "/" + ClassGenerator.DUMMY_CLASS_NAME;
	String intfClassName = PKG_NAME + "/" + ClassGenerator.INTERFACE_NAME;

	private ArrayList<GeneratedClassDef> dummyClassVersions = new ArrayList<GeneratedClassDef>();
	private ArrayList<GeneratedClassDef> interfaceVersions = new ArrayList<GeneratedClassDef>();

	@BeforeMethod
	protected void setUp() throws Exception {
		String directory = "testMutateSplitTables";
		/* get class bytes of all the versions of the test class and interface */
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		byte[] classData = null;
		InterfaceGenerator intfGenerator = new InterfaceGenerator(directory, PKG_NAME);
		DummyClassGenerator dummyClassGenerator = new DummyClassGenerator(directory, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		intfGenerator.setCharacteristics(characteristics);
		classData = intfGenerator.getClassData();
		interfaceVersions.add(new GeneratedClassDef(intfClassName, "V1", classData));

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, true);
		intfGenerator.setCharacteristics(characteristics);
		classData = intfGenerator.getClassData();
		interfaceVersions.add(new GeneratedClassDef(intfClassName, "V2", classData));

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V3");
		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, true);
		intfGenerator.setCharacteristics(characteristics);
		classData = intfGenerator.getClassData();
		interfaceVersions.add(new GeneratedClassDef(intfClassName, "V3", classData));

		for (int i = 1; i <= NUM_CLASS_VERSIONS; i++) {
			String invokers = "";
			if (0x1 == (i & 0x1)) {
				invokers = invokers + "I";
			}
			if (0x2 == (i & 0x2)) {
				invokers = invokers + "S";
			}
			if (0x4 == (i & 0x4)) {
				invokers = invokers + "N";
			}
			/*
			 * class version format is M<num_invokers>_<invoker_list> e.g. M1_S,
			 * M2_IS, M3_ISN and so on
			 */
			String classVersion = "M" + invokers.length() + "_" + invokers;

			characteristics.clear();
			characteristics.put(ClassGenerator.VERSION, classVersion);
			characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
			/*
			 * Generate shared invokers based on invokers encoded in class
			 * version
			 */
			String sharedInvokers[] = new String[invokers.length()];
			for (int j = 0; j < invokers.length(); j++) {
				switch (invokers.charAt(j)) {
				case 'I':
					sharedInvokers[j] = DummyClassGenerator.INVOKE_INTERFACE;
					break;
				case 'S':
					sharedInvokers[j] = DummyClassGenerator.INVOKE_STATIC;
					break;
				case 'N':
					sharedInvokers[j] = DummyClassGenerator.INVOKE_SPECIAL;
					break;
				}
			}
			characteristics.put(ClassGenerator.SHARED_INVOKERS, sharedInvokers);
			dummyClassGenerator.setCharacteristics(characteristics);
			classData = dummyClassGenerator.getClassData();
			dummyClassVersions.add(new GeneratedClassDef(className, classVersion, classData));
		}
	}

	private void callInvokerMethodsAndVerifyResult(Class<?> dummyClass, GeneratedClassDef dummyClassDef,
			GeneratedClassDef intfDef) throws Exception {
		String intfVersion = intfDef.getVersion();
		String dummyClassVersion = dummyClassDef.getVersion();
		/*
		 * class version format is M<num_invokers>_<invoker_list> e.g. M1_S,
		 * M2_IS, M3_ISN and so on
		 */
		int numInvokers = Integer.valueOf(dummyClassVersion.substring(1, 2));
		String invokerList = dummyClassVersion.substring(3);
		String rc = null;
		String expected = null;
		Method method = null;

		for (int i = 0; i < numInvokers; i++) {
			expected = null;

			switch (invokerList.charAt(i)) {
			case 'I':
				method = dummyClass.getMethod(DummyClassGenerator.INVOKE_INTERFACE, (Class[])null);
				if (intfVersion.equals("V1") || intfVersion.equals("V2")) {
					/* interface has abstract or default method */
					expected = ClassGenerator.DUMMY_CLASS_NAME + "_" + dummyClassVersion + "."
							+ ClassGenerator.INTF_METHOD_NAME;
				}
				if (expected == null) {
					logger.debug("Calling invokeInterface(). Expected to throw exception.");
				} else {
					logger.debug("Calling invokeInterface(). Expected to return: " + expected);
				}
				break;
			case 'S':
				method = dummyClass.getMethod(DummyClassGenerator.INVOKE_STATIC, (Class[])null);
				if (intfVersion.equals("V3")) {
					/* interface has static method */
					expected = "Static " + ClassGenerator.INTERFACE_NAME + "_" + intfVersion + "."
							+ ClassGenerator.INTF_METHOD_NAME;
				}
				if (expected == null) {
					logger.debug("Calling invokeStatic(). Expected to throw exception.");
				} else {
					logger.debug("Calling invokeStatic(). Expected to return: " + expected);
				}
				break;
			case 'N':
				method = dummyClass.getMethod(DummyClassGenerator.INVOKE_SPECIAL, (Class[])null);
				if (intfVersion.equals("V2")) {
					/* interface has default method */
					expected = "Default " + ClassGenerator.INTERFACE_NAME + "_" + intfVersion + "."
							+ ClassGenerator.INTF_METHOD_NAME;
				}
				if (expected == null) {
					logger.debug("Calling invokeSpecial(). Expected to throw exception.");
				} else {
					logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
				}
				break;
			}
			try {
				rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
				logger.debug("Returned: " + rc);
				if (expected == null) {
					/*
					 * above call is expected to throw exception but it didn't,
					 * so fail
					 */
					Assert.fail("Expected to throw exception but returned: " + rc);
				} else {
					AssertJUnit.assertEquals("Did not get expected value", expected, rc);
				}
			} catch (Exception e) {
				if (expected != null) {
					Assert.fail("Did not expect exception to be thrown", e);
				} else {
					/* exception is expected */
					logger.debug("Expected exception caught");
				}
			}
		}
	}

	@Test
	public void testMutateSplitTables() throws Exception {
		/* for each interface version */
		for (int i = 0; i < interfaceVersions.size(); i++) {
			/* for each class version */
			for (int j = 0; j < dummyClassVersions.size(); j++) {
				for (int k = 0; k < dummyClassVersions.size(); k++) {
					if (j == k) {
						continue;
					}
					GeneratedClassLoader classLoader = new GeneratedClassLoader();

					logger.debug("Interface version used: " + ClassGenerator.INTERFACE_NAME + "_"
							+ interfaceVersions.get(i).getVersion());
					classLoader.setClassData(interfaceVersions.get(i).getClassData());
					Class.forName(intfClassName.replace('/', '.'), true, classLoader);
					/* Load class at dummyClassVersions.get(i) */
					logger.debug("Initial Dummy Class version: " + ClassGenerator.DUMMY_CLASS_NAME + "_"
							+ dummyClassVersions.get(j).getVersion());
					classLoader.setClassData(dummyClassVersions.get(j).getClassData());
					Class<?> dummyClass = Class.forName(className.replace('/', '.'), true, classLoader);

					/* Call invoker methods */
					callInvokerMethodsAndVerifyResult(dummyClass, dummyClassVersions.get(j), interfaceVersions.get(i));

					logger.debug("Retransforming Dummy Class to: " + ClassGenerator.DUMMY_CLASS_NAME + "_"
							+ dummyClassVersions.get(k).getVersion());
					/* Retransform to dummyClassVersions.get(j) */
					byte[] redefinedClassData = dummyClassVersions.get(k).getClassData();
					rc001.redefineClass(dummyClass, redefinedClassData.length, redefinedClassData);

					/* Call invoker methods */
					callInvokerMethodsAndVerifyResult(dummyClass, dummyClassVersions.get(k), interfaceVersions.get(i));
				}
			}
		}
	}

	class GeneratedClassDef {
		private String className;
		private String version;
		private byte[] classData;

		public GeneratedClassDef(String name, String version, byte[] classData) {
			className = name;
			this.version = version;
			this.classData = classData;
		}

		public String getClassName() {
			return className;
		}

		public String getVersion() {
			return version;
		}

		public byte[] getClassData() {
			return classData;
		}
	}
}
