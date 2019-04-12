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
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryUsage;
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
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.Notification;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilterSupport;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.openmbean.CompositeData;

import com.ibm.lang.management.MemoryMXBean;

import org.openj9.test.util.process.Task;

// These classes are not public API.
import com.ibm.java.lang.management.internal.MemoryNotificationInfoUtil;
import com.ibm.java.lang.management.internal.MemoryUsageUtil;
import com.ibm.lang.management.internal.ExtendedMemoryMXBeanImpl;

@Test(groups = { "level.sanity" })
public class TestMemoryMXBean {

	private static Logger logger = Logger.getLogger(TestMemoryMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		// Attributes corresponding with the standard (JLM) class API.
		attribs.put("HeapMemoryUsage", new AttributeData(CompositeData.class.getName(), true, false, false));
		attribs.put("NonHeapMemoryUsage", new AttributeData(CompositeData.class.getName(), true, false, false));
		attribs.put("ObjectPendingFinalizationCount", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("Verbose", new AttributeData(Boolean.TYPE.getName(), true, true, true));

		// IBM specific APIs added to the type.
		attribs.put("MaxHeapSizeLimit", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("MaxHeapSize", new AttributeData(Long.TYPE.getName(), true, true, false));
		attribs.put("MinHeapSize", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SetMaxHeapSizeSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("SharedClassCacheSize", new AttributeData(Long.TYPE.getName(), true, false, false));
		/* The next five attributes correspond with APIs that have both, a getter and
		 * a setter.  However, the setter API have a signature that returns boolean,
		 * and hence, cannot be considered as attributes with "writable=true".  They
		 * must be placed under operations, instead.
		 */
		attribs.put("SharedClassCacheSoftmxBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMinAotBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMaxAotBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMinJitDataBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMaxJitDataBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheSoftmxUnstoredBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMaxAotUnstoredBytes", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheMaxJitDataUnstoredBytes",
				new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("SharedClassCacheFreeSpace", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("GCMode", new AttributeData(String.class.getName(), true, false, false));
		attribs.put("GCMasterThreadCpuUsed", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("GCSlaveThreadsCpuUsed", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("MaximumGCThreads", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("CurrentGCThreads", new AttributeData(Integer.TYPE.getName(), true, false, false));
	}// end static initializer

	private ExtendedMemoryMXBeanImpl mb;
	private MBeanServer mbs;
	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		mb = (ExtendedMemoryMXBeanImpl)ManagementFactory.getMemoryMXBean();
		try {
			objName = new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up MemoryMXBeanImpl test: " + me.getMessage());
		}
		logger.info("Starting TestMemoryMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	static class ClassForTestMaxHeapSize implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			Thread.sleep(2000);
			long size = bean.getMinHeapSize();
			bean.setMaxHeapSize(size + 1024);
			if (size + 1024 != bean.getMaxHeapSize()) {
				throw new RuntimeException("not equal");
			}
		}
	}

	@Test
	public final void testGetAttribute() {
		try {
			// The good attributes...
			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "HeapMemoryUsage"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "HeapMemoryUsage") instanceof CompositeData);
			AssertJUnit.assertTrue(
					((CompositeData)(mbs.getAttribute(objName, "HeapMemoryUsage"))).containsKey("committed"));

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "NonHeapMemoryUsage"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "NonHeapMemoryUsage") instanceof CompositeData);
			AssertJUnit
					.assertTrue(((CompositeData)(mbs.getAttribute(objName, "NonHeapMemoryUsage"))).containsKey("max"));

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ObjectPendingFinalizationCount"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ObjectPendingFinalizationCount") instanceof Integer);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "Verbose"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "Verbose") instanceof Boolean);

			// The IBM attributes...
			Long maxHSL = (Long)mbs.getAttribute(objName, "MaxHeapSizeLimit");
			AssertJUnit.assertNotNull(maxHSL);
			AssertJUnit.assertTrue(maxHSL > 0);
			logger.debug("Max heap size limit : " + maxHSL);

			Long maxHS = (Long)mbs.getAttribute(objName, "MaxHeapSize");
			AssertJUnit.assertNotNull(maxHS);
			AssertJUnit.assertTrue(maxHS > 0);
			logger.debug("Max heap size : " + maxHS);

			Long minHS = (Long)mbs.getAttribute(objName, "MinHeapSize");
			AssertJUnit.assertNotNull(minHS);
			AssertJUnit.assertTrue(minHS > 0);
			logger.debug("Min heap size : " + minHS);

			Boolean supported = (Boolean)mbs.getAttribute(objName, "SetMaxHeapSizeSupported");
			AssertJUnit.assertNotNull(supported);
			logger.debug("Set max heap size supported : " + supported);

			String gcMode = (String)mbs.getAttribute(objName, "GCMode");
			AssertJUnit.assertNotNull(gcMode);
			AssertJUnit.assertTrue(gcMode.length() > 0);
			logger.debug("GC Mode : " + gcMode);

			Long sharedCacheSize = (Long)mbs.getAttribute(objName, "SharedClassCacheSize");
			AssertJUnit.assertNotNull(sharedCacheSize);
			AssertJUnit.assertTrue(sharedCacheSize > -1);
			logger.debug("Shared class cache size : " + sharedCacheSize);

			Long sharedCacheFreeSpace = (Long)mbs.getAttribute(objName, "SharedClassCacheFreeSpace");
			AssertJUnit.assertNotNull(sharedCacheFreeSpace);
			AssertJUnit.assertTrue(sharedCacheFreeSpace > -1);
			logger.debug("Shared class cache free space : " + sharedCacheFreeSpace);
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		}

		// Now try fetching a nonexistent attribute; should throw an AttributeNotFoundException.
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
			String bad = (String)(mbs.getAttribute(objName, "HeapMemoryUsage"));
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
			logger.debug("Exception occurred, as expected: " + "attempting to perform invalid type cast on attribute.");
		}
	}

	@Test
	public final void testSetAttribute() {
		// The only writable attribute of this type of bean
		Attribute attr = null;
		try {
			attr = new Attribute("Verbose", new Boolean(true));
			mbs.setAttribute(objName, attr);
		} catch (AttributeNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected AttributeNotFoundException " + e.getMessage());
		} catch (InvalidAttributeValueException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InvalidAttributeValueException occurred (Verbose): " + e.getMessage());
		} catch (MBeanException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected MBeanException occurred (Verbose): " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (Verbose): " + e.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred (Verbose): " + e.getMessage());
		}

		// Now check that the other, readonly attributes can't be set.
		MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
		CompositeData cd = MemoryUsageUtil.toCompositeData(mu);
		attr = new Attribute("HeapMemoryUsage", cd);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred (HeapMemoryUsage): " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (HeapMemoryUsage): " + e.getMessage());
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + e1.getMessage());
		}

		attr = new Attribute("NonHeapMemoryUsage", cd);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred (NonHeapMemoryUsage): " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (NonHeapMemoryUsage): " + e.getMessage());
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof javax.management.AttributeNotFoundException);
			logger.debug("Exception occurred, as expected: " + e1.getMessage());
		}

		attr = new Attribute("ObjectPendingFinalizationCount", new Long(38));
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Unreacheable code: should have thrown an exception.");
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred (ObjectPendingFinalizationCount): "
					+ e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (ObjectPendingFinalizationCount): " + e.getMessage());
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
			Assert.fail("Unexpected InstanceNotFoundException occurred (Verbose): " + e.getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (Verbose): " + e.getMessage());
		} catch (Exception e2) {
			AssertJUnit.assertTrue(e2 instanceof javax.management.InvalidAttributeValueException);
			logger.debug("Exception occurred, as expected: " + e2.getMessage());
		}

		//set Verbose back to false
		try {
			attr = new Attribute("Verbose", new Boolean(false));
			mbs.setAttribute(objName, attr);
		} catch (AttributeNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected AttributeNotFoundException " + e.getMessage());
		} catch (InvalidAttributeValueException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InvalidAttributeValueException occurred (Verbose): " + e.getMessage());
		} catch (MBeanException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected MBeanException occurred (Verbose): " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (Verbose): " + e.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred (Verbose): " + e.getMessage());
		}
	}

	@Test
	public final void testInvoke() {
		// Only one operation for the standard (JLM) bean.
		Object retVal;
		try {
			retVal = mbs.invoke(objName, "gc", new Object[] {}, null);
			AssertJUnit.assertNull(retVal);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (gc): " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException  occurred (gc): " + e.getCause().getMessage());
		} catch (NullPointerException n) {
			Assert.fail("Unexpected NullPointerException  occurred (gc): " + n.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException  occurred (gc): " + e.getCause().getMessage());
		}

		// Five more operations which IBM adds.
		try {
			// Test the operation setSharedClassCacheSoftmxBytes
			long softMxBytes = mb.getSharedClassCacheSoftmxBytes();
			// Just in case the shared cache has run out of space, ensure we set
			// it to a sufficiently large size, before testing min/max AOT and JIT.
			if (mb.getSharedClassCacheFreeSpace() == 0) {
				softMxBytes = 2 * softMxBytes;
			}
			// Just that an attempt to set Shared Class Cache parameter through
			// operation invocation (rather than direct method call) works / does
			// not throw an exception.
			retVal = (Boolean)mbs.invoke(objName, "setSharedClassCacheSoftmxBytes", new Object[] { softMxBytes },
					new String[] { long.class.getName() });
			AssertJUnit.assertTrue(((Boolean)(retVal)).booleanValue() == true);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (setSharedClassCacheSoftmxBytes): "
					+ e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred (setSharedClassCacheSoftmxBytes): "
					+ e.getCause().getMessage());
		} catch (NullPointerException npe) {
			Assert.fail("Unexpected NullPointerException occurred (setSharedClassCacheSoftmxBytes): "
					+ npe.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred (setSharedClassCacheSoftmxBytes): "
					+ e.getCause().getMessage());
		}

		try {
			// Test the operation setSharedClassCacheMinAotBytes
			long minAotBytes = mb.getSharedClassCacheMinAotBytes();
			// If this wasn't set, try setting it to some nominal size
			// so as to test the doability of invoke on the operation.
			if (minAotBytes == -1) {
				minAotBytes = 1024;
			}
			// Just that an attempt to set Shared Class Cache parameter through
			// operation invocation (rather than direct method call) works / does
			// not throw an exception.
			retVal = (Boolean)mbs.invoke(objName, "setSharedClassCacheMinAotBytes", new Object[] { minAotBytes },
					new String[] { long.class.getName() });
			AssertJUnit.assertTrue(((Boolean)(retVal)).booleanValue() == true);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (setSharedClassCacheMinAotBytes): "
					+ e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred (setSharedClassCacheMinAotBytes): "
					+ e.getCause().getMessage());
		} catch (NullPointerException n) {
			Assert.fail("Unexpected NullPointerException occurred (setSharedClassCacheMinAotBytes): "
					+ n.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred (setSharedClassCacheMinAotBytes): "
					+ e.getCause().getMessage());
		}

		try {
			// Test the operation setSharedClassCacheMaxAotBytes
			long maxAotBytes = mb.getSharedClassCacheMaxAotBytes();
			// If this wasn't set, try setting it to some nominal size
			// so as to test the doability of invoke on the operation.
			if (maxAotBytes == -1) {
				maxAotBytes = 2048;
			}
			// Just that an attempt to set Shared Class Cache parameter through
			// operation invocation (rather than direct method call) works / does
			// not throw an exception.
			retVal = (Boolean)mbs.invoke(objName, "setSharedClassCacheMaxAotBytes", new Object[] { maxAotBytes },
					new String[] { long.class.getName() });
			AssertJUnit.assertTrue(((Boolean)(retVal)).booleanValue() == true);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (setSharedClassCacheMaxAotBytes): "
					+ e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred (setSharedClassCacheMaxAotBytes): "
					+ e.getCause().getMessage());
		} catch (NullPointerException npe) {
			Assert.fail("Unexpected NullPointerException occurred (setSharedClassCacheMaxAotBytes): "
					+ npe.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred (setSharedClassCacheMaxAotBytes): "
					+ e.getCause().getMessage());
		}

		try {
			// Test the operation setSharedClassCacheMinJitDataBytes
			long minJitBytes = mb.getSharedClassCacheMinJitDataBytes();
			// If this wasn't set, try setting it to some nominal size
			// so as to test the doability of invoke on the operation.
			if (minJitBytes == -1) {
				minJitBytes = 1024;
			}
			// Just that an attempt to set Shared Class Cache parameter through
			// operation invocation (rather than direct method call) works / does
			// not throw an exception.
			retVal = (Boolean)mbs.invoke(objName, "setSharedClassCacheMinJitDataBytes", new Object[] { minJitBytes },
					new String[] { long.class.getName() });
			AssertJUnit.assertTrue(((Boolean)(retVal)).booleanValue() == true);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (setSharedClassCacheMinJitDataBytes): "
					+ e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred (setSharedClassCacheMinJitDataBytes): "
					+ e.getCause().getMessage());
		} catch (NullPointerException npe) {
			Assert.fail("Unexpected NullPointerException occurred (setSharedClassCacheMinJitDataBytes): "
					+ npe.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred (setSharedClassCacheMinJitDataBytes): "
					+ e.getCause().getMessage());
		}

		try {
			// Test the operation setSharedClassCacheMaxJitDataBytes
			long maxJitBytes = mb.getSharedClassCacheMaxJitDataBytes();
			// If this wasn't set, try setting it to some nominal size
			// so as to test the doability of invoke on the operation.
			if (maxJitBytes == -1) {
				maxJitBytes = 2048;
			}
			// Just that an attempt to set Shared Class Cache parameter through
			// operation invocation (rather than direct method call) works / does
			// not throw an exception.
			retVal = (Boolean)mbs.invoke(objName, "setSharedClassCacheMaxJitDataBytes", new Object[] { maxJitBytes },
					new String[] { long.class.getName() });
			AssertJUnit.assertTrue(((Boolean)(retVal)).booleanValue() == true);
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException occurred (setSharedClassCacheMaxJitDataBytes): "
					+ e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred (setSharedClassCacheMaxJitDataBytes): "
					+ e.getCause().getMessage());
		} catch (NullPointerException npe) {
			Assert.fail("Unexpected NullPointerException occurred (setSharedClassCacheMaxJitDataBytes): "
					+ npe.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred (setSharedClassCacheMaxJitDataBytes): "
					+ e.getCause().getMessage());
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.size() == attribs.size());

		// Check through the returned values
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (attribs.containsKey(name)) {
					if (attribs.get(name).type.equals(Integer.TYPE.getName())) {
						AssertJUnit.assertNotNull(value);
						AssertJUnit.assertTrue(value instanceof Integer);
						AssertJUnit.assertTrue((Integer)value > -1);
					} // end if a String value expected
					else if (attribs.get(name).type.equals(Boolean.TYPE.getName())) {
						boolean tmp = ((Boolean)value).booleanValue();
					} // end else a long expected
					else if (attribs.get(name).type.equals(CompositeData.class.getName())) {
						AssertJUnit.assertNotNull(value);
						AssertJUnit.assertTrue(value instanceof CompositeData);
						// Sanity check on the contents of the returned
						// CompositeData instance. For this kind of bean
						// the "wrapped" type must be a MemoryUsage.
						CompositeData cd = (CompositeData)value;
						AssertJUnit.assertTrue(cd.containsKey("committed"));
						AssertJUnit.assertTrue(cd.containsKey("init"));
						AssertJUnit.assertTrue(cd.containsKey("max"));
						AssertJUnit.assertTrue(cd.containsKey("used"));
						AssertJUnit.assertFalse(cd.containsKey("trash"));
					} // end else a String array expected
				} // end if a known attribute
				else {
					Assert.fail("Unexpected attribute name returned!");
				} // end else an unknown attribute
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
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public void testSetIBMAttribute() {
		long originalHeapSize = mb.getMaxHeapSize();
		long newHeapSize = 0;
		long delta = mb.getMaxHeapSizeLimit() - originalHeapSize;
		// Try expanding the upper bounds on maximum heap only if there is
		// room for it (getMaxHeapSizeLimit() exceeds getMaxHeapSize()), or
		// else, trying to set a new heap maximum fails (returns 0 attrs)
		// due to an IllegalArgumentException (see API setMaxHeapSize()).
		if (delta > 0) {
			newHeapSize = originalHeapSize + delta;
		} else {
			// Can't increase; try decreasing.  If that too isn't possible,
			// just stay put with current heap limit.
			delta = originalHeapSize - mb.getMinHeapSize();
			if (delta > 0) {
				newHeapSize = originalHeapSize - delta;
			} else {
				newHeapSize = originalHeapSize;
			}
		}
		AttributeList attList = new AttributeList();
		Attribute heapSize = new Attribute("MaxHeapSize", new Long(newHeapSize));
		attList.add(heapSize);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, attList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred (MaxHeapSize): " + e.getMessage());
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexception occured (MaxHeapSize): " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		if (mb.isSetMaxHeapSizeSupported()) {
			// Only one setter; there are a whole bunch of set*() APIs that IBM adds
			// but unfortunately, they aren't attributes (have return values).
			AssertJUnit.assertTrue(setAttrs.size() == 1);
			AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getName().equals("MaxHeapSize"));
			AssertJUnit.assertEquals(newHeapSize, mb.getMaxHeapSize());
		} else {
			AssertJUnit.assertTrue(setAttrs.size() == 0);
		} // end else
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
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 1);
		AssertJUnit.assertTrue(((Attribute)(setAttrs.get(0))).getName().equals("Verbose"));

		// A failure scenario - a non-existent attribute...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("H.R. Puffenstuff", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Another failure scenario - a non-writable attribute...
		badList = new AttributeList();
		garbage = new Attribute("ObjectPendingFinalizationCount", new Integer(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
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
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
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
			return;
		} catch (IntrospectionException e) {
			Assert.fail("Unexpected IntrospectionException : " + e.getMessage());
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		// Class name
		AssertJUnit.assertTrue(mbi.getClassName().equals(mb.getClass().getName()));

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertTrue(constructors.length == 0);

		// One public operation (JLM) + five from CILM.
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertTrue(operations.length == 6);

		// One notification
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertTrue(notifications.length == 1);

		// Print description and the class name (not necessarily identical).
		logger.debug("MBean description for " + mb.getClass().getName() + ": " + mbi.getDescription());

		// Eight attributes - some writable.
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attributes.length == 24);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestMemoryMXBean.ignoredAttributes, attribs);
		} // end for
	}

	@Test
	public final void testGc() {
		// Test that the bean has a gc method.
		try {
			mb.gc();
		} catch (Exception e) {
			Assert.fail("Unexpected exception!");
		}
	}

	@Test
	public final void testGetHeapMemoryUsage() {
		AssertJUnit.assertNotNull(mb.getHeapMemoryUsage());
		AssertJUnit.assertTrue(mb.getHeapMemoryUsage() instanceof MemoryUsage);
	}

	@Test
	public final void testGetNonHeapMemoryUsage() {
		AssertJUnit.assertNotNull(mb.getNonHeapMemoryUsage());
		AssertJUnit.assertTrue(mb.getNonHeapMemoryUsage() instanceof MemoryUsage);
		AssertJUnit.assertTrue(mb.getNonHeapMemoryUsage().getCommitted() > -1);
		AssertJUnit.assertTrue(mb.getNonHeapMemoryUsage().getUsed() > -1);
	}

	@Test
	public final void testGetObjectPendingFinalizationCount() {
		AssertJUnit.assertTrue(mb.getObjectPendingFinalizationCount() > -1);
	}

	@Test
	public final void testIsVerbose() {
		// TODO Set - test - reset - test when VM permits this
	}

	@Test
	public final void testSetVerbose() {
		// TODO Set - test - reset - test when VM permits this
	}

	@Test
	public void testSetMaxHeapSize() {
		long originalHeapSize = mb.getMaxHeapSize();
		long newHeapSize = originalHeapSize + (1024);
		if (mb.isSetMaxHeapSizeSupported()) {
			try {
				mb.setMaxHeapSize(newHeapSize);
				AssertJUnit.assertEquals(newHeapSize, mb.getMaxHeapSize());
			} catch (IllegalArgumentException i) {

			}
		} else {
			try {
				mb.setMaxHeapSize(newHeapSize);
				Assert.fail("Should have thrown an exception");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		} // end else
	}

	@Test
	public void testGetGCMode() {
		String mode = mb.getGCMode();
		AssertJUnit.assertNotNull(mode);
		AssertJUnit.assertTrue(mode.length() > 0);
	}

	/**
	 * Test the getSharedClassCacheSize() API.
	 */
	@Test
	public void testGetSharedClassCacheSize() {
		try {
			long val = mb.getSharedClassCacheSize();
			AssertJUnit.assertTrue(val > -1);
			logger.debug("Shared class cache size = " + val);
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	/**
	 * Test the getSharedClassCacheFreeSpace() API.
	 */
	@Test
	public void testGetSharedClassCacheFreeSpace() {
		try {
			long val = mb.getSharedClassCacheFreeSpace();
			AssertJUnit.assertTrue(val > -1);
			logger.debug("Shared class cache free space = " + val);
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	// -----------------------------------------------------------------
	// Notification implementation tests follow ....
	// -----------------------------------------------------------------
	/*
	 * Class under test for void
	 * removeNotificationListener(NotificationListener, NotificationFilter,
	 * Object)
	 */
	@Test
	public final void testRemoveNotificationListenerNotificationListenerNotificationFilterObject() {
		// Register a listener
		NotificationFilterSupport filter = new NotificationFilterSupport();
		filter.enableType(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED);
		MyTestListener listener = new MyTestListener();
		mb.addNotificationListener(listener, filter, null);

		// Fire off a notification and ensure that the listener receives it.
		try {
			MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
			MemoryNotificationInfo info = new MemoryNotificationInfo("Tim", mu, 42);
			CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			mb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Remove the listener
			mb.removeNotificationListener(listener, filter, null);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			mb.sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				mb.removeNotificationListener(listener, filter, null);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (ListenerNotFoundException e) {
			}
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		} catch (ListenerNotFoundException e) {
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public final void testAddNotificationListener() {
		// Add a listener with a handback object.
		MyTestListener listener = new MyTestListener();
		ArrayList<String> arr = new ArrayList<String>();
		arr.add("Hegemony or survival ?");
		mb.addNotificationListener(listener, null, arr);

		// Fire off a notification and ensure that the listener receives it.
		try {
			MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
			MemoryNotificationInfo info = new MemoryNotificationInfo("Tim", mu, 42);
			CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			mb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNotNull(listener.getHandback());
			AssertJUnit.assertSame(arr, listener.getHandback());
			ArrayList arr2 = (ArrayList)listener.getHandback();
			AssertJUnit.assertTrue(arr2.size() == 1);
			AssertJUnit.assertEquals("Hegemony or survival ?", arr2.get(0));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/*
	 * Class under test for void
	 * removeNotificationListener(NotificationListener)
	 */
	@Test
	public final void testRemoveNotificationListenerNotificationListener() {
		// Add a listener without a filter object.
		MyTestListener listener = new MyTestListener();
		mb.addNotificationListener(listener, null, null);
		// Fire off a notification and ensure that the listener receives it.
		try {
			MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
			MemoryNotificationInfo info = new MemoryNotificationInfo("Tim", mu, 42);
			CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			mb.sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNull(listener.getHandback());

			// Verify the user data of the notification.
			Notification n = listener.getNotification();
			AssertJUnit.assertNotNull(n);
			verifyNotificationUserData(n.getUserData());

			// Remove the listener
			mb.removeNotificationListener(listener);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			mb.sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				mb.removeNotificationListener(listener);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (ListenerNotFoundException e) {
			}
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		} catch (ListenerNotFoundException e) {
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/**
	 * @param userData
	 */
	private void verifyNotificationUserData(Object userData) {
		// Should be a CompositeData instance
		AssertJUnit.assertTrue(userData instanceof CompositeData);
		CompositeData cd = (CompositeData)userData;
		AssertJUnit.assertTrue(cd.containsKey("poolName"));
		AssertJUnit.assertTrue(cd.containsKey("usage"));
		AssertJUnit.assertTrue(cd.containsKey("count"));
		AssertJUnit.assertTrue(cd.get("poolName") instanceof String);
		AssertJUnit.assertTrue(((String)cd.get("poolName")).length() > 0);
		AssertJUnit.assertTrue(cd.get("count") instanceof Long);
		AssertJUnit.assertTrue(((Long)cd.get("count")) > 0);
	}

	@Test
	public final void testGetNotificationInfo() {
		AssertJUnit.assertTrue(mb instanceof NotificationEmitter);
		NotificationEmitter emitter = (NotificationEmitter)mb;
		MBeanNotificationInfo[] notifications = emitter.getNotificationInfo();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertTrue(notifications.length > 0);
		for (int i = 0; i < notifications.length; i++) {
			MBeanNotificationInfo info = notifications[i];
			AssertJUnit.assertEquals(Notification.class.getName(), info.getName());
			AssertJUnit.assertEquals("Memory Notification", info.getDescription());
			String[] types = info.getNotifTypes();
			for (int j = 0; j < types.length; j++) {
				String type = types[j];
				AssertJUnit.assertTrue(type.equals(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED)
						|| type.equals(MemoryNotificationInfo.MEMORY_COLLECTION_THRESHOLD_EXCEEDED));

			} // end for
		} // end for
	}
}

/**
 * Helper class
 */
class MyTestListener implements NotificationListener {
	private int count = 0;

	private Object handback = null;

	private Notification notification = null;

	public Object getHandback() {
		return this.handback;
	}

	public int getNotificationsReceivedCount() {
		return count;
	}

	public Notification getNotification() {
		return this.notification;
	}

	public void resetNotificationsReceivedCount() {
		count = 0;
	}

	/*
	 * (non-Javadoc)
	 *
	 * @see javax.management.NotificationListener#handleNotification(javax.management.Notification,
	 *      java.lang.Object)
	 */
	public void handleNotification(Notification notification, Object handback) {
		// Could also verify the contents of the notification here.
		this.count++;
		this.handback = handback;
		this.notification = notification;
	}
}
