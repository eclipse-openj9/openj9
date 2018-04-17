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
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Random;

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
import javax.management.openmbean.CompositeData;
import javax.management.ReflectionException;


/**
 * Test the GarbageCollectorMXBean class.
 */
@Test(groups = { "level.sanity" })
public class TestGarbageCollectorMXBean {

	private static Logger logger = Logger.getLogger(TestGarbageCollectorMXBean.class);

	static final Map<String, AttributeData> attribs;

	static final HashSet<String> ignoredAttributes;

	private MBeanServer mbs;

	private ObjectName objName;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("MemoryPoolNames", new AttributeData("[Ljava.lang.String;", true, false, false));
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("Valid", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("CollectionCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CollectionTime", new AttributeData(Long.TYPE.getName(), true, false, false));

		// Oracle extensions
		attribs.put("LastGcInfo", new AttributeData(CompositeData.class.getName(), true, false, false));

		// IBM extensions
		attribs.put("LastCollectionStartTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("LastCollectionEndTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("MemoryUsed", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("TotalMemoryFreed", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("TotalCompacts", new AttributeData(Long.TYPE.getName(), true, false, false));
	}// end static initializer

	private GarbageCollectorMXBean gcb;

	@BeforeClass
	protected void setUp() throws Exception {
		List<GarbageCollectorMXBean> allGCBeans = ManagementFactory.getGarbageCollectorMXBeans();
		/* Let's use a randomly chosen collector, out of the ones available. */
		Random ran = new Random();
		int idx = ran.nextInt(allGCBeans.size());
		gcb = allGCBeans.get(idx);
		logger.info("Test GarbageCollectorMXBean name = " + gcb.getName());
		try {
			objName = new ObjectName(ManagementFactory.GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE + ",name=" + gcb.getName());
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up GarbageCollectorMXBean test: " + me.getMessage());
		}
		logger.info("Starting GarbageCollectorMXBean tests ...");
	}

	//check that the agreed Oracle extension operations are present.
	@Test
	public final void testGetLastGcInfo() {
		AssertJUnit.assertTrue(gcb instanceof com.sun.management.GarbageCollectorMXBean);
		System.gc();
		System.gc();
		AssertJUnit.assertTrue(((com.sun.management.GarbageCollectorMXBean)gcb).getLastGcInfo() != null);
	}

	// Check that the agreed IBM extension operations are present.
	@Test
	public final void testGetLastCollectionStartTime() {
		AssertJUnit.assertTrue(gcb instanceof com.ibm.lang.management.GarbageCollectorMXBean);
		AssertJUnit.assertTrue(((com.ibm.lang.management.GarbageCollectorMXBean)gcb).getLastCollectionStartTime() > -1);
	}

	// Check that the agreed IBM extension operations are present.
	@Test
	public final void testGetLastCollectionEndTime() {
		AssertJUnit.assertTrue(gcb instanceof com.ibm.lang.management.GarbageCollectorMXBean);
		AssertJUnit.assertTrue(((com.ibm.lang.management.GarbageCollectorMXBean)gcb).getLastCollectionEndTime() > -1);
	}

	@Test
	public final void testGetCollectionCount() {
		/* Collection Count could be zero */
		AssertJUnit.assertTrue(gcb.getCollectionCount() >= 0);
	}

	@Test
	public final void testGetCollectionTime() {
		try {
			long cTime = gcb.getCollectionTime();
			// It is possible for this time to be -1 if the collection elapsed
			// time is undefined for this garbage collector.
			AssertJUnit.assertTrue(cTime > -2);
		} catch (Exception e) {
			Assert.fail("Unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public final void testGetMemoryPoolNames() {
		AssertJUnit.assertNotNull(gcb.getMemoryPoolNames());
		AssertJUnit.assertTrue(gcb.getMemoryPoolNames() instanceof String[]);
	}

	@Test
	public final void testGetName() {
		String name = gcb.getName();
		AssertJUnit.assertNotNull(name);
		AssertJUnit.assertTrue(name.length() > 0);
	}

	@Test
	public final void testIsValid() {
		Boolean b = gcb.isValid();
		// Just ensure that the invocation worked.
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.size() <= attribs.size());

		/* Obtain the attributes the bean exposes, through the MBeanServer instance,
		 * initialized above by passing it the bean's type string.
		 * Do some sanity checks on the values returned for those attributes.
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
				} else if (name.equals("CollectionCount")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof Long);
					// It is possible for this count to be -1 if undefined for
					// this garbage collector
					AssertJUnit.assertTrue((Long)value > -2);
				} else if (name.equals("CollectionTime")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof Long);
					// It is possible for this time to be -1 if undefined for
					// this garbage collector
					AssertJUnit.assertTrue((Long)value > -2);
				} else if (name.equals("LastCollectionStartTime")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue((Long)value > -2);
				} else if (name.equals("LastCollectionEndTime")) {
					AssertJUnit.assertNotNull(value);
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue((Long)value > -2);
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
		AssertJUnit.assertTrue(setAttrs.size() == 0);
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

		/* Check the class name. Test substring match (not equals()) since gcb.getClass().getName()
		 * typically returns a fully qualified name (prefixing package name).  This should have always
		 * been prefixed by package name, and not being changed as an aftermath of current changes
		 */
		AssertJUnit.assertTrue(gcb.getClass().getName().endsWith(mbi.getClassName()));

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertEquals(0, constructors.length);

		// No public operations
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertEquals(0, operations.length);

		// No notifications
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);

		AssertJUnit.assertEquals(1, notifications.length);

		// Print out both, the description as well as the the class name.
		logger.debug("MBean description for " + gcb.getClass().getName() + ": " + mbi.getDescription());

		// 10 attributes (5 standard, 6 IBM) - none is writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertEquals(12, attributes.length);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestGarbageCollectorMXBean.ignoredAttributes, attribs);
		} // end for
	}

	@Test
	public final void testGetIBMAttribute() {
		try {
			// The good attributes...
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "LastCollectionStartTime") instanceof Long);
			AssertJUnit.assertTrue(((Long)mbs.getAttribute(objName, "LastCollectionStartTime")) > -1);

			AssertJUnit.assertTrue(mbs.getAttribute(objName, "LastCollectionEndTime") instanceof Long);
			AssertJUnit.assertTrue(((Long)mbs.getAttribute(objName, "LastCollectionEndTime")) > -1);
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		}
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

			AssertJUnit.assertTrue(mbs.getAttribute(objName, "CollectionCount") instanceof Long);
			// It is possible for this count to be -1 if undefined for
			// this garbage collector
			AssertJUnit.assertTrue(((Long)mbs.getAttribute(objName, "CollectionCount")) > -2);

			AssertJUnit.assertTrue(mbs.getAttribute(objName, "CollectionTime") instanceof Long);
			// It is possible for this time to be -1 if undefined for
			// this garbage collector
			AssertJUnit.assertTrue(((Long)mbs.getAttribute(objName, "CollectionTime")) > -2);
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
		} catch (Exception e1) {
			/* Expected exception. */
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to query a non-existent attribute.");
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "CollectionTime"));
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (Exception e2) {
			/* Expected exception. */
			AssertJUnit.assertTrue(e2 instanceof java.lang.ClassCastException);
			logger.debug("Exception occurred, as expected: " + "attempting to perform invalid type cast on attribute.");
		}
	}

	@Test
	public final void testSetAttribute() {
		// Let's try and set some non-writable attributes.
		Attribute attr = new Attribute("Name", "Dando");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("Valid", new Boolean(true));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("MemoryPoolNames", new String[] { "X", "Y", "Z" });
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("CollectionCount", new Long(233));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
		}

		attr = new Attribute("CollectionTime", new Long(233));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + "attempting to set a read-only attribute.");
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
