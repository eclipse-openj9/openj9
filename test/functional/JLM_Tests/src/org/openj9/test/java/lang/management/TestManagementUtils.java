/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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
package org.openj9.test.java.lang.management;

import org.openj9.test.util.VersionCheck;
import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.InvalidKeyException;

// This class is not public API.
import com.ibm.java.lang.management.internal.StackTraceElementUtil;

@Test(groups = { "level.sanity" })
public class TestManagementUtils {

	private static Logger logger = Logger.getLogger(TestManagementUtils.class);

	@BeforeClass
	protected void setUp() throws Exception {
		logger.info("Starting TestManagementUtils tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
		// do nothing
	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getClassLoadingBean()'
	 */
	@Test
	public final void testGetClassLoadingBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getMemoryBean()'
	 */
	@Test
	public final void testGetMemoryBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getThreadBean()'
	 */
	@Test
	public final void testGetThreadBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getRuntimeBean()'
	 */
	@Test
	public final void testGetRuntimeBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getOperatingSystemBean()'
	 */
	@Test
	public final void testGetOperatingSystemBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getCompilationBean()'
	 */
	@Test
	public final void testGetCompliationBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getLoggingBean()'
	 */
	@Test
	public final void testGetLoggingBean() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getMemoryManagerMXBeans()'
	 */
	@Test
	public final void testGetMemoryManagerMXBeans() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getMemoryPoolMXBeans()'
	 */
	@Test
	public final void testGetMemoryPoolMXBeans() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getGarbageCollectorMXBeans()'
	 */
	@Test
	public final void testGetGarbageCollectorMXBeans() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.verifyFieldTypes(CompositeData, String[], String[])'
	 */
	@Test
	public final void testVerifyFieldTypes() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.verifyFieldNames(CompositeData, String[])'
	 */
	@Test
	public final void testVerifyFieldNames() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.verifyFieldNumber(CompositeData, int)'
	 */
	@Test
	public final void testVerifyFieldNumber() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toMemoryUsageCompositeData(MemoryUsage)'
	 */
	@Test
	public final void testToMemoryUsageCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getMemoryUsageCompositeType()'
	 */
	@Test
	public final void testGetMemoryUsageCompositeType() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toMemoryNotificationInfoCompositeData(MemoryNotificationInfo)'
	 */
	@Test
	public final void testToMemoryNotificationInfoCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toProcessingCapacityNotificationInfoCompositeData(ProcessingCapacityNotificationInfo)'
	 */
	@Test
	public final void testToProcessingCapacityNotificationInfoCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toTotalPhysicalMemoryNotificationInfoCompositeData(TotalPhysicalMemoryNotificationInfo)'
	 */
	@Test
	public final void testToTotalPhysicalMemoryNotificationInfoCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toAvailableProcessorsNotificationInfoCompositeData(AvailableProcessorsNotificationInfo)'
	 */
	@Test
	public final void testToAvailableProcessorsNotificationInfoCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toThreadInfoCompositeData(ThreadInfo)'
	 */
	@Test
	public final void testToThreadInfoCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toStackTraceElementCompositeData(StackTraceElement)'
	 */
	@Test
	public final void testToStackTraceElementCompositeData() {
		// Null file name
		StackTraceElement st = new StackTraceElement("foo.bar.DeclaringClass", "methodName", null, 42);
		CompositeData cd = StackTraceElementUtil.toCompositeData(st);
		int numValues = (VersionCheck.major() >= 9) ? 8 : 5;

		// Examine the returned CompositeData
		AssertJUnit.assertEquals(numValues, cd.values().size());
		AssertJUnit.assertEquals("foo.bar.DeclaringClass", cd.get("className"));
		AssertJUnit.assertEquals("methodName", cd.get("methodName"));
		AssertJUnit.assertNull(cd.get("fileName"));
		AssertJUnit.assertEquals(42, cd.get("lineNumber"));
		AssertJUnit.assertFalse((Boolean)cd.get("nativeMethod"));
		try {
			AssertJUnit.assertNull(cd.get("madeUpAttribute"));
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof InvalidKeyException);
		}

		// Non-null file name
		st = new StackTraceElement("foo.bar.DeclaringClass", "methodName", "DeclaringClass.java", -1);
		cd = StackTraceElementUtil.toCompositeData(st);

		AssertJUnit.assertEquals(numValues, cd.values().size());
		AssertJUnit.assertEquals("foo.bar.DeclaringClass", cd.get("className"));
		AssertJUnit.assertEquals("methodName", cd.get("methodName"));
		AssertJUnit.assertNotNull(cd.get("fileName"));
		AssertJUnit.assertEquals("DeclaringClass.java", cd.get("fileName"));
		AssertJUnit.assertEquals(-1, cd.get("lineNumber"));
		AssertJUnit.assertFalse((Boolean)cd.get("nativeMethod"));
		try {
			AssertJUnit.assertNull(cd.get("madeUpAttribute"));
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof InvalidKeyException);
		}

		// Native method
		st = new StackTraceElement("foo.bar.DeclaringClass", "methodName", "DeclaringClass.java", -2);
		cd = StackTraceElementUtil.toCompositeData(st);

		// Examine the returned CompositeData
		AssertJUnit.assertEquals(numValues, cd.values().size());
		AssertJUnit.assertEquals("foo.bar.DeclaringClass", cd.get("className"));
		AssertJUnit.assertEquals("methodName", cd.get("methodName"));
		AssertJUnit.assertNotNull(cd.get("fileName"));
		AssertJUnit.assertEquals("DeclaringClass.java", cd.get("fileName"));
		AssertJUnit.assertEquals(-2, cd.get("lineNumber"));
		AssertJUnit.assertTrue((Boolean)cd.get("nativeMethod"));
		try {
			AssertJUnit.assertNull(cd.get("madeUpAttribute"));
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof InvalidKeyException);
		}
	}

	/*
	 * Test method for
	 * 'com.ibm.lang.management.ManagementUtils.convertStringArrayToList(String[])'
	 */
	@Test
	public final void testConvertStringArrayToList() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.convertTabularDataToMap(TabularData)'
	 */
	@Test
	public final void testConvertTabularDataToMap() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.convertFromCompositeData(CompositeData, Class<T>) <T>'
	 */
	@Test
	public final void testConvertFromCompositeData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.convertFromOpenType(Object, Class<?>, Class<T>) <T>'
	 */
	@Test
	public final void testConvertFromOpenType() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.toSystemPropertiesTabularData(Map<String, String>)'
	 */
	@Test
	public final void testToSystemPropertiesTabularData() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.getClassMaybePrimitive(String)'
	 */
	@Test
	public final void testGetClassMaybePrimitive() {
		// TODO Auto-generated method stub

	}

	/*
	 * Test method for 'com.ibm.lang.management.ManagementUtils.isWrapperClass(Class<? extends Object>, Class)'
	 */
	@Test
	public final void testIsWrapperClass() {
		// TODO Auto-generated method stub

	}

}
