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
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Map;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.Notification;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilterSupport;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.openmbean.CompositeData;

import com.ibm.lang.management.AvailableProcessorsNotificationInfo;
import com.ibm.lang.management.CpuLoadCalculationConstants;
import com.ibm.lang.management.ProcessingCapacityNotificationInfo;
import com.ibm.lang.management.TotalPhysicalMemoryNotificationInfo;

// These classes are not part of the public API.
import com.ibm.lang.management.internal.AvailableProcessorsNotificationInfoUtil;
import com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl;
import com.ibm.lang.management.internal.ProcessingCapacityNotificationInfoUtil;
import com.ibm.lang.management.internal.TotalPhysicalMemoryNotificationInfoUtil;

@Test(groups = { "level.sanity" })
public class TestOperatingSystemMXBean {

	private static Logger logger = Logger.getLogger(TestOperatingSystemMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	/* TODO: remove dependence on isSupportedOS when API becomes
	 * available on z/OS and OSX.
	 */
	private static final boolean isSupportedOS;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("Arch", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("AvailableProcessors", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("Name", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("SystemLoadAverage", new AttributeData(Double.TYPE.getName(), true, false, false));
		attribs.put("Version", new AttributeData(String.class.getName(), true, false, false));

		// IBM extensions
		attribs.put("TotalPhysicalMemory", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("TotalPhysicalMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessingCapacity", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("ProcessCpuTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessCpuTimeByNS", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SystemCpuLoad", new AttributeData(Double.TYPE.getName(), true, false, false));
		attribs.put("FreePhysicalMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessVirtualMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CommittedVirtualMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessPrivateMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessPhysicalMemorySize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("TotalSwapSpaceSize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("FreeSwapSpaceSize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("ProcessCpuLoad", new AttributeData(Double.TYPE.getName(), true, false, false));
		attribs.put("HardwareModel", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("HardwareEmulated", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("ProcessRunning", new AttributeData(Boolean.TYPE.getName(), true, false, true)); //$NON-NLS-1$

		/* When we run on Unix, the implementation returned is that of
		 * UnixExtendedOperatingSystem.  It adds a couple of APIs to the
		 * com.ibm.lang.management.OperatingSystemMXBean.
		 */
		if (TestUtil.isRunningOnUnix()) {
			attribs.put("MaxFileDescriptorCount", new AttributeData(Long.TYPE.getName(), true, false, false));
			attribs.put("OpenFileDescriptorCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		}
		/* At present, we don't have support for one of the APIs. Test what's
		 * there and exclude the other; enable when the API becomes available
		 * on Z and OSX.
		 */
		String osName = System.getProperty("os.name", "");
		if (osName.equalsIgnoreCase("z/OS") || osName.equalsIgnoreCase("mac os x")) {
			isSupportedOS = false;
		} else {
			isSupportedOS = true;
		}
	}// end static initializer

	private ExtendedOperatingSystemMXBeanImpl osb;

	private MBeanServer mbs;

	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		osb = (ExtendedOperatingSystemMXBeanImpl)ManagementFactory.getOperatingSystemMXBean();
		try {
			objName = new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up OperatingSystemMXBean test: " + me.getMessage());
		}
		logger.info("Starting TestOperatingSystemMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
		// do nothing
	}

	@Test
	public final void testGetArch() {
		AssertJUnit.assertTrue(osb.getArch() != null);
	}

	@Test
	public final void testGetAvailableProcessors() {
		AssertJUnit.assertTrue(osb.getAvailableProcessors() > 0);
	}

	@Test
	public final void testGetName() {
		AssertJUnit.assertTrue(osb.getName() != null);
	}

	@Test
	public final void testGetVersion() {
		AssertJUnit.assertTrue(osb.getVersion() != null);
	}

	@Test
	public final void testGetAttribute() {
		try {
			// The good attributes...
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Arch") != null);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Arch") instanceof String);
			AssertJUnit.assertTrue(((Integer)(mbs.getAttribute(objName, "AvailableProcessors"))) > -1);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Name") != null);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Name") instanceof String);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Version") != null);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Version") instanceof String);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SystemLoadAverage") instanceof Double);

			// The good IBM attributes ...
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "TotalPhysicalMemory") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessingCapacity") instanceof Integer);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessCpuTime") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "FreePhysicalMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessVirtualMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessPrivateMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessPhysicalMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "TotalPhysicalMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessCpuTimeByNS") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "SystemCpuLoad") instanceof Double);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "CommittedVirtualMemorySize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "TotalSwapSpaceSize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "FreeSwapSpaceSize") instanceof Long);
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ProcessCpuLoad") instanceof Double);

			if (TestUtil.isRunningOnUnix()) {
				AssertJUnit.assertTrue(mbs.getAttribute(objName, "MaxFileDescriptorCount") instanceof Long);
				AssertJUnit.assertTrue(mbs.getAttribute(objName, "OpenFileDescriptorCount") instanceof Long);
			}
		} catch (AttributeNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected MBeanException : " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = (Long) mbs.getAttribute(objName, "RPM");
			Assert.fail("Should have thrown an AttributeNotFoundException.");
		} catch (Exception e1) {
			// expected
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String) mbs.getAttribute(objName, "AvailableProcessors");
			Assert.fail("Should have thrown a ClassCastException");
		} catch (Exception e1) {
			// expected
		}
	}

	@Test
	public final void testSetAttribute() {
		// Nothing is writable for this type
		Attribute attr = new Attribute("Name", "Boris");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			// expected
		}

		attr = new Attribute("Arch", "ie Bunker");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			// expected
		}

		attr = new Attribute("Version", "27 and a half");
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			// expected
		}

		attr = new Attribute("AvailableProcessors", new Integer(2));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			// expected
		}

		// Try and set the Name attribute with an incorrect type.
		attr = new Attribute("Name", new Long(42));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e1) {
			// expected
		}
	}

	/**
	 * IBM attribute
	 */
	@Test
	public void testGetTotalPhysicalMemory() {
		AssertJUnit.assertTrue(osb.getTotalPhysicalMemory() > 0);
	}

	/**
	 * IBM attribute
	 */
	@Test
	public void testGetTotalPhysicalMemorySize() {
		AssertJUnit.assertTrue(osb.getTotalPhysicalMemorySize() > 0);
	}

	/**
	 * IBM attribute
	 */
	@Test(groups = { "disable.os.zos", "disable.os.aix" })
	public void testgetCommittedVirtualMemorySize() {
		AssertJUnit.assertTrue(osb.getCommittedVirtualMemorySize() > 0);
	}

	/**
	 * IBM attribute
	 */
	@Test
	public void testGetProcessingCapacity() {
		AssertJUnit.assertTrue(osb.getProcessingCapacity() > 0);
	}

	/**
	 * IBM attribute
	 */
	@Test(priority = -1) // priority is -1 so this test executes before any other
	public void testGetProcessCPULoad() {
		// first time it's -1.0
		AssertJUnit.assertTrue(osb.getProcessCpuLoad() == CpuLoadCalculationConstants.ERROR_VALUE);
		// sleep for a while otherwise next call will still return -1.0
		try {
			Thread.sleep(2000);
		} catch (Exception ex) {

		}
		double value = osb.getProcessCpuLoad();
		AssertJUnit.assertTrue(value >= 0 && value <= 1.0);
	}

	/**
	 * IBM attributes MaxFileDescriptorCount & OpenFileDescriptorCount.
	 */
	@Test
	public void testUnixFileDescriptorAPIs() {
		if (TestUtil.isRunningOnUnix()) {
			long maxFileDecriptors = 0;
			long openFileDecriptors = 0;
			/* Can't cast osb to UnixExtendedOperatingSystem as it is package-scope, to call
			 * the APIs as object-methods.  Instead, check the corresponding attributes.
			 */
			try {
				maxFileDecriptors = ((Long) mbs.getAttribute(objName, "MaxFileDescriptorCount")).longValue();
				openFileDecriptors = ((Long) mbs.getAttribute(objName, "OpenFileDescriptorCount")).longValue();
			} catch (AttributeNotFoundException e) {
				Assert.fail("Unexpected AttributeNotFoundException exception: " + e.getMessage());
			} catch (MBeanException e) {
				Assert.fail("Unexpected MBeanException exception: " + e.getMessage());
			} catch (ReflectionException e) {
				Assert.fail("Unexpected ReflectionException exception: " + e.getMessage());
			} catch (InstanceNotFoundException e) {
				// An unlikely exception - if this occurs, we can't proceed with the test.
				Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			}
			AssertJUnit.assertTrue((maxFileDecriptors > 0) && (maxFileDecriptors <= Long.MAX_VALUE));
			/* TODO: remove dependence on isSupportedOS when API becomes
			 * available on z/OS.
			 */
			if (isSupportedOS) {
				/* The number of file descriptors open, at any point, must be within the
				 * limits the underlying os configuration allows.
				 */
				AssertJUnit.assertTrue(maxFileDecriptors >= openFileDecriptors);
			}
			logger.debug("Maximum file descriptors allowed (ulimit -n): " + maxFileDecriptors);
			logger.debug("Current file descriptors in open state: " + openFileDecriptors);
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		if (!TestUtil.isRunningOnUnix()) {
			// On Unix, we have two more attributes that the call
			// mbs.getAttributes() cannot account for.  Check this.

			// for reasons not yet clear, the JMX mechanism does not
			// "see" the OperatingSystemMXBean APIs getHardwareModel() and isHardwareEmulated(),
			// which is why on Windows we see this discrepancy in attribute count.
			// Should be 19 attributes on non-Unices and 21 (the UnixOperatingSystem bean adds
			// 2 attributes).
			//AssertJUnit.assertEquals(attribs.size(), attributes.size());
		}

		// Check through the returned values
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (name.equals("Arch")) {
					AssertJUnit.assertTrue(value instanceof String);
					AssertJUnit.assertEquals(System.getProperty("os.arch"), (String)value);
				} else if (name.equals("AvailableProcessors")) {
					AssertJUnit.assertTrue(((Integer)(value)) > -1);
				} else if (name.equals("Name")) {
					AssertJUnit.assertTrue(value instanceof String);
					AssertJUnit.assertEquals(System.getProperty("os.name"), (String)value);
				} else if (name.equals("SystemLoadAverage")) {
					AssertJUnit.assertTrue(value instanceof Double);
				} else if (name.equals("Version")) {
					AssertJUnit.assertTrue(value instanceof String);
					AssertJUnit.assertEquals(System.getProperty("os.version"), (String)value);
				} else if (name.equals("TotalPhysicalMemory")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
				} else if (name.equals("TotalPhysicalMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > -1);
				} else if (name.equals("ProcessingCapacity")) {
					AssertJUnit.assertTrue(value instanceof Integer);
					AssertJUnit.assertTrue(((Integer)(value)) > -1);
				} else if (name.equals("MaxFileDescriptorCount")) {
					AssertJUnit.assertTrue(value instanceof Long);
					AssertJUnit.assertTrue(((Long)(value)) > 0);
				} else if (name.equals("ProcessCpuTime")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("ProcessCpuTimeByNS")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("SystemCpuLoad")) {
					AssertJUnit.assertTrue(value instanceof Double);
				} else if (name.equals("FreePhysicalMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("ProcessVirtualMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("CommittedVirtualMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("ProcessPrivateMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("ProcessPhysicalMemorySize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("TotalSwapSpaceSize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("FreeSwapSpaceSize")) {
					AssertJUnit.assertTrue(value instanceof Long);
				} else if (name.equals("ProcessCpuLoad")) {
					AssertJUnit.assertTrue(value instanceof Double);
				} else if (name.equals("HardwareModel")) {
					AssertJUnit.assertTrue(value instanceof String);
				} else if (name.equals("HardwareEmulated")) {
					AssertJUnit.assertTrue(value instanceof Boolean);
				} else if (name.equals("OpenFileDescriptorCount")) {
					AssertJUnit.assertTrue(value instanceof Long);
					/* TODO: remove dependence on isSupportedOS when API becomes
					 * available on z/OS and OSX.
					 */
					if (isSupportedOS) {
						AssertJUnit.assertTrue(((Long)value) > 0);
					}
				} else {
					Assert.fail("Unexpected attribute name returned! " + name);
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
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// No writable attributes for this type - should get a failure...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Name", "Waiting for the moon");
		badList.add(garbage);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testInvoke() {
		// OperatingSystemMXBean has no operations to invoke...
		try {
			Object retVal = mbs.invoke(objName, "DoTheRightThing", new Object[] { new Long(7446), new Long(54) },
					new String[] { "java.lang.Long", "java.lang.Long" });
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e) {
			// expected
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

		// Class name
		AssertJUnit.assertEquals(mbi.getClassName(), osb.getClass().getName());

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertEquals(0, constructors.length);

		// No public notifications but 3 in IBM extension
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertEquals(3, notifications.length);

		// 12 attributes - 5 standard and 14 from IBM.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);

		/* Under UNIX, we expect a couple more APIs, since the instance would be
		 * that of UnixExtendedOperatingSystem.
		 */
		if (TestUtil.isRunningOnUnix()) {
			AssertJUnit.assertEquals(24, attributes.length);
		} else {
			AssertJUnit.assertEquals(22, attributes.length);
		}
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestOperatingSystemMXBean.ignoredAttributes, attribs);
		} // end for
	}

	// -----------------------------------------------------------------
	// Notification implementation tests follow ....
	// -----------------------------------------------------------------

	@Test
	public void testAvailableProcessorsNotications() {
		// Register a listener
		NotificationFilterSupport filter = new NotificationFilterSupport();
		filter.enableType(AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE);
		MyTestListener listener = new MyTestListener();
		this.osb.addNotificationListener(listener, filter, null);

		// Fire off a notification and ensure that the listener receives it.
		try {
			AvailableProcessorsNotificationInfo info = new AvailableProcessorsNotificationInfo(2);
			CompositeData cd = AvailableProcessorsNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(
					AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Remove the listener
			osb.removeNotificationListener(listener, filter, null);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			osb.sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				osb.removeNotificationListener(listener, filter, null);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof ListenerNotFoundException);
			}
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
		} catch (ListenerNotFoundException e) {
			e.printStackTrace();
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
		}
	}

	@Test
	public void testProcessingCapacityNotications() {
		// Register a listener
		NotificationFilterSupport filter = new NotificationFilterSupport();
		filter.enableType(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE);
		MyTestListener listener = new MyTestListener();
		this.osb.addNotificationListener(listener, filter, null);

		// Fire off a notification and ensure that the listener receives it.
		try {
			ProcessingCapacityNotificationInfo info = new ProcessingCapacityNotificationInfo(50);
			CompositeData cd = ProcessingCapacityNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Remove the listener
			osb.removeNotificationListener(listener, filter, null);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			osb.sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				osb.removeNotificationListener(listener, filter, null);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof ListenerNotFoundException);
			}
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
		} catch (ListenerNotFoundException e) {
			e.printStackTrace();
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
		}
	}

	@Test
	public void testTotalPhysicalMemoryNotifications() {
		// Register a listener
		NotificationFilterSupport filter = new NotificationFilterSupport();
		filter.enableType(TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE);
		MyTestListener listener = new MyTestListener();
		this.osb.addNotificationListener(listener, filter, null);

		// Fire off a notification and ensure that the listener receives it.
		try {
			TotalPhysicalMemoryNotificationInfo info = new TotalPhysicalMemoryNotificationInfo(100 * 1024);
			CompositeData cd = TotalPhysicalMemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(
					TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Remove the listener
			osb.removeNotificationListener(listener, filter, null);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			osb.sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				osb.removeNotificationListener(listener, filter, null);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof ListenerNotFoundException);
			}
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
		} catch (ListenerNotFoundException e) {
			e.printStackTrace();
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
		}
	}

	@Test
	public final void testAddListenerForAvailableProcessorNotifications() {
		// Add a listener with a handback object.
		MyTestListener listener = new MyTestListener();
		ArrayList<String> arr = new ArrayList<String>();
		arr.add("Stone by stone in this big dark forest");
		osb.addNotificationListener(listener, null, arr);

		// Fire off a notification and ensure that the listener receives it.
		try {
			AvailableProcessorsNotificationInfo info = new AvailableProcessorsNotificationInfo(2);
			CompositeData cd = AvailableProcessorsNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(
					AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNotNull(listener.getHandback());
			AssertJUnit.assertSame(arr, listener.getHandback());
			ArrayList arr2 = (ArrayList)listener.getHandback();
			AssertJUnit.assertTrue(arr2.size() == 1);
			AssertJUnit.assertEquals("Stone by stone in this big dark forest", arr2.get(0));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public final void testAddListenerForProcessingCapacityNotifications() {
		// Add a listener with a handback object.
		MyTestListener listener = new MyTestListener();
		ArrayList<String> arr = new ArrayList<String>();
		arr.add("Socrates eats hemlock");
		osb.addNotificationListener(listener, null, arr);

		// Fire off a notification and ensure that the listener receives it.
		try {
			ProcessingCapacityNotificationInfo info = new ProcessingCapacityNotificationInfo(50);
			CompositeData cd = ProcessingCapacityNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNotNull(listener.getHandback());
			AssertJUnit.assertSame(arr, listener.getHandback());
			ArrayList arr2 = (ArrayList)listener.getHandback();
			AssertJUnit.assertTrue(arr2.size() == 1);
			AssertJUnit.assertEquals("Socrates eats hemlock", arr2.get(0));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public final void testAddListenerForTotalPhysicalMemoryNotifications() {
		// Add a listener with a handback object.
		MyTestListener listener = new MyTestListener();
		ArrayList<String> arr = new ArrayList<String>();
		arr.add("Radio ! Live transmission !");
		osb.addNotificationListener(listener, null, arr);

		// Fire off a notification and ensure that the listener receives it.
		try {
			TotalPhysicalMemoryNotificationInfo info = new TotalPhysicalMemoryNotificationInfo(100 * 1024);
			CompositeData cd = TotalPhysicalMemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(
					TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE,
					new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			this.osb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNotNull(listener.getHandback());
			AssertJUnit.assertSame(arr, listener.getHandback());
			ArrayList arr2 = (ArrayList)listener.getHandback();
			AssertJUnit.assertTrue(arr2.size() == 1);
			AssertJUnit.assertEquals("Radio ! Live transmission !", arr2.get(0));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public final void testGetNotificationInfo() {
		AssertJUnit.assertTrue(osb instanceof NotificationEmitter);
		NotificationEmitter emitter = (NotificationEmitter)osb;
		MBeanNotificationInfo[] notifications = emitter.getNotificationInfo();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertTrue(notifications.length == 3);
		for (int i = 0; i < notifications.length; i++) {
			MBeanNotificationInfo info = notifications[i];
			AssertJUnit.assertEquals(Notification.class.getName(), info.getName());
			String[] types = info.getNotifTypes();
			String description = info.getDescription();
			if (description.equals("Processing Capacity Notification")) {
				AssertJUnit.assertTrue(types.length == 1);
				AssertJUnit.assertTrue(types[0].equals(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE));
			} else if (description.equals("Total Physical Memory Notification")) {
				AssertJUnit.assertTrue(types.length == 1);
				AssertJUnit
						.assertTrue(types[0].equals(TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE));
			} else if (description.equals("Available Processors Notification")) {
				AssertJUnit.assertTrue(types.length == 1);
				AssertJUnit
						.assertTrue(types[0].equals(AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE));
			} else {
				Assert.fail("Unexpected notification description : " + description);
			}
		} // end for
	}

	@Test
	public void testGetSystemLoadAverage() throws Exception {
		double sla = osb.getSystemLoadAverage();
	}
}
