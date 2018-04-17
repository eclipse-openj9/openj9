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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryManagerMXBean;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
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

import com.ibm.lang.management.GarbageCollectorMXBean;

/**
 * @brief Class tests the various aspects of the bean MemoryManagerMXBean.
 */
@Test(groups = { "level.sanity" })
public class TestMemoryManagerMXBean {

	private static Logger logger = Logger.getLogger(TestMemoryManagerMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("MemoryPoolNames", new AttributeData("[Ljava.lang.String;", true, false, false));
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("Valid", new AttributeData(Boolean.TYPE.getName(), true, false, true));
	}// end static initializer

	private MemoryManagerMXBean mmb;
	private MBeanServer mbs;
	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		List<MemoryManagerMXBean> allMMBeans = ManagementFactory.getMemoryManagerMXBeans();
		/*Let's use the first collector. */
		mmb = allMMBeans.get(0);
		logger.info("Test MemoryManagerMXBean name=" + mmb.getName());
		try {
			objName = new ObjectName(ManagementFactory.MEMORY_MANAGER_MXBEAN_DOMAIN_TYPE + ",name=" + mmb.getName());
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up MemoryManagerMXBean test: " + me.getMessage());
		}
		logger.info("Starting MemoryManagerMXBean tests ...");
	}

	@Test
	public final void testGetMemoryPoolNames() {
		AssertJUnit.assertNotNull(mmb.getMemoryPoolNames());
		AssertJUnit.assertTrue(mmb.getMemoryPoolNames() instanceof String[]);
		String[] poolNames = mmb.getMemoryPoolNames();
		for (int i = 0; i < poolNames.length; i++) {
			String poolName = poolNames[i];
			AssertJUnit.assertNotNull(poolName);
			AssertJUnit.assertTrue(poolName.length() > 0);
		}
	}

	@Test
	public final void testGetName() {
		String name = mmb.getName();
		AssertJUnit.assertNotNull(name);
		AssertJUnit.assertTrue(name.length() > 0);
		logger.debug("Memory manager name : " + name);
	}

	@Test
	public final void testIsValid() {
		Boolean b = mmb.isValid();
		// Just ensure that the invocation worked.
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.size() == attribs.size());

		/* Obtain the attributes the bean exposes, through the MBeanServer instance,
		 * initialized above by passing it the bean's type string.
		 * Sanity check the values of those attributes.
		 */
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (name.equals("Valid")) {
					// This could be true or false - just so long as we don't
					// get an exception raised...
					boolean validVal = ((Boolean)value).booleanValue();
				} else if (name.equals("Name")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof String);
					AssertJUnit.assertTrue(((String)value).length() > 0);
				} else if (name.equals("MemoryPoolNames")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof String[]);
					String[] strVals = (String[])value;
					for (int i = 0; i < strVals.length; i++) {
						String poolName = strVals[i];
						AssertJUnit.assertNotNull(poolName);
						AssertJUnit.assertTrue(poolName.length() > 0);
					} // end for
				}
			} catch (Exception e) {
				Assert.fail("Unexpected exception thrown : " + e.getMessage());
			}
		} // end while
	}

	@Test
	public final void testSetAttributes() {
		// No writable attributes for this type
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Name", "Bez");
		badList.add(garbage);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		// Shouldn't have gotten a single attribute that could be written to.
		AssertJUnit.assertTrue(setAttrs.size() == 0);
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
		Class<?> mmbClass = mmb.getClass();
		String runtimeType = mmbClass.getName();
		AssertJUnit.assertTrue(runtimeType.endsWith(mbi.getClassName()));

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

		/* nonHeap memoryManager has 0 notification and garbageCollector has 1 notification  */
		AssertJUnit.assertTrue((0 == notifications.length) || (1 == notifications.length));

		// Print description and the class name (not necessarily identical).
		logger.debug("MBean description for " + runtimeType + ": " + mbi.getDescription());

		// The number of attributes depends on what kind of MemoryManagerMXBean
		// we have.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		if (GarbageCollectorMXBean.class.isAssignableFrom(mmbClass)) {
			AssertJUnit.assertTrue(attributes.length == 11);
			validateGCAttributes(attributes);
		} else if (MemoryManagerMXBean.class.isAssignableFrom(mmbClass)) {
			AssertJUnit.assertTrue(attributes.length == 4);
			validateMemoryManagerAttributes(attributes);
		} else {
			Assert.fail("Unexpected kind of memory manager MXBean : " + runtimeType);
		}
	}

	/**
	 * @param attributes
	 */
	private void validateMemoryManagerAttributes(MBeanAttributeInfo[] attributes) {
		for (int i = 0; i < (attributes.length - 1); i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestMemoryManagerMXBean.ignoredAttributes, attribs);
		} // end for
	}

	/**
	 * @param attributes
	 */
	private void validateGCAttributes(MBeanAttributeInfo[] attributes) {
		for (int i = 0; i < (attributes.length - 1); i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestGarbageCollectorMXBean.ignoredAttributes,
					TestGarbageCollectorMXBean.attribs);
		} // end for
	}

	@Test
	public final void testGetAttribute() {
		try {
			// The good attributes...
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "MemoryPoolNames") instanceof String[]);
			String[] arr = (String[])mbs.getAttribute(objName, "MemoryPoolNames");
			for (int i = 0; i < arr.length; i++) {
				String element = arr[i];
				AssertJUnit.assertNotNull(element);
				AssertJUnit.assertTrue(element.length() > 0);
			} // end for

			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Name") instanceof String);
			AssertJUnit.assertTrue(((String)mbs.getAttribute(objName, "Name")).length() > 0);

			// This could be true or false - just so long as we don't get an
			// exception raised...
			boolean validVal = ((Boolean)(mbs.getAttribute(objName, "Valid")));
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
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
		} catch (Exception e1) {
			// THis exception is expected.
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("AttributeNotFoundException thrown, as expected.");
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "MemoryPoolNames"));
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (InstanceNotFoundException e) {
			// Unlikely exception
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			// Unlikely exception
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		} catch (Exception e2) {
			// This exception is expected.
			AssertJUnit.assertTrue(e2 instanceof java.lang.ClassCastException);
			logger.debug("ClassCastException thrown, as expected.");
		}
	}

	@Test
	public final void testSetAttribute() {
		// Let's try and set some non-writable attributes.
		Attribute attr = new Attribute("Name", "Dando");
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

		attr = new Attribute("Valid", new Boolean(true));
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

		attr = new Attribute("MemoryPoolNames", new String[] { "X", "Y", "Z" });
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
	}

	@Test
	public final void testInvoke() {
		// No operations to invoke...
		try {
			Object retVal = mbs.invoke(objName, "KissTheRoadOfRoratonga", new Object[] { new Long(7446), new Long(54) },
					new String[] { "java.lang.Long", "java.lang.Long" });
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.ReflectionException);
			logger.debug("Exception occurred, as expected: " + e.getMessage());
		}
	}
}
