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

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Properties;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.MBeanException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.openmbean.TabularData;

/**
 * @brief Unit test case for testing functionality and structural makeup of the
 * RuntimeMXBean class.
 */
@Test(groups = { "level.sanity" })
public class TestRuntimeMXBean {

	private static Logger logger = Logger.getLogger(TestRuntimeMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("BootClassPath", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("ClassPath", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("InputArguments", new AttributeData("[Ljava.lang.String;", true, false, false));
		attribs.put("LibraryPath", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("ManagementSpecVersion", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("SpecName", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("SpecVendor", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("SpecVersion", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("StartTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SystemProperties", new AttributeData(TabularData.class.getName(), true, false, false));
		attribs.put("Uptime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("VmName", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("VmVendor", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("VmVersion", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("BootClassPathSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));

		// IBM specific APIs
		attribs.put("CPULoad", new AttributeData(Double.TYPE.getName(), true, false, false));
		attribs.put("ProcessID", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("VMGeneratedCPULoad", new AttributeData(Double.TYPE.getName(), true, false, false));
		attribs.put("VMIdleState", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("VMIdle", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("AttachApiInitialized", new AttributeData(Boolean.TYPE.getName(), true, false, true)); //$NON-NLS-1$
		attribs.put("AttachApiTerminated", new AttributeData(Boolean.TYPE.getName(), true, false, true)); //$NON-NLS-1$
		attribs.put("VmId", new AttributeData(String.class.getName(), true, false, false)); //$NON-NLS-1$
	}// end static initializer

	private RuntimeMXBean rb;

	private MBeanServer mbs;

	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		rb = ManagementFactory.getRuntimeMXBean();
		try {
			objName = new ObjectName(ManagementFactory.RUNTIME_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up RuntimeMXBean test: " + me.getMessage(), me);
		}
		logger.info("Starting TestRuntimeMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testGetAttribute() {
		try {
			logger.debug("ClassPath = " + mbs.getAttribute(objName, "ClassPath"));
			// The good attributes...
			if (rb.isBootClassPathSupported()) {
				AssertJUnit.assertNotNull(mbs.getAttribute(objName, "BootClassPath"));
				AssertJUnit.assertTrue(mbs.getAttribute(objName, "BootClassPath") instanceof String);
				logger.debug("Bootclasspath = " + mbs.getAttribute(objName, "BootClassPath"));
			} else {
				try {
					String bcp = (String)mbs.getAttribute(objName, "BootClassPath");
					Assert.fail("Should have thrown exception!");
				} catch (Exception e) {
					logger.debug("Exception is " + e.getMessage());
					AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
				}
			}

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ClassPath"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ClassPath") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "InputArguments"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "InputArguments") instanceof String[]);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "LibraryPath"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "LibraryPath") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ManagementSpecVersion"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ManagementSpecVersion") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "Name"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Name") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "SpecName"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SpecName") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "SpecVendor"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SpecVendor") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "SpecVersion"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SpecVersion") instanceof String);

			AssertJUnit.assertTrue(mbs.getAttribute(objName, "StartTime") instanceof Long);
			AssertJUnit.assertTrue(((Long)mbs.getAttribute(objName, "StartTime")) > -1);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "SystemProperties"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SystemProperties") instanceof TabularData);
			AssertJUnit.assertTrue(((TabularData)(mbs.getAttribute(objName, "SystemProperties"))).size() > 0);
			if (System.getSecurityManager() == null) {
				AssertJUnit.assertTrue(((TabularData)(mbs.getAttribute(objName, "SystemProperties"))).size() ==
						System.getProperties().size());
			} // end if no security manager

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "Uptime"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Uptime") instanceof Long);
			AssertJUnit.assertTrue((Long)mbs.getAttribute(objName, "Uptime") > -1);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "VmName"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "VmName") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "VmVendor"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "VmVendor") instanceof String);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "VmVersion"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "VmVersion") instanceof String);
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage(), e);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getCause().getMessage(), e);
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage(), e);
		} catch (Exception e) {
			Assert.fail("Unexpected Exception : " + e.getMessage(), e);
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = ((Long)(mbs.getAttribute(objName, "RPM")));
			Assert.fail("Should have thrown an AttributeNotFoundException.");
		} catch (Exception e1) {
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "TotalLoadedClassCount"));
			Assert.fail("Should have thrown a ClassCastException");
		} catch (Exception e2) {
		}
	}

	@Test
	public final void testGetBootClassPath() {
		if (rb.isBootClassPathSupported()) {
			AssertJUnit.assertTrue(rb.getBootClassPath() instanceof String);
			AssertJUnit.assertTrue(rb.getBootClassPath() != null);
		} else {
			try {
				String bcp = rb.getBootClassPath();
				Assert.fail("Should have thrown an unsupported operation exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testGetClassPath() {
		AssertJUnit.assertTrue(rb.getClassPath() instanceof String);
		AssertJUnit.assertTrue(rb.getClassPath() != null);
	}

	@Test
	public final void testGetLibraryPath() {
		AssertJUnit.assertTrue(rb.getLibraryPath() instanceof String);
		AssertJUnit.assertTrue(rb.getLibraryPath() != null);
	}

	@Test
	public final void testGetManagementSpecVersion() {
		AssertJUnit.assertTrue(rb.getManagementSpecVersion() instanceof String);
		AssertJUnit.assertTrue(rb.getManagementSpecVersion() != null);
	}

	@Test
	public final void testGetName() {
		AssertJUnit.assertTrue(rb.getName() instanceof String);
		AssertJUnit.assertTrue(rb.getName() != null);
	}

	@Test
	public final void testGetSpecName() {
		AssertJUnit.assertTrue(rb.getSpecName() instanceof String);
		AssertJUnit.assertTrue(rb.getSpecName() != null);
	}

	@Test
	public final void testGetSpecVendor() {
		AssertJUnit.assertTrue(rb.getSpecVendor() instanceof String);
		AssertJUnit.assertTrue(rb.getSpecVendor() != null);
	}

	@Test
	public final void testGetSpecVersion() {
		AssertJUnit.assertTrue(rb.getSpecVersion() instanceof String);
		AssertJUnit.assertTrue(rb.getSpecVersion() != null);
	}

	@Test
	public final void testGetStartTime() {
		AssertJUnit.assertTrue(rb.getStartTime() > -1);
	}

	@Test
	public final void testGetUptime() {
		AssertJUnit.assertTrue(rb.getUptime() > -1);
	}

	@Test
	public final void testGetVmName() {
		AssertJUnit.assertTrue(rb.getVmName() instanceof String);
		AssertJUnit.assertTrue(rb.getVmName() != null);
	}

	@Test
	public final void testGetVmVendor() {
		AssertJUnit.assertTrue(rb.getVmVendor() instanceof String);
		AssertJUnit.assertTrue(rb.getVmVendor() != null);
	}

	@Test
	public final void testGetVmVersion() {
		AssertJUnit.assertTrue(rb.getVmVersion() instanceof String);
		AssertJUnit.assertTrue(rb.getVmVersion() != null);
	}

	@Test
	public final void testIsBootClassPathSupported() {
		try {
			boolean value = rb.isBootClassPathSupported();
		} catch (Exception e) {
			Assert.fail("Unexpected exception." , e);
		}
	}

	@Test
	public final void testGetInputArguments() {
		List<String> args = rb.getInputArguments();
		// Should always return a List<String> object. If no
		// input args then should return an empty List<String>.
		AssertJUnit.assertNotNull(args);
		AssertJUnit.assertTrue(args instanceof List);
		Iterator<String> it = args.iterator();
		while (it.hasNext()) {
			String element = it.next();
			AssertJUnit.assertNotNull(element);
			AssertJUnit.assertTrue(element.length() > 0);
		}
	}

	@Test
	public final void testGetSystemProperties() {
		Map<String, String> props = rb.getSystemProperties();
		AssertJUnit.assertNotNull(props);
		AssertJUnit.assertTrue(props instanceof Map);
		AssertJUnit.assertTrue(props.size() > 0);

		if (System.getSecurityManager() == null) {
			AssertJUnit.assertTrue(props.size() == System.getProperties().size());
		}
	}

	@Test
	public final void testSetAttribute() {
		// Nothing is writable for this type
		Attribute attr = new Attribute("BootClassPath", "Boris");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("BootClassPath", "Pasternak");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("InputArguments", new ArrayList<String>());
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("LibraryPath", "Sterling Morrison");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("ManagementSpecVersion", "Moe Tucker");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("Name", "Julian Cope");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("SpecName", "Andy Partridge");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("SpecVendor", "Siouxie Sioux");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("SpecVersion", "Ari Up");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("StartTime", new Long(2333));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("SystemProperties", new HashMap<String, String>());
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("Uptime", new Long(1979));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("VmName", "Joe Strummer");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("VmVendor", "Paul Haig");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("VmVersion", "Jerry Dammers");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		attr = new Attribute("BootClassPathSupported", new Boolean(false));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
		}

		// Try and set the Name attribute with an incorrect type.
		attr = new Attribute("Name", new Long(42));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e2) {
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage(), e);
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage(), e);
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.size() <= attribs.size());

		// Check through the returned values
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (attribs.containsKey(name)) {
					if (attribs.get(name).type.equals(String.class.getName())) {
						AssertJUnit.assertNotNull(value);
						AssertJUnit.assertTrue(value instanceof String);
					} // end if a String value expected
					else if (attribs.get(name).type.equals(Long.TYPE.getName())) {
						AssertJUnit.assertTrue(((Long)(value)) > -1);
					} // end else a long expected
					else if (attribs.get(name).type.equals(Boolean.TYPE.getName())) {
						boolean tmp = ((Boolean)value).booleanValue();
					} // end else a boolean expected
					else if (attribs.get(name).type.equals("[Ljava.lang.String;")) {
						String[] tmp = (String[])value;
						AssertJUnit.assertNotNull(tmp);
					} // end else a String array expected
					else if (attribs.get(name).type.equals(TabularData.class.getName())) {
						AssertJUnit.assertNotNull(value);
						AssertJUnit.assertTrue(value instanceof TabularData);
						// Sanity check on the contents of the returned
						// TabularData instance. Only one attribute of the
						// RuntimeMXBean returns a TabularDataType -
						// the SystemProperties.
						TabularData td = (TabularData)value;
						AssertJUnit.assertTrue(td.size() > 0);
						if (System.getSecurityManager() == null) {
							Properties props = System.getProperties();
							AssertJUnit.assertTrue(td.size() == props.size());
							Enumeration<?> propNames = props.propertyNames();
							while (propNames.hasMoreElements()) {
								String property = (String)propNames.nextElement();
								String propVal = props.getProperty(property);
								AssertJUnit.assertEquals(propVal, td.get(new String[] { property }).get("value"));
							} // end while
						} // end if no security manager
					} // end else a String array expected
				} // end if a known attribute
				else {
					Assert.fail("Unexpected attribute name returned!");
				} // end else an unknown attribute
			} catch (Exception e) {
				Assert.fail("Unexpected exception thrown : " + e.getMessage(), e);
			}
		} // end while

		// A failing scenario - pass in an attribute that is not part of
		// the management interface.
		String[] badNames = { "Cork", "Galway" };
		try {
			attributes = mbs.getAttributes(objName, badNames);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage(), e);
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage(), e);
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// No writable attributes for this type - should get a failure...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Name", "City Sickness");
		Attribute trash = new Attribute("SpecVendor", "Marbles");
		badList.add(garbage);
		badList.add(trash);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage(), e);
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage(), e);
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testInvoke() {
		// RuntimeMXBean has no operations to invoke...
		try {
			Object retVal = null;
			try {
				retVal = mbs.invoke(objName, "DoTheStrand", new Object[] { new Long(7446), new Long(54) },
						new String[] { "java.lang.Long", "java.lang.Long" });
			} catch (InstanceNotFoundException e) {
				// An unlikely exception - if this occurs, we can't proceed with the test.
				Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage(), e);
				return;
			}
			// An exception other then InstanceNotFoundException should have been thrown,
			// since there aren't any operations and we just tried to invoke one.
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (Exception e) {
			logger.debug("Exception occurred, as expected: " + e.getMessage());
		}
	}

	@Test
	public final void testGetMBeanInfo() {
		MBeanInfo mbi = null;
		try {
			mbi = mbs.getMBeanInfo(objName);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage(), e);
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage(), e);
			return;
		} catch (IntrospectionException e) {
			Assert.fail("Unexpected IntrospectionException : " + e.getMessage(), e);
			return;
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		// Class name
		AssertJUnit.assertTrue(mbi.getClassName().equals(rb.getClass().getName()));

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

		// Print description and the class name (not necessarily identical).
		logger.debug("MBean description for " + rb.getClass().getName() + ": " + mbi.getDescription());

		// Sixteen attributes - none writable - and eight IBM specific.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		org.testng.Assert.assertEquals(25, attributes.length, "wrong number of attributes");
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestRuntimeMXBean.ignoredAttributes, attribs);
		} // end for
	}

	@Test
	public final void testVMIdleState() {
		AssertJUnit.assertTrue(com.ibm.lang.management.RuntimeMXBean.VMIdleStates.INVALID != ((com.ibm.lang.management.RuntimeMXBean)rb).getVMIdleState());
	}
}
