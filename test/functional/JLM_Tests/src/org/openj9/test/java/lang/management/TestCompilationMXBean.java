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
import java.lang.management.CompilationMXBean;
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
 * @brief Test the CompilationMXBeanImpl implementation by IBM.  Make sanity checks and also, where
 * possible, validate the results against alternatives.
 */
@Test(groups = { "level.sanity" })
public class TestCompilationMXBean {

	private static Logger logger = Logger.getLogger(TestCompilationMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("TotalCompilationTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CompilationTimeMonitoringSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
	}// end static initializer

	private CompilationMXBean cb;
	private MBeanServer mbs;
	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		cb = ManagementFactory.getCompilationMXBean();
		try {
			objName = new ObjectName(ManagementFactory.COMPILATION_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up CompilationMXBean test: " + me.getMessage());
		}
		logger.info("Starting CompilationMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testGetName() {
		AssertJUnit.assertNotNull(cb.getName());
	}

	/* Test API getTotalCompilationTime(), both, when it is supported and when it isn't. */
	@Test
	public final void testGetTotalCompilationTime() {
		if (cb.isCompilationTimeMonitoringSupported()) {
			AssertJUnit.assertTrue(cb.getTotalCompilationTime() > -1);
		} else {
			try {
				long tmp = cb.getTotalCompilationTime();
				Assert.fail("Should have thrown unsupported operation exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred: as expected (compilation time monitoring is not supported).");
			}
		}
	}

	/* Test API isCompilationTimeMonitoringSupported(). */
	@Test
	public final void testIsCompilationTimeMonitoringSupported() {
		// check if the method is there.
		String str = cb.isCompilationTimeMonitoringSupported() ? "yes" : "not" ;
		logger.debug("Compilation Time monitoring supported? : " + str);
	}

	@Test
	public final void testGetAttribute() {
		// The good attributes...
		try {
			/* Obtain the attributes the bean exposes, through the MBeanServer instance,
			 * initialized above by passing it the bean's type string.
			 */
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Name").getClass().equals(String.class));

			if (cb.isCompilationTimeMonitoringSupported()) {
				AssertJUnit.assertTrue(((Long) mbs.getAttribute(objName, "TotalCompilationTime")) > -1);
			} else {
				try {
					AssertJUnit.assertTrue(((Long) mbs.getAttribute(objName, "TotalCompilationTime")).longValue() > -1);
					Assert.fail("Unreacheable code: should have thrown unsupported operation exception");
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof MBeanException);
					logger.debug("Exception occurred, as expected: " + "attempting to obtain compilation CPU times when it is not supported.");
				}
			}

			// This could be true or false - just so long as we don't get an
			// exception raised...
			boolean ctmSupported = ((Boolean) (mbs.getAttribute(objName, "CompilationTimeMonitoringSupported")));
			AssertJUnit.assertTrue(ctmSupported == cb.isCompilationTimeMonitoringSupported());
		} catch (AttributeNotFoundException e) {
			/* Unlikely exception: attribute lookup failed! */
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getMessage());
		} catch (ReflectionException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = ((Long)(mbs.getAttribute(objName, "RPM")));
			Assert.fail("Unreacheable code: should have thrown an AttributeNotFoundException.");
		} catch (InstanceNotFoundException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (Exception e1) {
			/* Expected exception. */
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to query a non-existent attribute.");
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "TotalCompilationTime"));
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (InstanceNotFoundException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (Exception e2) {
			/* Expected exception. */
			AssertJUnit.assertTrue(e2 instanceof ClassCastException);
			logger.debug("Exception occurred, as expected: " + "attempting to perform invalid type cast on attribute.");
		}
	}

	/* Test the setAttribute() API. */
	@Test
	public final void testSetAttribute() {
		// Nothing is writable for this type. Try setting values into those readonly attributes ...
		Attribute attr = new Attribute("Name", "Boris");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("TotalCompilationTime", new Long(65533));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("CompilationTimeMonitoringSupported", new Boolean(true));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		// Try and set the Name attribute with an incorrect type.
		attr = new Attribute("Name", new Long(42));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception");
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (Exception e2) {
			AssertJUnit.assertTrue(e2 instanceof javax.management.AttributeNotFoundException);
			logger.debug("AttributeNotFoundException Exception occurred, as expected: " + e2.getMessage());
		}
	}

	/* Test the getAttributes() API. */
	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (ReflectionException e) {
			/* Unlikely exception, cause due to attribute lookup. */
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attribs.size() >= attributes.size());

		// Check through the returned values
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (name.equals("Name")) {
					AssertJUnit.assertTrue(value != null);
				} else if (name.equals("TotalCompilationTime")) {
					AssertJUnit.assertTrue(((Long)(value)) > -1);
				} else if (name.equals("CompilationTimeMonitoringSupported")) {
					// This could be true or false - just so long as we don't
					// get an exception raised...
					boolean ctmsVal = ((Boolean)value).booleanValue();
				} else {
					Assert.fail("Unexpected attribute found!");
				}
			} catch (Exception e) {
				Assert.fail("Unexpected exception thrown : " + e.getMessage());
			}
		} // end while

		// A failing scenario - pass in an attribute that is not part of
		// the management interface.
		String[] badNames = { "Cork", "Galway" };
		try {
			attributes = mbs.getAttributes(objName, badNames);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// No writable attributes for this type - should get a failure...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Name", "Curtains");
		badList.add(garbage);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testInvoke() {
		// CompilationMXBean has no operations to invoke...
		try {
			Object retVal = mbs.invoke(objName, "KissTheBlarney", new Object[] { new Long(7446), new Long(54) },
					new String[] { "java.lang.Long", "java.lang.Long" });
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
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (IntrospectionException e) {
			Assert.fail("Unexpected IntrospectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		/* Check the class name. Test substring match (not equals()) since cb.getClass().getName()
		 * typically returns a fully qualified name (prefixing package name).  This should have always
		 * been prefixed by package name, and not being changed as an aftermath of current changes
		 */
		AssertJUnit.assertTrue(cb.getClass().getName().endsWith(mbi.getClassName()));

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

		// Print out the description here.
		logger.debug("MBean description for " + cb.getClass().getName() + ": " + mbi.getDescription());

		// Three attributes - none writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.length == 4);
		logger.debug("TestCompilationMXBean.java: testGetMBeanInfo: attributes.length: " + attributes.length);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestCompilationMXBean.ignoredAttributes, attribs);
		} // end for
	}
}
