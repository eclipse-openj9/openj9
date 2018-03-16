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
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryType;
import java.lang.management.MemoryUsage;
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
import javax.management.openmbean.CompositeData;

/**
 * @brief Unit tests for testing the various functionalities and structural makeup (attributes,
 * operations, etc), of the MemoryPoolMXBean class.
 */
@Test(groups = { "level.sanity" })
public class TestMemoryPoolMXBean {

	private static Logger logger = Logger.getLogger(TestMemoryPoolMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		attribs = new Hashtable<String, AttributeData>();
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		// JLM attributes first - 15 of these (including the ObjectName).
		attribs.put("CollectionUsage", new AttributeData(CompositeData.class.getName(), true, false, false));
		// Has a setter too.
		attribs.put("CollectionUsageThreshold", new AttributeData(Long.TYPE.getName(), true, true, false));
		attribs.put("CollectionUsageThresholdCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("MemoryManagerNames", new AttributeData("[Ljava.lang.String;", true, false, false));
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("PeakUsage", new AttributeData(CompositeData.class.getName(), true, false, false));
		attribs.put("Type", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("Usage", new AttributeData(CompositeData.class.getName(), true, false, false));
		// Has a setter too.
		attribs.put("UsageThreshold", new AttributeData(Long.TYPE.getName(), true, true, false));
		attribs.put("UsageThresholdCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CollectionUsageThresholdExceeded", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("CollectionUsageThresholdSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("UsageThresholdExceeded", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("UsageThresholdSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("Valid", new AttributeData(Boolean.TYPE.getName(), true, false, true));

		// IBM extensions (only 1).
		attribs.put("PreCollectionUsage", new AttributeData(CompositeData.class.getName(), true, false, false));

	}// end static initializer

	private List<MemoryPoolMXBean> allBeans;

	private MemoryPoolMXBean testBean;

	private MBeanServer mbs;

	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		allBeans = ManagementFactory.getMemoryPoolMXBeans();
		AssertJUnit.assertTrue(allBeans.size() > 0);
		Random ran = new Random();
		int idx = ran.nextInt(allBeans.size());
		testBean = allBeans.get(idx);
		AssertJUnit.assertNotNull(testBean);
		try {
			objName = new ObjectName(ManagementFactory.MEMORY_POOL_MXBEAN_DOMAIN_TYPE + ",name=" + testBean.getName());
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up MemoryMXBeanImpl test: " + me.getMessage());
		}
		logger.info("Starting TestMemoryPoolMXBean tests ..." + "Test MemoryPoolMXBean name = " + testBean.getName());
	}

	// IBM extension method
	@Test
	public final void testGetPreCollectionUsage() {
		AssertJUnit.assertNotNull(testBean);
		AssertJUnit.assertTrue(testBean instanceof com.ibm.lang.management.MemoryPoolMXBean);
		MemoryUsage mu = ((com.ibm.lang.management.MemoryPoolMXBean)testBean).getPreCollectionUsage();

		// mu will be null if the platform does not support pre-collection usage
		// information
		if (mu != null) {
			AssertJUnit.assertTrue(mu.getCommitted() != -1);
			AssertJUnit.assertTrue(mu.getUsed() != -1);
		}
	}

	@Test
	@SuppressWarnings("unchecked")
	public final void testGetAttributes() {
		MemoryPoolMXBean testImpl = testBean;
		try {
			AttributeList attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
			AssertJUnit.assertNotNull(attributes);
			AssertJUnit.assertTrue(attributes.size() <= attribs.size());
			logger.debug("Number of retrieved attributes = " + attributes.size());

			// Check through the returned values
			Iterator it = attributes.iterator();
			while (it.hasNext()) {
				Attribute element = (Attribute)it.next();
				AssertJUnit.assertNotNull(element);
				String name = element.getName();
				Object value = element.getValue();
				if (name.equals("CollectionUsage")) {
					// Could return a null value if VM does not support
					// the method.
					if (value != null) {
						AssertJUnit.assertTrue(value instanceof CompositeData);
						MemoryUsage mu = MemoryUsage.from((CompositeData)value);
						AssertJUnit.assertTrue(mu.getCommitted() != -1);
						AssertJUnit.assertTrue(mu.getUsed() != -1);
						logger.debug("CollectionUsage : " + mu);
					}
				} else if (name.equals("CollectionUsageThreshold")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
				} else if (name.equals("CollectionUsageThresholdCount")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
					logger.debug("CollectionUsageThresholdCount = " + value);
				} else if (name.equals("MemoryManagerNames")) {
					AssertJUnit.assertTrue(value instanceof String[]);
					String[] names = (String[])value;
					AssertJUnit.assertTrue(names.length > 0);
					for (int i = 0; i < names.length; i++) {
						String string = names[i];
						AssertJUnit.assertNotNull(string);
						AssertJUnit.assertTrue(string.length() > 0);
						logger.debug("Memory manager[" + i + "] = " + string);
					} // end for
				} else if (name.equals("Name")) {
					AssertJUnit.assertTrue(value instanceof String);
					String str = (String)value;
					AssertJUnit.assertNotNull(str);
					logger.debug("Name = " + str);
				} else if (name.equals("PeakUsage")) {
					AssertJUnit.assertTrue(value instanceof CompositeData);
					MemoryUsage mu = MemoryUsage.from((CompositeData)value);
					AssertJUnit.assertNotNull(mu);
					AssertJUnit.assertTrue(mu.getCommitted() != -1);
					AssertJUnit.assertTrue(mu.getUsed() != -1);
					logger.debug("Peak usage : " + mu);
				} else if (name.equals("Type")) {
					AssertJUnit.assertTrue(value instanceof String);
					String str = (String)value;
					AssertJUnit.assertNotNull(str);
					logger.debug("Type = " + str);
				} else if (name.equals("Usage")) {
					AssertJUnit.assertTrue(value instanceof CompositeData);
					MemoryUsage mu = MemoryUsage.from((CompositeData)value);
					AssertJUnit.assertNotNull(mu);
					AssertJUnit.assertTrue(mu.getCommitted() != -1);
					AssertJUnit.assertTrue(mu.getUsed() != -1);
					logger.debug("Usage : " + mu);
				} else if (name.equals("UsageThreshold")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
					logger.debug("Usage threshold = " + value);
				} else if (name.equals("UsageThresholdCount")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
					logger.debug("UsageThresholdCount = " + value);
				} else if (name.equals("CollectionUsageThresholdExceeded")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
					logger.debug("CollectionUsageThresholdExceeded = " + value);
				} else if (name.equals("CollectionUsageThresholdSupported")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
					logger.debug("CollectionUsageThresholdSupported = " + value);
				} else if (name.equals("UsageThresholdExceeded")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
					logger.debug("UsageThresholdExceeded = " + value);
				} else if (name.equals("UsageThresholdSupported")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
					logger.debug("UsageThresholdSupported = " + value);
				} else if (name.equals("Valid")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
					logger.debug("Valid = " + value);
				} else if (name.equals("PreCollectionUsage")) {
					// Attribute belonging to the IBM extension
					// Can return a null value if the VM does not support
					// the method.
					if (value != null) {
						AssertJUnit.assertTrue(value instanceof CompositeData);
						MemoryUsage mu = MemoryUsage.from((CompositeData)value);
						AssertJUnit.assertTrue(mu.getCommitted() != -1);
						AssertJUnit.assertTrue(mu.getUsed() != -1);
						logger.debug("Pre-collection usage (IBM) : " + mu);
					}
				} else {
					Assert.fail("Unexpected attribute returned : " + name + " !!");
				}
			} // end while
		} catch (Exception e) {
			Assert.fail("Caught Exception : " + e.getCause().getMessage());
			e.printStackTrace();
		}

		// A failing scenario - pass in an attribute that is not part of
		// the management interface.
		String[] badNames = { "Cork", "Galway" };
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, badNames);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// Only two attributes can be set for this platform bean type
		// - UsageThreshold and CollectionUsageThreshold
		MemoryPoolMXBean testImpl = testBean;
		AttributeList attList = new AttributeList();
		Attribute newUT = new Attribute("UsageThreshold", new Long(100 * 1024));
		attList.add(newUT);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, attList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() <= 1);

		if (setAttrs.size() == 1) {
			AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getName().equals("UsageThreshold"));
			AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getValue() instanceof Long);
			long recoveredValue = (Long)((Attribute)(setAttrs.get(0))).getValue();
			AssertJUnit.assertEquals(100 * 1024, recoveredValue);
		}

		attList = new AttributeList();
		Attribute newCUT = new Attribute("CollectionUsageThreshold", new Long(250 * 1024));
		attList.add(newCUT);
		try {
			setAttrs = mbs.setAttributes(objName, attList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() <= 1);

		if (setAttrs.size() == 1) {
			AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getName().equals("CollectionUsageThreshold"));
			AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getValue() instanceof Long);
			long recoveredValue = (Long)((Attribute)(setAttrs.get(0))).getValue();
			AssertJUnit.assertEquals(250 * 1024, recoveredValue);
		}

		// A failure scenario - a non-existent attribute...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Bantry", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Another failure scenario - a non-writable attribute...
		badList = new AttributeList();
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
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
		garbage = new Attribute("CollectionUsageThreshold", new Boolean(true));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testGetMBeanInfo() {
		MemoryPoolMXBean testImpl = testBean;
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

		// Class name
		AssertJUnit.assertTrue(mbi.getClassName().equals(testImpl.getClass().getName()));

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertTrue(constructors.length == 0);

		// One public operation - resetPeakUsage
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertTrue(operations.length == 1);
		AssertJUnit.assertEquals("resetPeakUsage", operations[0].getName());

		// No notifications
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertTrue(notifications.length == 0);

		// Print description and the class name (not necessarily identical).
		logger.debug("MBean description for " + testImpl.getClass().getName() + ": " + mbi.getDescription());

		// Sixteen attributes - only two are writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.length == 17);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestMemoryPoolMXBean.ignoredAttributes, attribs);
		} // end for
	}

	@Test
	public final void testGetAttribute() {
		MemoryPoolMXBean testImpl = testBean;
		try {
			// The 14 good public attributes...
			{
				// If collection usage not supported then we can get a
				// null return here.
				try {
					CompositeData cd = (CompositeData)mbs.getAttribute(objName, "CollectionUsage");
					if (cd != null) {
						logger.debug("CollectionUsage : cd=" + cd);
						MemoryUsage mu = MemoryUsage.from(cd);
						AssertJUnit.assertTrue(mu.getCommitted() != -1);
						AssertJUnit.assertTrue(mu.getUsed() != -1);
						logger.debug("CollectionUsage : " + mu);
					}
				} catch (Exception e) {
					//logger.debug("**Error TestMemoryPoolMXBean testImpl: "+ testImpl.getName() +", testGetAttribute() Exception: "+ e.getClass().getName());
					Assert.fail("Unexpected exception : " + e.getClass().getName());
				}
			}

			try {
				if (testImpl.isCollectionUsageThresholdSupported()) {
					Long l = (Long)mbs.getAttribute(objName, "CollectionUsageThreshold");
					AssertJUnit.assertNotNull(l);
					AssertJUnit.assertTrue(l > -1);
				} else {
					try {
						Long l = (Long)mbs.getAttribute(objName, "CollectionUsageThreshold");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: as expected (collection usage threshold not supported).");
					}
				} // end else collection usage threshold is not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				if (testImpl.isCollectionUsageThresholdSupported()) {
					Long l = (Long)mbs.getAttribute(objName, "CollectionUsageThresholdCount");
					AssertJUnit.assertNotNull(l);
					AssertJUnit.assertTrue(l > -1);
				} else {
					try {
						Long l = (Long)mbs.getAttribute(objName, "CollectionUsageThresholdCount");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: as expected (collection usage threshold count not supported).");
					}
				} // end else collection usage threshold is not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				String[] names = (String[])mbs.getAttribute(objName, "MemoryManagerNames");
				AssertJUnit.assertNotNull(names);
				for (int i = 0; i < names.length; i++) {
					String string = names[i];
					AssertJUnit.assertNotNull(string);
					AssertJUnit.assertTrue(string.length() > 0);
					logger.debug("MemoryManagerNames[" + i + "] = " + string);
				} // end for
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				String name = (String)mbs.getAttribute(objName, "Name");
				AssertJUnit.assertNotNull(name);
				AssertJUnit.assertTrue(name.length() > 0);
				logger.debug("Name is " + name);
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				CompositeData cd = (CompositeData)mbs.getAttribute(objName, "PeakUsage");

				if (testImpl.isValid()) {
					AssertJUnit.assertNotNull(cd);
					MemoryUsage mu = MemoryUsage.from(cd);
					AssertJUnit.assertTrue(mu.getCommitted() != -1);
					AssertJUnit.assertTrue(mu.getUsed() != -1);
					logger.debug("PeakUsage : " + mu);
				} else {
					AssertJUnit.assertNull(cd);
				}
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				String name = (String)mbs.getAttribute(objName, "Type");
				AssertJUnit.assertNotNull(name);
				AssertJUnit.assertTrue(name.length() > 0);
				logger.debug("Type is " + name);
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				CompositeData cd = (CompositeData)mbs.getAttribute(objName, "Usage");
				if (testImpl.isValid()) {
					AssertJUnit.assertNotNull(cd);
					MemoryUsage mu = MemoryUsage.from(cd);
					AssertJUnit.assertTrue(mu.getCommitted() != -1);
					AssertJUnit.assertTrue(mu.getUsed() != -1);
					logger.debug("Usage : " + mu);
				} else {
					AssertJUnit.assertNull(cd);
				}
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				if (testImpl.isUsageThresholdSupported()) {
					Long l = (Long)mbs.getAttribute(objName, "UsageThreshold");
					AssertJUnit.assertNotNull(l);
					AssertJUnit.assertTrue(l > -1);
					logger.debug("Usage threshold = " + l);
				} else {
					try {
						Long l = (Long)mbs.getAttribute(objName, "UsageThreshold");
						Assert.fail("Unreacheable code: should have thrown exception");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: as expected (usage threshold not supported).");
					}
				} // end else usage threshold not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				if (testImpl.isUsageThresholdSupported()) {
					Long l = (Long)mbs.getAttribute(objName, "UsageThresholdCount");
					AssertJUnit.assertNotNull(l);
					AssertJUnit.assertTrue(l > -1);
					logger.debug("Usage threshold count = " + l);
				} else {
					try {
						Long l = (Long)mbs.getAttribute(objName, "UsageThresholdCount");
						Assert.fail("Unreacheable code: should have thrown exception");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: as expected (usage threshold count not supported).");
					}
				} // end else usage threshold not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				if (testImpl.isCollectionUsageThresholdSupported()) {
					Boolean b = (Boolean)mbs.getAttribute(objName, "CollectionUsageThresholdExceeded");
					AssertJUnit.assertNotNull(b);
					logger.debug("CollectionUsageThresholdExceeded is " + b);
				} else {
					try {
						Boolean b = (Boolean)mbs.getAttribute(objName, "CollectionUsageThresholdExceeded");
						Assert.fail("Unreacheable code: should have thrown exception");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: " + "as expected (collection usage threshold exceeded not supported).");
					}
				} // end else collection usage threshold not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				Boolean b = (Boolean)mbs.getAttribute(objName, "CollectionUsageThresholdSupported");
				AssertJUnit.assertNotNull(b);
				logger.debug("CollectionUsageThresholdSupported is " + b);
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				if (testImpl.isUsageThresholdSupported()) {
					Boolean b = (Boolean)mbs.getAttribute(objName, "UsageThresholdExceeded");
					AssertJUnit.assertNotNull(b);
					logger.debug("UsageThresholdExceeded is " + b);
				} else {
					try {
						Boolean b = (Boolean)mbs.getAttribute(objName, "UsageThresholdExceeded");
						Assert.fail("Unreacheable code: should have thrown exception");
					} catch (Exception e) {
						AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
						logger.debug("Exception occurred: as expected (usage threshold exceeded not supported).");
					}
				} // end else usage threshold not supported
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				Boolean b = (Boolean)mbs.getAttribute(objName, "UsageThresholdSupported");
				AssertJUnit.assertNotNull(b);
				logger.debug("UsageThresholdSupported is " + b);
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			try {
				Boolean b = (Boolean)mbs.getAttribute(objName, "Valid");
				AssertJUnit.assertNotNull(b);
				logger.debug("Valid is " + b);
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}

			// The 1 good IBM attribute ...
			try {
				// If not supported on the VM then permissible to get a
				// null return value.
				CompositeData cd = (CompositeData)mbs.getAttribute(objName, "PreCollectionUsage");
				if (null != cd) {
					MemoryUsage mu = MemoryUsage.from(cd);
					AssertJUnit.assertTrue(mu.getCommitted() != -1);
					AssertJUnit.assertTrue(mu.getUsed() != -1);
					logger.debug("(IBM) PreCollectionUsage : " + mu);
				}
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			}
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = ((Long)(mbs.getAttribute(objName, "RPM")));
			Assert.fail("Unreacheable code: should have thrown an AttributeNotFoundException.");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof AttributeNotFoundException);
			logger.debug("Exception occurred: as expected, no such attribute found (RPM).");
		}

		// Type mismatch should result in a casting exception
		try {
			Long bad = (Long)(mbs.getAttribute(objName, "Name"));
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof ClassCastException);
			logger.debug("Exception occurred: as expected, invalid type cast attempted.");
		}
	}

	@Test
	public void testSetUsageThresholdAttribute() {
		MemoryPoolMXBean testImpl = testBean;

		if (testImpl.isUsageThresholdSupported()) {
			try {
				long originalUT = (Long)mbs.getAttribute(objName, "UsageThreshold");
				long newUT = originalUT + 1024;
				Attribute newUTAttr = new Attribute("UsageThreshold", new Long(newUT));
				mbs.setAttribute(objName, newUTAttr);

				AssertJUnit.assertEquals(new Long(newUT), (Long)mbs.getAttribute(objName, "UsageThreshold"));
			} catch (AttributeNotFoundException e) {
				e.printStackTrace();
				Assert.fail("Unexpected AttributeNotFoundException occurred: " + e.getMessage());
			} catch (InstanceNotFoundException e) {
				e.printStackTrace();
				Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			} catch (MBeanException e) {
				e.printStackTrace();
				Assert.fail("Unexpected MBeanException occurred: " + e.getMessage());
			} catch (ReflectionException e) {
				e.printStackTrace();
				Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			} catch (InvalidAttributeValueException e) {
				e.printStackTrace();
				Assert.fail("Unexpected InvalidAttributeValueException occurred: " + e.getMessage());
			}
		} else {
			try {
				Attribute newUTAttr = new Attribute("UsageThreshold", new Long(100 * 1024));
				mbs.setAttribute(objName, newUTAttr);
				Assert.fail("Unreacheable code: should have thrown exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
				logger.debug("Exception occurred, as expected: cannot set attribute (UsageThreshold).");
			}
		} // end else usage threshold is not supported
	}

	@Test
	public void testSetCollectionUsageThresholdAttribute() {
		MemoryPoolMXBean testImpl = testBean;

		if (testImpl.isCollectionUsageThresholdSupported()) {
			try {
				long originalCUT = (Long)mbs.getAttribute(objName, "CollectionUsageThreshold");
				long newCUT = originalCUT + 1024;
				Attribute newCUTAttr = new Attribute("CollectionUsageThreshold", new Long(newCUT));
				mbs.setAttribute(objName, newCUTAttr);

				AssertJUnit.assertEquals(new Long(newCUT), (Long)mbs.getAttribute(objName, "CollectionUsageThreshold"));
			} catch (AttributeNotFoundException e) {
				e.printStackTrace();
				Assert.fail("Unexpected AttributeNotFoundException occurred: " + e.getMessage());
			} catch (InstanceNotFoundException e) {
				e.printStackTrace();
				Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			} catch (MBeanException e) {
				e.printStackTrace();
				Assert.fail("Unexpected MBeanException occurred: " + e.getMessage());
			} catch (ReflectionException e) {
				e.printStackTrace();
				Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			} catch (InvalidAttributeValueException e) {
				e.printStackTrace();
				Assert.fail("Unexpected InvalidAttributeValueException occurred: " + e.getMessage());
			}
		} else {
			try {
				Attribute newCUTAttr = new Attribute("CollectionUsageThreshold", new Long(100 * 1024));
				mbs.setAttribute(objName, newCUTAttr);
				Assert.fail("Unreacheable code: should have thrown exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
				logger.debug("Exception occurred, as expected: cannot set attribute (CollectionUsageThreshold).");
			}
		} // end else collection usage threshold is not supported
	}

	@Test
	public final void testBadSetAttribute() {
		MemoryPoolMXBean testImpl = testBean;

		// Let's try and set some non-writable attributes.
		Attribute attr = new Attribute("UsageThresholdCount", new Long(25));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e) {
			logger.debug("testBadSetAttribute exception:" + e.getClass().getName() + ", errMessage="
					+ e.getMessage());
			AssertJUnit.assertTrue(e instanceof AttributeNotFoundException);
		}

		// Try and set the UsageThreshold attribute with an incorrect
		// type.
		attr = new Attribute("UsageThreshold", "rubbish");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof InvalidAttributeValueException);
		}
	}

	@Test
	public final void testInvoke() {
		MemoryPoolMXBean testImpl = testBean;

		// We have one operation - resetPeakUsage() which should reset *peak*
		// usage to the current memory usage.
		try {
			Object retVal = null;
			try {
				retVal = mbs.invoke(objName, "resetPeakUsage", new Object[] {}, null);
			} catch (InstanceNotFoundException e) {
				// InstanceNotFoundException is an unlikely exception - if this occurs, we
				// can proceed with the test.
				Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			}
			MemoryUsage currentMU = testImpl.getUsage();
			MemoryUsage peakMU = testImpl.getPeakUsage();
			logger.debug("currentMU : " + currentMU);
			logger.debug("peakMU    : " + peakMU);
			AssertJUnit.assertNull(retVal);
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		// Try and invoke a non-existent method...
		try {
			Object retVal = mbs.invoke(objName, "madeupMethod", new Object[] { "fibber" },
					new String[] { String.class.getName() });
			Assert.fail("Unreacheable code: should have thrown an exception calling a non-existent operation");
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred: " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof ReflectionException);
			logger.debug("ReflectionException occurred, as expected: attempted to invoke non-existent method.");
		}
	}

	@Test
	public final void testGetCollectionUsage() {
		// If this method is not supported then it returns null.
		try {
			MemoryUsage mu = testBean.getCollectionUsage();
			if (mu != null) {
				AssertJUnit.assertTrue(mu.getCommitted() > -1);
				AssertJUnit.assertTrue(mu.getUsed() > -1);
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception : " + e.getClass().getName());
		}
	}

	@Test
	public final void testGetCollectionUsageThreshold() {
		if (testBean.isCollectionUsageThresholdSupported()) {
			long val = testBean.getCollectionUsageThreshold();
			AssertJUnit.assertTrue(val > -1);
		} else {
			try {
				long val = testBean.getCollectionUsageThreshold();
				Assert.fail("Unreacheable code: should have thrown exception.");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred, as expected: collection threshold not supported.");
			}
		} // end else
	}

	@Test
	public final void testGetCollectionUsageThresholdCount() {
		if (testBean.isCollectionUsageThresholdSupported()) {
			long val = testBean.getCollectionUsageThresholdCount();
			AssertJUnit.assertTrue(val > -1);
		} else {
			try {
				long val = testBean.getCollectionUsageThresholdCount();
				Assert.fail("Unreacheable code: should have thrown exception.");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred, as expected: cannot obtain collection threshold count.");
			}
		} // end else
	}

	@Test
	public final void testGetMemoryManagerNames() {
		String[] mgrs = testBean.getMemoryManagerNames();
		AssertJUnit.assertNotNull(mgrs);
		for (int i = 0; i < mgrs.length; i++) {
			String string = mgrs[i];
			AssertJUnit.assertNotNull(string);
			AssertJUnit.assertTrue(string.length() > 0);
		} // end for
	}

	@Test
	public final void testGetName() {
		String name = testBean.getName();
		AssertJUnit.assertNotNull(name);
		AssertJUnit.assertTrue(name.length() > 0);
	}

	@Test
	public final void testGetPeakUsage() {
		MemoryUsage mu = testBean.getPeakUsage();
		if (testBean.isValid()) {
			// is a valid pool
			AssertJUnit.assertNotNull(mu);
			AssertJUnit.assertTrue(mu.getCommitted() != -1);
			AssertJUnit.assertTrue(mu.getUsed() != -1);
		} else {
			AssertJUnit.assertNull(mu);
		} // end else not a valid pool
	}

	@Test
	public final void testGetType() {
		MemoryType type = testBean.getType();
		AssertJUnit.assertNotNull(type);
		boolean isOfKnownType = false;
		MemoryType[] allTypes = MemoryType.values();
		for (int i = 0; i < allTypes.length; i++) {
			MemoryType type2 = allTypes[i];
			if (type2.equals(type)) {
				isOfKnownType = true;
				break;
			}
		} // end for
		AssertJUnit.assertTrue(isOfKnownType);
	}

	@Test
	public final void testGetUsage() {
		MemoryUsage mu = testBean.getUsage();
		if (testBean.isValid()) {
			// is a valid pool
			AssertJUnit.assertNotNull(mu);
			AssertJUnit.assertTrue(mu.getCommitted() != -1);
			AssertJUnit.assertTrue(mu.getUsed() != -1);
		} else {
			AssertJUnit.assertNull(mu);
		} // end else not a valid pool
	}

	@Test
	public final void testGetUsageThreshold() {
		if (testBean.isUsageThresholdSupported()) {
			long val = testBean.getUsageThreshold();
			AssertJUnit.assertTrue(val > -1);
		} else {
			try {
				long val = testBean.getUsageThreshold();
				Assert.fail("Unreacheable code: should have thrown an exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred, as expected: cannot obtain usage threshold.");
			}
		} // end else usage threshold not supported
	}

	@Test
	public final void testGetUsageThresholdCount() {
		if (testBean.isUsageThresholdSupported()) {
			long val = testBean.getUsageThresholdCount();
			AssertJUnit.assertTrue(val > -1);
		} else {
			try {
				long val = testBean.getUsageThresholdCount();
				Assert.fail("Unreacheable code: should have thrown an exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred, as expected: cannout obtain usage threshold count.");
			}
		} // end else usage threshold not supported
	}

	@Test
	public final void testIsCollectionUsageThresholdExceeded() {
		if (testBean.isCollectionUsageThresholdSupported()) {
			boolean val = testBean.isCollectionUsageThresholdExceeded();
		} else {
			try {
				boolean val = testBean.isCollectionUsageThresholdExceeded();
				Assert.fail("Unreacheable code: should have thrown an exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("Exception occurred, as expected:" + " cannot check collection usage threshold exceeded.");
			}
		} // end else usage threshold not supported
	}

	@Test
	public final void testIsUsageThresholdExceeded() {
		if (testBean.isUsageThresholdSupported()) {
			boolean val = testBean.isUsageThresholdExceeded();
		} else {
			try {
				boolean val = testBean.isUsageThresholdExceeded();
				Assert.fail("Unreacheable code: should have thrown an exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("UnsupportedOperationException occurred, as expected.");
			}
		} // end else usage threshold not supported
	}

	@Test
	public final void testResetPeakUsage() {
		MemoryUsage peak = testBean.getPeakUsage();
		logger.debug("testResetPeakUsage bean name:" + testBean.getName());
		logger.debug("peakMU    : " + peak);
		MemoryUsage current = testBean.getUsage();
		testBean.resetPeakUsage();
		peak = testBean.getPeakUsage();
		logger.debug("currentMU : " + current);
		logger.debug("new peakMU: " + peak);
	}

	@Test
	public final void testSetCollectionUsageThreshold() {
		// Good case
		if (testBean.isCollectionUsageThresholdSupported()) {
			long before = testBean.getCollectionUsageThreshold();
			testBean.setCollectionUsageThreshold(before + (8 * 1024));
			long after = testBean.getCollectionUsageThreshold();
			AssertJUnit.assertEquals((before + (8 * 1024)), after);
		} else {
			try {
				testBean.setCollectionUsageThreshold(1024);
				Assert.fail("Unreacheable code: should have thrown exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("UnsupportedOperationException occurred, as expected.");
			}
		}

		// Not so good case
		if (testBean.isCollectionUsageThresholdSupported()) {
			try {
				testBean.setCollectionUsageThreshold(-1024);
				Assert.fail("Unreacheable code: should have thrown exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
				logger.debug("IllegalArgumentException occurred, as expected.");
			}
		}
	}

	@Test
	public final void testSetUsageThreshold() {
		// Good case
		if (testBean.isUsageThresholdSupported()) {
			long before = testBean.getUsageThreshold();
			testBean.setUsageThreshold(before + (8 * 1024));
			long after = testBean.getUsageThreshold();
			AssertJUnit.assertEquals((before + (8 * 1024)), after);
		} else {
			try {
				testBean.setUsageThreshold(1024);
				Assert.fail("Unreacheable code: should have thrown exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
				logger.debug("UnsupportedOperationException occurred, as expected.");
			}
		}

		// Not so good case
		if (testBean.isUsageThresholdSupported()) {
			try {
				testBean.setUsageThreshold(-1024);
				Assert.fail("Unreacheable code: should have thrown exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
				logger.debug("IllegalArgumentException occurred, as expected.");
			}
		}
	}
}
