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

import org.testng.log4testng.Logger;
import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.LogManager;

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

// This class is not public API.
import com.ibm.java.lang.management.internal.LoggingMXBeanImpl;

/**
 * @brief Test the LoggingMXBean class.
 */
@SuppressWarnings({ "nls", "unused" })
@Test(groups = { "level.sanity" })
public class TestLoggingMXBean {

	private static Logger logger = Logger.getLogger(TestLoggingMXBean.class);
	private static final Map<String, AttributeData> attribs;

	static {
		// The only attributes; the other get*() APIs take parameters, thus, making them operations.
		attribs = new Hashtable<>();
		attribs.put("LoggerNames", new AttributeData("[Ljava.lang.String;", true, false, false));
		attribs.put("ObjectName", new AttributeData(String.class.getName(), true, false, false));
	}

	private LoggingMXBeanImpl lb;

	private Enumeration<String> loggerNamesEnumeration;

	private MBeanServer mbs;

	private ObjectName objName;

	@BeforeMethod
	protected void resetLoggerNamesEnumeration() {
		loggerNamesEnumeration = LogManager.getLogManager().getLoggerNames();
	}

	@BeforeClass
	protected void setUp() throws Exception {
		lb = LoggingMXBeanImpl.getInstance();

		try {
			objName = new ObjectName(LogManager.LOGGING_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up ClassLoadingMXBean test: " + me.getMessage());
		}
		logger.info("Starting TestLoggingMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testIsSingleton() {
		// Verify we always get the same instance
		LoggingMXBeanImpl bean = LoggingMXBeanImpl.getInstance();
		AssertJUnit.assertSame(lb, bean);
	}

	@Test
	public final void testGetAttribute() {
		// Only two readable attributes for this type.
		try {
			Object attribute = mbs.getAttribute(objName, "LoggerNames");
			AssertJUnit.assertNotNull(attribute);
			AssertJUnit.assertTrue(attribute instanceof String[]);
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException (LoggerNames): " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException (LoggerNames): " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException (LoggerNames): " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException (LoggerNames): " + e.getMessage());
		}

		try {
			Object attribute = mbs.getAttribute(objName, "ObjectName");
			AssertJUnit.assertNotNull(attribute);
			AssertJUnit.assertTrue(attribute instanceof ObjectName);
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException (ObjectName): " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException (ObjectName): " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException (ObjectName): " + e.getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException (ObjectName): " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			Long rpm = (Long) mbs.getAttribute(objName, "RPM");
			Assert.fail("Unreacheable code: should have thrown an AttributeNotFoundException.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
		} catch (AttributeNotFoundException e) {
			// This exception is expected.
			logger.debug("AttributeNotFoundException thrown, as expected.");
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String) mbs.getAttribute(objName, "LoggerNames");
			Assert.fail("Unreacheable code: should have thrown a ClassCastException");
		} catch (InstanceNotFoundException e) {
			// Unlikely exception
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
		} catch (ReflectionException e) {
			// Unlikely exception
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
		} catch (ClassCastException e) {
			// This exception is expected.
			logger.debug("Exception occurred, as expected: attempting to perform invalid type cast on attribute.");
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}
	}

	@Test
	public final void testGetLoggerLevel() {
		// Verify we get something sensible back for the known loggers...
		while (loggerNamesEnumeration.hasMoreElements()) {
			String logName = loggerNamesEnumeration.nextElement();
			AssertJUnit.assertNotNull(logName);
			String level = lb.getLoggerLevel(logName);
			AssertJUnit.assertNotNull(level);
			if (level.length() > 0) {
				try {
					Level l = Level.parse(level);
				} catch (Exception e) {
					Assert.fail("Unexpected exception parsing returned log level name: " + e.getMessage());
				}
			}
		}

		// Ensure we get a null back if the named Logger is fictional...
		AssertJUnit.assertNull(lb.getLoggerLevel("made up name"));
	}

	@Test
	public final void testGetLoggerNames() {
		// The answer according to the bean ...
		List<String> beanNames = lb.getLoggerNames();

		AssertJUnit.assertNotNull(beanNames);
		while (loggerNamesEnumeration.hasMoreElements()) {
			String mgrName = loggerNamesEnumeration.nextElement();
			AssertJUnit.assertTrue(beanNames.contains(mgrName));
		}
	}

	@Test
	public final void testGetParentLoggerName() {
		// Verify we get something sensible back for the known loggers...
		while (loggerNamesEnumeration.hasMoreElements()) {
			String logName = loggerNamesEnumeration.nextElement();
			java.util.logging.Logger logger = LogManager.getLogManager().getLogger(logName);
			java.util.logging.Logger parent = logger.getParent();
			if (parent != null) {
				// The logger is not the root logger
				String parentName = logger.getParent().getName();
				AssertJUnit.assertEquals(parentName, lb.getParentLoggerName(logName));
			} else {
				// The logger is the root logger and has no parent.
				AssertJUnit.assertEquals("", lb.getParentLoggerName(logName));
			}
		}

		// Ensure we get a null back if the named Logger is fictional...
		AssertJUnit.assertNull(lb.getParentLoggerName("made up name"));
	}

	@Test
	public final void testSetLoggerLevel() {
		String logName = null;
		while (loggerNamesEnumeration.hasMoreElements()) {
			logName = loggerNamesEnumeration.nextElement();

			// Store the logger's current log level.
			java.util.logging.Logger logger = LogManager.getLogManager().getLogger(logName);
			Level originalLevel = logger.getLevel();

			// Set the logger to have a new level.
			lb.setLoggerLevel(logName, Level.SEVERE.getName());

			// Verify the set worked
			AssertJUnit.assertEquals(Level.SEVERE.getName(), logger.getLevel().getName());
		}

		// Verify that we get an IllegalArgumentException if we supply a
		// bogus loggerName.
		try {
			lb.setLoggerLevel("Ella Fitzgerald", Level.SEVERE.getName());
			Assert.fail("Should have thrown an exception for a bogus log name!");
		} catch (IllegalArgumentException e) {
			// This exception is expected.
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		// Verify that we get an IllegalArgumentException if we supply a
		// bogus log level value.
		try {
			lb.setLoggerLevel(logName, "Scott Walker");
			Assert.fail("Should have thrown an exception for a bogus log level!");
		} catch (IllegalArgumentException e) {
			// This exception is expected.
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}
	}

	@Test
	public final void testSetAttribute() {
		// The only attribute for this type is not a writable one.
		Attribute attr = new Attribute("LoggerLevel", "Boris");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (AttributeNotFoundException | ReflectionException e) {
			// One of these exceptions is expected.
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		attr = new Attribute("LoggerNames", new String[] { "Strummer", "Jones" });
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (AttributeNotFoundException | ReflectionException e) {
			// One of these exceptions is expected.
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[0]));
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertEquals(attribs.size(), attributes.size());

		// Check the returned values
		for (int i = 0; i < attributes.size(); ++i) {
			Attribute attribute = (Attribute) attributes.get(i);

			AssertJUnit.assertNotNull(attribute);

			String attributeName = attribute.getName();
			Object value = attribute.getValue();

			switch (attributeName) {
			case "LoggerNames":
				AssertJUnit.assertTrue(value instanceof String[]);
				break;
			case "ObjectName":
				AssertJUnit.assertTrue(value instanceof ObjectName);
				break;
			default:
				Assert.fail("Unexpected attribute: " + attributeName);
				break;
			}
		}

		// A failing scenario - pass in some attribute names that are not part
		// of the management interface.
		String[] badNames = { "Cork", "Galway" };
		try {
			attributes = mbs.getAttributes(objName, badNames);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertEquals(0, attributes.size());
	}

	@Test
	public final void testSetAttributes() {
		// No writable attributes for this type - should get a failure...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Name", "City Sickness");
		Attribute trash = new Attribute("SpecVendor", "Marbles");
		badList.add(garbage);
		badList.add(trash);
		AttributeList setAttrs;
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertEquals(0, setAttrs.size());
	}

	@Test
	public final void testInvoke() {
		// 3 different operations that can be invoked on this kind of bean.
		while (loggerNamesEnumeration.hasMoreElements()) {
			String logName = loggerNamesEnumeration.nextElement();
			// Store the logger's current log level.
			java.util.logging.Logger logger = LogManager.getLogManager().getLogger(logName);
			Level originalLevel = logger.getLevel();
			java.util.logging.Logger parentLogger = logger.getParent();

			// Operation #1 --- getLoggerLevel(String)
			try {
				Object retVal = mbs.invoke(objName, "getLoggerLevel", new Object[] { logName },
						new String[] { String.class.getName() });
				AssertJUnit.assertNotNull(retVal);
				AssertJUnit.assertTrue(retVal instanceof String);
				if (originalLevel != null) {
					AssertJUnit.assertEquals(originalLevel.getName(), retVal);
				} else {
					AssertJUnit.assertEquals("", retVal);
				}
			} catch (MBeanException e) {
				Assert.fail("Unexpected MBeanException (getLoggerLevel): " + e.getCause().getMessage());
			} catch (ReflectionException e) {
				Assert.fail("Unexpected ReflectionException (getLoggerLevel): " + e.getCause().getMessage());
			} catch (NullPointerException n) {
				Assert.fail("Unexpected NullPointerException (getLoggerLevel): " + n.getCause().getMessage());
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException (getLoggerLevel): " + e.getCause().getMessage());
			}

			// Operation #2 --- getParentLoggerName(String)
			try {
				Object retVal = mbs.invoke(objName, "getParentLoggerName", new Object[] { logName },
						new String[] { String.class.getName() });
				AssertJUnit.assertNotNull(retVal);
				AssertJUnit.assertTrue(retVal instanceof String);
				if (parentLogger != null) {
					AssertJUnit.assertEquals(parentLogger.getName(), retVal);
				} else {
					AssertJUnit.assertEquals("", retVal);
				}
			} catch (MBeanException e) {
				Assert.fail("Unexpected MBeanException (getParentLoggerName): " + e.getCause().getMessage());
			} catch (ReflectionException e) {
				Assert.fail("Unexpected ReflectionException (getParentLoggerName): " + e.getCause().getMessage());
			} catch (NullPointerException n) {
				Assert.fail("Unexpected NullPointerException (getParentLoggerName): " + n.getCause().getMessage());
			} catch (InstanceNotFoundException e) {
				Assert.fail("Unexpected InstanceNotFoundException (getParentLoggerName): " + e.getCause().getMessage());
			}

			// Call getParentLoggerName(String) again with a bad argument type.
			try {
				Object retVal = lb.invoke("getParentLoggerName", new Object[] { new Long(311) },
						new String[] { Long.TYPE.getName() });
				Assert.fail("Should have thrown exception !!");
			} catch (ReflectionException e) {
				// This exception is expected.
			} catch (Exception e) {
				Assert.fail("Unexpected exception: " + e.getMessage());
			}

			// Operation #3 --- setLoggerLevel(String, String)
			try {
				Object retVal = lb.invoke("setLoggerLevel", new Object[] { logName, Level.SEVERE.getName() },
						new String[] { String.class.getName(), String.class.getName() });
				// Verify the set worked
				AssertJUnit.assertEquals(Level.SEVERE.getName(), logger.getLevel().getName());
			} catch (MBeanException e) {
				Assert.fail("Unexpected MBeanException (setLoggerLevel): " + e.getCause().getMessage());
			} catch (ReflectionException e) {
				Assert.fail("Unexpected ReflectionException (setLoggerLevel): " + e.getCause().getMessage());
			} catch (NullPointerException n) {
				Assert.fail("Unexpected NullPointerException (setLoggerLevel): " + n.getCause().getMessage());
			}
		}

		// Try invoking a bogus operation ...
		try {
			Object retVal = lb.invoke("GetUpStandUp", new Object[] { new Long(7446), new Long(54) },
					new String[] { "java.lang.Long", "java.lang.Long" });
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (ReflectionException e) {
			// This exception is expected.
			logger.debug("Exception occurred, as expected: " + e.getMessage());
		} catch (Exception e) {
			Assert.fail("Unexpected exception: " + e.getMessage());
		}
	}

	@Test
	public final void testGetMBeanInfo() {
		MBeanInfo mbi;
		try {
			mbi = mbs.getMBeanInfo(objName);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
			return;
		} catch (IntrospectionException e) {
			Assert.fail("Unexpected IntrospectionException: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		// Class name
		AssertJUnit.assertEquals(lb.getClass().getName(), mbi.getClassName());

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertEquals(0, constructors.length);

		// Three public operations
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertEquals(3, operations.length);

		// No notifications
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertEquals(0, notifications.length);

		// Print out the description here.
		logger.debug("MBean description for " + lb.getClass().getName() + ": " + mbi.getDescription());

		// One attribute - not writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertEquals(2, attributes.length);

		for (MBeanAttributeInfo attribute : attributes) {
			String attributeName = attribute.getName();
			String attributeType = attribute.getType();

			switch (attributeName) {
			case "LoggerNames":
				AssertJUnit.assertEquals(String[].class.getName(), attributeType);
				break;
			case "ObjectName":
				AssertJUnit.assertEquals(ObjectName.class.getName(), attributeType);
				break;
			default:
				Assert.fail("Unexpected attribute: " + attributeName + " of type " + attributeType);
				break;
			}

			AssertJUnit.assertTrue(attribute.isReadable());
			AssertJUnit.assertFalse(attribute.isWritable());
		}
	}

}
