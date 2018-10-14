/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ClassLoadingMXBean;
import java.lang.management.ManagementFactory;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Map;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;

/**
 * @brief Tests the implementation of the class jlm.ClassLoadingMXBean.
 * The class has getters for 4 fields: LoadedClassCount, TotalLoadedClassCount, UnloadedClassCount, and Verbose,
 * whereas, only a single setter, that for Verbose.  In addition, it inherits the ObjectName field.
 */
@Test(groups = { "level.sanity" })
public class TestClassLoadingMXBean {

	private static Logger logger = Logger.getLogger(TestClassLoadingMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("Verbose", new AttributeData(Boolean.TYPE.getName(), true, true, true));
		attribs.put("LoadedClassCount", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("TotalLoadedClassCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("UnloadedClassCount", new AttributeData(Long.TYPE.getName(), true, false, false));
	}// end static initializer

	private ClassLoadingMXBean clb;
	private MBeanServer mbs;
	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		clb = ManagementFactory.getClassLoadingMXBean();
		try {
			objName = new ObjectName(ManagementFactory.CLASS_LOADING_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up ClassLoadingMXBean test: " + me.getMessage());
		}
		logger.info("Starting ClassLoadingMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testGetLoadedClassCount() {
		AssertJUnit.assertTrue(clb.getLoadedClassCount() > -1);
		logger.debug("Number of loaded classes : " + clb.getLoadedClassCount());
	}

	@Test
	public final void testGetTotalLoadedClassCount() {
		AssertJUnit.assertTrue(clb.getTotalLoadedClassCount() > -1);
		logger.debug("Total number of loaded classes : " + clb.getTotalLoadedClassCount());
	}

	@Test
	public final void testGetUnloadedClassCount() {
		AssertJUnit.assertTrue(clb.getUnloadedClassCount() > -1);
		logger.debug("Total number of unloaded classes : " + clb.getUnloadedClassCount());
	}

	@Test
	public final void testSetVerbose() {
		boolean initialVal = clb.isVerbose();
		clb.setVerbose(!initialVal);
		AssertJUnit.assertTrue(clb.isVerbose() != initialVal);
		clb.setVerbose(initialVal);
		AssertJUnit.assertTrue(clb.isVerbose() == initialVal);
	}

	@Test
	public final void testGetAttribute() {
		// The good attributes...
		try {
			/* Obtain the attributes the bean exposes, through the MBeanServer instance,
			 * initialized above by passing it the bean's type string.
			 */
			AssertJUnit.assertTrue(((Integer)(mbs.getAttribute(objName, "LoadedClassCount"))).intValue() > -1);
			AssertJUnit.assertTrue(((Long)(mbs.getAttribute(objName, "TotalLoadedClassCount"))).longValue() > -1);
			AssertJUnit.assertTrue(((Long)(mbs.getAttribute(objName, "UnloadedClassCount"))).longValue() > -1);
			// This could be true or false - just so long as we don't get an
			// exception raised...
			boolean verboseVal = ((Boolean)(mbs.getAttribute(objName, "Verbose")));
		} catch (AttributeNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected AttributeNotFoundException: " + e.getMessage());
		} catch (MBeanException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected MBeanException: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = ((Long)(mbs.getAttribute(objName, "RPM")));
			Assert.fail("Unreacheable code: should have thrown an AttributeNotFoundException.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (javax.management.AttributeNotFoundException e1) {
			// This exception is expected.
			logger.debug("AttributeNotFoundException thrown, as expected.");
		} catch (Exception e) {
			Assert.fail("Unexpected Exception occurred: " + e.getMessage());
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "TotalLoadedClassCount"));
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (InstanceNotFoundException e) {
			// Unlikely exception
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// Unlikely exception
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (java.lang.ClassCastException e2) {
			// This exception is expected.
			logger.debug("Exception occurred, as expected: " + "attempting to perform invalid type cast on attribute.");
		} catch (Exception e) {
			Assert.fail("Unexpected Exception occurred: " + e.getMessage());
		}
	}

	@Test
	public final void testSetAttribute() {
		Attribute attr = null;
		try {
			// The one writable attribute of ClassLoadingMXBean...
			Boolean before = (Boolean)mbs.getAttribute(objName, "Verbose");
			// Toggle the boolean state of "isVerbose"
			boolean newVal = !before;
			attr = new Attribute("Verbose", new Boolean(newVal));
			mbs.setAttribute(objName, attr);
			Boolean after = (Boolean)mbs.getAttribute(objName, "Verbose");
			assert (newVal == after);
		} catch (AttributeNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected AttributeNotFoundException " + e.getMessage());
		} catch (InvalidAttributeValueException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InvalidAttributeValueException " + e.getMessage());
		} catch (MBeanException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected MBeanException " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException " + e.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		}

		// Let's try and set some non-writable attributes.
		attr = new Attribute("LoadedClassCount", new Integer(25));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + e1.getMessage());
		}

		attr = new Attribute("TotalLoadedClassCount", new Long(3300));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + e1.getMessage());
		}

		attr = new Attribute("UnloadedClassCount", new Long(38));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + e1.getMessage());
		}

		// Try and set the Verbose attribute with an incorrect type.
		attr = new Attribute("Verbose", new Long(42));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (Exception e2) {
			AssertJUnit.assertTrue(e2 instanceof javax.management.InvalidAttributeValueException);
			logger.debug("Exception occurred, as expected: " + e2.getMessage());
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList list = null;
		try {
			list = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(list);
		AssertJUnit.assertTrue(list.size() == attribs.size());
		AssertJUnit.assertTrue(attribs.size() == 4);
		Iterator<?> it = list.iterator();
		logger.debug("Attributes in ClassLoadingMXBean...");
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			String name = element.getName();
			Object value = element.getValue();
			logger.debug("Attribute name: " + name + ", Value: " + value);

			//It isn't reasonable to assert that the same values are returned via different paths.
			//It might be reasonable to assert that each of those metrics are non-decreasing over time.
			try {
				if (name.equals("Verbose")) {
					AssertJUnit.assertTrue(((Boolean)value).booleanValue() == clb.isVerbose());
				} else if (name.equals("LoadedClassCount")) {
					AssertJUnit.assertTrue(((Integer)value).intValue() > -1);
				} else if (name.equals("TotalLoadedClassCount")) {
					AssertJUnit.assertTrue(((Long)value).longValue() > -1);
					AssertJUnit.assertTrue(((Long)value).longValue() <= clb.getTotalLoadedClassCount());
				} else if (name.equals("UnloadedClassCount")) {
					AssertJUnit.assertTrue(((Long)value).longValue() > -1);
					AssertJUnit.assertTrue(((Long)value).longValue() <= clb.getUnloadedClassCount());
				} else {
					Assert.fail("Unexpected attribute name returned!");
				}
			} catch (Exception e) {
				Assert.fail("Unexpected exception occurred: " + e.getMessage());
			}
		}

		// A failing scenario - pass in an attribute that is not part of
		// the management interface.
		String[] badNames = { "Cork", "Galway" };
		try {
			list = mbs.getAttributes(objName, badNames);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(list);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(list.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// Ideal scenario...
		AttributeList attList = new AttributeList();
		Attribute verbose = new Attribute("Verbose", new Boolean(false));
		attList.add(verbose);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, attList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 1);
		AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getName().equals("Verbose"));

		// A failure scenario - a non-existent attribute...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Bantry", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Another failure scenario - a non-writable attribute...
		badList = new AttributeList();
		garbage = new Attribute("TotalLoadedClassCount", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Yet another failure scenario - a wrongly-typed attribute...
		badList = new AttributeList();
		garbage = new Attribute("Verbose", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testInvoke() {
		// ClassLoadingMXBean has no operations to invoke...
		try {
			Object retVal = mbs.invoke(objName, "KissTheBlarney", new Object[] { new Long(7446), new Long(54) },
					new String[] { "java.lang.Long", "java.lang.Long" });
			// An exception other then InstanceNotFoundException should have been thrown,
			// since there aren't any operations and we just tried to invoke one.
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof ReflectionException);
			logger.debug("Exception occurred, as expected: " + e.getMessage());
		}
	}

	@Test
	public final void testGetMBeanInfo() {
		MBeanInfo mbi = null;
		try {
			mbi = mbs.getMBeanInfo(objName);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (IntrospectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected IntrospectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		/* Check the class name. Test substring match (not equals()) since clb.getClass().getName()
		 * typically returns a fully qualified name (prefixing package name).  This should have always
		 * been prefixed by package name, and not being changed as an aftermath of current changes
		 */
		AssertJUnit.assertTrue(clb.getClass().getName().endsWith(mbi.getClassName()));

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertTrue(constructors.length == 0);

		// No public operations
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertTrue(operations.length == 0);

		// No notifications
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertTrue(notifications.length == 0);

		/* Print out the description and class name as they are not bound to be
		 * the same.
		 */
		logger.debug("MBean description for " + clb.getClass().getName() + ": " + mbi.getDescription());

		// Four attributes - only Verbose is writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.length == 5);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestClassLoadingMXBean.ignoredAttributes, attribs);
		} // end for
	}
}

/**
 * Helper class
 */

class AttributeData {
	final String type;

	final boolean readable;

	final boolean writable;

	final boolean isAccessor;

	AttributeData(String type, boolean readable, boolean writable, boolean isAccessor) {
		this.type = type;
		this.readable = readable;
		this.writable = writable;
		this.isAccessor = isAccessor;
	}
}
