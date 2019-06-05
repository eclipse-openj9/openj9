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
import java.io.IOException;
import java.lang.management.ClassLoadingMXBean;
import java.lang.management.CompilationMXBean;
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryUsage;
import java.lang.management.OperatingSystemMXBean;
import java.lang.management.PlatformManagedObject;
import java.lang.management.RuntimeMXBean;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.Notification;
import javax.management.NotificationEmitter;
import javax.management.ObjectName;
import javax.management.openmbean.CompositeData;

import com.ibm.lang.management.ProcessingCapacityNotificationInfo;

// These classes are not public API.
import com.ibm.java.lang.management.internal.MemoryMXBeanImpl;
import com.ibm.java.lang.management.internal.MemoryNotificationInfoUtil;
import com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl;
import com.ibm.lang.management.internal.ProcessingCapacityNotificationInfoUtil;

@SuppressWarnings({ "nls", "static-method", "unused" })
@Test(groups = { "level.sanity" })
public class TestManagementFactory {

	private static Logger logger = Logger.getLogger(TestManagementFactory.class);

	@BeforeClass
	protected void setUp() throws Exception {
		logger.info("Starting TestManagementFactory tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
		// do nothing
	}

	@Test
	public final void testGetClassLoadingMXBean() {
		ClassLoadingMXBean clb = ManagementFactory.getClassLoadingMXBean();
		AssertJUnit.assertNotNull(clb);

		// Verify that there is only instance of the class loading bean
		ClassLoadingMXBean clb2 = ManagementFactory.getClassLoadingMXBean();
		AssertJUnit.assertNotNull(clb2);
		AssertJUnit.assertSame(clb, clb2);
	}

	// Need to be able to determine if there is a JIT compiler running in
	// this VM. That way I can write a sensible test case for this kind
	// of bean that may or may not exist at runtime.
	@Test
	public final void testGetCompilationMXBean() {
		CompilationMXBean cb = ManagementFactory.getCompilationMXBean();
		if (cb != null) {
			String name = cb.getName();
			AssertJUnit.assertNotNull(name);
			AssertJUnit.assertTrue(name.length() > 0);

			if (cb.isCompilationTimeMonitoringSupported()) {
				AssertJUnit.assertTrue(cb.getTotalCompilationTime() > -1);
			} else {
				try {
					cb.getTotalCompilationTime();
					Assert.fail("Should have thrown exception.");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			} // end else
		} // end if
	}

	@Test
	public final void testGetGarbageCollectorMXBeans() {
		List<GarbageCollectorMXBean> allBeans = ManagementFactory.getGarbageCollectorMXBeans();
		AssertJUnit.assertNotNull(allBeans);
		AssertJUnit.assertTrue(allBeans.size() > 0);
		GarbageCollectorMXBean gcb = allBeans.get(0);
		AssertJUnit.assertNotNull(gcb);
	}

	@Test
	public final void testGetMemoryManagerMXBeans() {
		List<MemoryManagerMXBean> allBeans = ManagementFactory.getMemoryManagerMXBeans();
		AssertJUnit.assertNotNull(allBeans);
		AssertJUnit.assertTrue(allBeans.size() > 0);
		MemoryManagerMXBean mmb = allBeans.get(0);
		AssertJUnit.assertNotNull(mmb);
	}

	@Test
	public final void testGetMemoryMXBean() {
		MemoryMXBean mb = ManagementFactory.getMemoryMXBean();
		AssertJUnit.assertNotNull(mb);

		// Verify that there is only instance of the this bean
		MemoryMXBean mb2 = ManagementFactory.getMemoryMXBean();
		AssertJUnit.assertNotNull(mb2);
		AssertJUnit.assertSame(mb, mb2);
	}

	@Test
	public final void testGetMemoryPoolMXBeans() {
		List<MemoryPoolMXBean> allBeans = ManagementFactory.getMemoryPoolMXBeans();
		AssertJUnit.assertNotNull(allBeans);
		AssertJUnit.assertTrue(allBeans.size() > 0);
		MemoryPoolMXBean mmb = allBeans.get(0);
		AssertJUnit.assertNotNull(mmb);
	}

	@Test
	public final void testGetOperatingSystemMXBean() {
		OperatingSystemMXBean ob1 = ManagementFactory.getOperatingSystemMXBean();
		AssertJUnit.assertNotNull(ob1);

		// Verify that there is only instance of the this bean
		OperatingSystemMXBean ob2 = ManagementFactory.getOperatingSystemMXBean();
		AssertJUnit.assertNotNull(ob2);
		AssertJUnit.assertSame(ob1, ob2);
	}

	@Test
	public final void testGetPlatformMBeanServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);

		// Verify that subsequent calls always return the same server object.
		MBeanServer pServer2 = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer2);
		AssertJUnit.assertSame(pServer, pServer2);

		// Verify the default domain is "DefaultDomain"
		AssertJUnit.assertEquals("DefaultDomain", pServer.getDefaultDomain());
	}

	@Test
	public final void testClassLoadingBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.CLASS_LOADING_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	@Test
	public final void testMemoryBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	@Test
	public final void testThreadBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.THREAD_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	@Test
	public final void testRuntimeBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.RUNTIME_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	@Test
	public final void testOperatingSystemBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit
					.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	// Need to be able to determine if there is a JIT compiler running in
	// this VM. That way I can write a sensible test case for this kind
	// of bean that may or may not exist at runtime.
	@Test
	public final void testCompilationBeanRegisteredWithServer() {
		MBeanServer pServer = ManagementFactory.getPlatformMBeanServer();
		AssertJUnit.assertNotNull(pServer);
		try {
			AssertJUnit.assertTrue(pServer.isRegistered(new ObjectName(ManagementFactory.COMPILATION_MXBEAN_NAME)));
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException !");
			e.printStackTrace();
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException !");
			e.printStackTrace();
		}
	}

	@Test
	public final void testGetRuntimeMXBean() {
		RuntimeMXBean rb = ManagementFactory.getRuntimeMXBean();
		AssertJUnit.assertNotNull(rb);

		// Verify that there is only instance of the this bean
		RuntimeMXBean rb2 = ManagementFactory.getRuntimeMXBean();
		AssertJUnit.assertNotNull(rb2);
		AssertJUnit.assertSame(rb, rb2);
	}

	@Test
	public final void testGetThreadMXBean() {
		ThreadMXBean tb = ManagementFactory.getThreadMXBean();
		AssertJUnit.assertNotNull(tb);

		// Verify that there is only instance of the this bean
		ThreadMXBean tb2 = ManagementFactory.getThreadMXBean();
		AssertJUnit.assertNotNull(tb2);
		AssertJUnit.assertSame(tb, tb2);
	}

	@Test
	public void testClassLoadingMXBeanProxy() {
		try {
			ClassLoadingMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=ClassLoading",
					ClassLoadingMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=ClassLoading", proxy.getObjectName().toString());

			//It isn't reasonable to assert that the same values are returned via different paths.
			//It might be reasonable to assert that each of those metrics are non-decreasing over time.
			//It needs to check proxy <= direct <= proxy (with a call at each stage).

			long totalLoadedClassCount = ManagementFactory.getClassLoadingMXBean().getTotalLoadedClassCount();
			AssertJUnit.assertTrue(totalLoadedClassCount <= proxy.getTotalLoadedClassCount());
			totalLoadedClassCount = proxy.getTotalLoadedClassCount();
			AssertJUnit.assertTrue(totalLoadedClassCount <= ManagementFactory.getClassLoadingMXBean().getTotalLoadedClassCount());
			totalLoadedClassCount = ManagementFactory.getClassLoadingMXBean().getTotalLoadedClassCount();
			AssertJUnit.assertTrue(totalLoadedClassCount <= proxy.getTotalLoadedClassCount());

			long unloadedClassCount = ManagementFactory.getClassLoadingMXBean().getUnloadedClassCount();
			AssertJUnit.assertTrue(unloadedClassCount <= proxy.getUnloadedClassCount());
			unloadedClassCount = proxy.getUnloadedClassCount();
			AssertJUnit.assertTrue(unloadedClassCount <= ManagementFactory.getClassLoadingMXBean().getUnloadedClassCount());
			unloadedClassCount = ManagementFactory.getClassLoadingMXBean().getUnloadedClassCount();
			AssertJUnit.assertTrue(unloadedClassCount <= proxy.getUnloadedClassCount());

			AssertJUnit.assertEquals(proxy.isVerbose(), ManagementFactory.getClassLoadingMXBean().isVerbose());
			boolean initialVal = proxy.isVerbose();
			proxy.setVerbose(!initialVal);
			AssertJUnit.assertTrue(proxy.isVerbose() != initialVal);
			proxy.setVerbose(initialVal);
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testCompilationMXBeanProxy() {
		try {
			CompilationMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=Compilation", CompilationMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=Compilation", proxy.getObjectName().toString());

			AssertJUnit.assertEquals(proxy.getName(), ManagementFactory.getCompilationMXBean().getName());
			AssertJUnit.assertEquals(proxy.isCompilationTimeMonitoringSupported(),
					ManagementFactory.getCompilationMXBean().isCompilationTimeMonitoringSupported());
			if (!proxy.isCompilationTimeMonitoringSupported()) {
				try {
					long tct = proxy.getTotalCompilationTime();
					Assert.fail("Should have thrown unsupported operation exception!");
				} catch (UnsupportedOperationException t) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + t.getMessage());
				}
			}
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testGarbageCollectorMXBeanProxy() {
		List<java.lang.management.GarbageCollectorMXBean> allBeans = ManagementFactory.getGarbageCollectorMXBeans();
		AssertJUnit.assertNotNull(allBeans);

		// Test for every available GC bean
		for (GarbageCollectorMXBean realBean : allBeans) {
			String beanName = realBean.getName();
			GarbageCollectorMXBean proxy = null;

			try {
				proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
						"java.lang:type=GarbageCollector,name=" + beanName, GarbageCollectorMXBean.class);
				AssertJUnit.assertNotNull(proxy);
				AssertJUnit.assertNotNull(proxy.toString());
				AssertJUnit.assertEquals("java.lang:type=GarbageCollector,name=" + beanName,
						proxy.getObjectName().toString());

				AssertJUnit.assertEquals(realBean.getName(), proxy.getName());

				AssertJUnit.assertEquals(realBean.isValid(), proxy.isValid());

				String[] realMemPools = realBean.getMemoryPoolNames();
				String[] proxyMemPools = proxy.getMemoryPoolNames();
				AssertJUnit.assertTrue(Arrays.equals(realMemPools, proxyMemPools));

			} catch (IOException e) {
				e.printStackTrace();
				Assert.fail("Caught unexpected IOException : " + e.getMessage());
			} catch (IllegalArgumentException e) {
				e.printStackTrace();
				Assert.fail("Caught unexpected IllegalArgumentException : " + e.getMessage());
			}
		}
	}

	@Test
	public void testGarbageCollectorMXBeanProxyWithMemoryManagerMXBeanInterface() {
		List<java.lang.management.GarbageCollectorMXBean> allBeans = ManagementFactory.getGarbageCollectorMXBeans();
		AssertJUnit.assertNotNull(allBeans);

		// Test for every available GC bean
		for (GarbageCollectorMXBean realBean : allBeans) {
			String beanName = realBean.getName();
			MemoryManagerMXBean proxy = null;

			try {
				proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
						"java.lang:type=GarbageCollector,name=" + beanName, MemoryManagerMXBean.class);
				AssertJUnit.assertNotNull(proxy);
				AssertJUnit.assertNotNull(proxy.toString());
				AssertJUnit.assertEquals("java.lang:type=GarbageCollector,name=" + beanName,
						proxy.getObjectName().toString());

				AssertJUnit.assertEquals(realBean.getName(), proxy.getName());

				AssertJUnit.assertEquals(realBean.isValid(), proxy.isValid());

				String[] realMemPools = realBean.getMemoryPoolNames();
				String[] proxyMemPools = proxy.getMemoryPoolNames();
				AssertJUnit.assertTrue(Arrays.equals(realMemPools, proxyMemPools));
			} catch (IOException e) {
				e.printStackTrace();
				Assert.fail("Caught unexpected IOException : " + e.getMessage());
			} catch (IllegalArgumentException e) {
				e.printStackTrace();
				Assert.fail("Caught unexpected IllegalArgumentException : " + e.getMessage());
			}
		}
	}

	@Test
	public void testFailingMemoryManagerMXBeanProxy() {
		// Trying to get a proxy to a non-existent bean should throw back
		// an IllegalArgumentException.
		String badName = "java.lang:type=MemoryManager,name=IDontExist";
		MemoryManagerMXBean proxy = null;
		try {
			proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(), badName,
					MemoryManagerMXBean.class);
			Assert.fail("Should have thrown an exception");
		} catch (IllegalArgumentException e) {
			logger.debug("IllegalArgumentException occurred, as expected: " + e.getMessage());
		} catch (IOException e) {
			e.printStackTrace();
			Assert.fail("Caught unexpected IOException : " + e.getMessage());
		}
	}

	@Test
	public void testMemoryManagerMXBeanProxy() {
		List<MemoryManagerMXBean> allBeans = ManagementFactory.getMemoryManagerMXBeans();
		AssertJUnit.assertNotNull(allBeans);

		MemoryManagerMXBean realBean = allBeans.get(0);
		String beanName = realBean.getName();
		String namePrefix = "java.lang:type=MemoryManager,name=";
		if (realBean instanceof GarbageCollectorMXBean) {
			namePrefix = "java.lang:type=GarbageCollector,name=";
		}
		MemoryManagerMXBean proxy = null;

		try {
			proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					namePrefix + beanName, MemoryManagerMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals(namePrefix + beanName, proxy.getObjectName().toString());

			AssertJUnit.assertEquals(realBean.getName(), proxy.getName());

			AssertJUnit.assertEquals(realBean.isValid(), proxy.isValid());

			String[] realMemPools = realBean.getMemoryPoolNames();
			String[] proxyMemPools = proxy.getMemoryPoolNames();
			AssertJUnit.assertTrue(Arrays.equals(realMemPools, proxyMemPools));
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Caught unexpected Exception : " + e.getClass().getName() + " : " + e.getMessage());
		}
	}

	/**
	 * For IBM extensions on the MemoryMXBean
	 */
	@Test
	public void testExtMemoryMXBeanProxyNotificationEmitter() {
		try {
			com.ibm.lang.management.MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=Memory",
					com.ibm.lang.management.MemoryMXBean.class);
			MemoryMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			// Verify that the proxy is acting as a NotificationEmitter
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;
			// Add a listener with a handback object.
			MyTestListener listener = new MyTestListener();
			ArrayList<String> arr = new ArrayList<>();
			arr.add("Save your money for the children.");
			proxyEmitter.addNotificationListener(listener, null, arr);
			// Fire off a notification and ensure that the listener receives it.
			try {
				MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
				MemoryNotificationInfo info = new MemoryNotificationInfo("Bob", mu, 42);
				CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
				Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
						new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
				notification.setUserData(cd);
				((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
				AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

				// Verify that the handback is as expected.
				AssertJUnit.assertNotNull(listener.getHandback());
				AssertJUnit.assertSame(arr, listener.getHandback());
				ArrayList<?> arr2 = (ArrayList<?>)listener.getHandback();
				AssertJUnit.assertEquals(1, arr2.size());
				AssertJUnit.assertEquals("Save your money for the children.", arr2.get(0));
			} catch (MalformedObjectNameException e) {
				Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
				e.printStackTrace();
			}
		} catch (IllegalArgumentException e) {
			Assert.fail("Unexpected IllegalArgumentException : " + e.getMessage());
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException : " + e.getMessage());
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
		}
	}

	@Test
	public void testMemoryMXBeanProxyNotificationEmitter() {
		try {
			MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Memory", MemoryMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			// Verify that the proxy is acting as a NotificationEmitter
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;
			// Add a listener with a handback object.
			MyTestListener listener = new MyTestListener();
			ArrayList<String> arr = new ArrayList<String>();
			arr.add("Rock on Tommy !!!");
			proxyEmitter.addNotificationListener(listener, null, arr);
			// Fire off a notification and ensure that the listener receives it.
			try {
				MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
				MemoryNotificationInfo info = new MemoryNotificationInfo("Tim", mu, 42);
				CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
				Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
						new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
				notification.setUserData(cd);
				((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
				AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

				// Verify that the handback is as expected.
				AssertJUnit.assertNotNull(listener.getHandback());
				AssertJUnit.assertSame(arr, listener.getHandback());
				ArrayList<?> arr2 = (ArrayList<?>)listener.getHandback();
				AssertJUnit.assertEquals(1, arr2.size());
				AssertJUnit.assertEquals("Rock on Tommy !!!", arr2.get(0));
			} catch (MalformedObjectNameException e) {
				Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
				e.printStackTrace();
			}
		} catch (IllegalArgumentException e) {
			Assert.fail("Unexpected IllegalArgumentException : " + e.getMessage());
		} catch (NullPointerException e) {
			Assert.fail("Unexpected NullPointerException : " + e.getMessage());
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
		}
	}

	/**
	 * For IBM extensions on the MemoryMXBean
	 */
	@Test
	public void testExtMemoryMXBeanProxyNotificationEmitterRemoveListeners() {
		try {
			com.ibm.lang.management.MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=Memory",
					com.ibm.lang.management.MemoryMXBean.class);
			MemoryMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;

			MyTestListener listener = new MyTestListener();
			proxyEmitter.addNotificationListener(listener, null, null);

			// Fire off a notification and ensure that the listener receives it.
			MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
			MemoryNotificationInfo info = new MemoryNotificationInfo("Marcel", mu, 42);
			CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNull(listener.getHandback());

			// Verify the user data of the notification.
			Notification n = listener.getNotification();
			AssertJUnit.assertNotNull(n);
			verifyMemoryNotificationUserData(n.getUserData());

			// Remove the listener
			proxyEmitter.removeNotificationListener(listener);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				proxyEmitter.removeNotificationListener(listener);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (ListenerNotFoundException e) {
				logger.debug("ListenerNotFoundException occurred, as expected: " + e.getMessage());
			}
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		} catch (ListenerNotFoundException e) {
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
			e.printStackTrace();
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testMemoryMXBeanProxyNotificationEmitterRemoveListeners() {
		try {
			MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Memory", MemoryMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;

			MyTestListener listener = new MyTestListener();
			proxyEmitter.addNotificationListener(listener, null, null);

			// Fire off a notification and ensure that the listener receives it.
			MemoryUsage mu = new MemoryUsage(1, 2, 3, 4);
			MemoryNotificationInfo info = new MemoryNotificationInfo("Marcel", mu, 42);
			CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(info);
			Notification notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 42);
			notification.setUserData(cd);
			((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
			AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

			// Verify that the handback is as expected.
			AssertJUnit.assertNull(listener.getHandback());

			// Verify the user data of the notification.
			Notification n = listener.getNotification();
			AssertJUnit.assertNotNull(n);
			verifyMemoryNotificationUserData(n.getUserData());

			// Remove the listener
			proxyEmitter.removeNotificationListener(listener);

			// Fire off a notification and ensure that the listener does
			// *not* receive it.
			listener.resetNotificationsReceivedCount();
			notification = new Notification(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
					new ObjectName(ManagementFactory.MEMORY_MXBEAN_NAME), 43);
			notification.setUserData(cd);
			((MemoryMXBeanImpl)ManagementFactory.getMemoryMXBean()).sendNotification(notification);
			AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

			// Try and remove the listener one more time. Should result in a
			// ListenerNotFoundException being thrown.
			try {
				proxyEmitter.removeNotificationListener(listener);
				Assert.fail("Should have thrown a ListenerNotFoundException!");
			} catch (ListenerNotFoundException e) {
				logger.debug("ListenerNotFoundException occurred, as expected: " + e.getMessage());
			}
		} catch (MalformedObjectNameException e) {
			Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
			e.printStackTrace();
		} catch (ListenerNotFoundException e) {
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
			e.printStackTrace();
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/**
	 * @param userData
	 */
	private void verifyMemoryNotificationUserData(Object userData) {
		// Should be a CompositeData instance
		AssertJUnit.assertTrue(userData instanceof CompositeData);
		CompositeData cd = (CompositeData)userData;
		AssertJUnit.assertTrue(cd.containsKey("poolName"));
		AssertJUnit.assertTrue(cd.containsKey("usage"));
		AssertJUnit.assertTrue(cd.containsKey("count"));
		AssertJUnit.assertEquals("Marcel", cd.get("poolName"));
		AssertJUnit.assertEquals(Long.valueOf(42), cd.get("count"));
	}

	/**
	 * @param userData
	 */
	private void verifyProcessingCapacityNotificationUserData(Object userData) {
		// Should be a CompositeData instance
		AssertJUnit.assertTrue(userData instanceof CompositeData);
		CompositeData cd = (CompositeData)userData;
		AssertJUnit.assertEquals(1, cd.values().size());
		AssertJUnit.assertTrue(cd.containsKey("newProcessingCapacity"));
		AssertJUnit.assertEquals(Integer.valueOf(240), cd.get("newProcessingCapacity"));
	}

	@Test
	public void testMemoryMXBeanProxyNotificationEmitterGetInfo() {
		try {
			MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Memory", MemoryMXBean.class);
			MemoryMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;
			// Verify that the notification info can be retrieved OK
			MBeanNotificationInfo[] notifications = proxyEmitter.getNotificationInfo();
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
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
		}
	}

	@Test
	public void testMemoryMXBeanProxy() {
		try {
			MemoryMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Memory", MemoryMXBean.class);
			MemoryMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=Memory", proxy.getObjectName().toString());

			MemoryUsage proxiedUsage = proxy.getHeapMemoryUsage();
			AssertJUnit.assertNotNull(proxiedUsage);
			AssertJUnit.assertTrue(proxiedUsage.getCommitted() > -1);
			AssertJUnit.assertTrue(proxiedUsage.getUsed() > -1);

			int proxiedCount = proxy.getObjectPendingFinalizationCount();
			AssertJUnit.assertTrue(proxiedCount > -1);

			AssertJUnit.assertEquals(proxy.isVerbose(), ManagementFactory.getMemoryMXBean().isVerbose());

			// TODO : Test out setVerbose when the VM API is implemented.
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	private static void validateMemoryUsage(MemoryUsage usage) {
		if (usage != null) {
			// The constructor for MemoryUsage validates its arguments,
			// but it won't hurt to verify them here.
			// We expect:
			//   -1 <= init
			//   0 <= used <= committed
			//   max >= -1
			//   committed <= max if max != -1
			long init = usage.getInit();
			long used = usage.getUsed();
			long committed = usage.getCommitted();
			long max = usage.getMax();

			AssertJUnit.assertTrue("init cannot be less than -1", init >= -1);
			AssertJUnit.assertTrue("used cannot be less than 0", used >= 0);
			AssertJUnit.assertTrue("used cannot be larger than committed", used <= committed);
			AssertJUnit.assertTrue("max cannot be less than -1", max >= -1);
			AssertJUnit.assertTrue("committed cannot be larger than max", committed <= max || max == -1);
		}
	}

	@Test
	public void testMemoryPoolMXBeanProxy() {
		List<MemoryPoolMXBean> allBeans = ManagementFactory.getMemoryPoolMXBeans();
		AssertJUnit.assertNotNull(allBeans);
		MemoryPoolMXBean realBean = allBeans.get(0);
		String beanName = realBean.getName();
		MemoryPoolMXBean proxy = null;
		try {
			proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=MemoryPool,name=" + beanName, MemoryPoolMXBean.class);
			MemoryPoolMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=MemoryPool,name=" + beanName, proxy.getObjectName().toString());

			AssertJUnit.assertEquals(realBean.getName(), proxy.getName());

			// query via the proxy first because it takes a significantly
			// longer path and is likely to affect usage
			MemoryUsage proxyUsage = proxy.getCollectionUsage();
			MemoryUsage directUsage = realBean.getCollectionUsage();
			validateMemoryUsage(proxyUsage);
			validateMemoryUsage(directUsage);
			/* directUsage and proxyUsage could be different, if gc happened between getCollectionUsage() calls*/

			if (realBean.isCollectionUsageThresholdSupported()) {
				AssertJUnit.assertEquals(realBean.getCollectionUsageThreshold(), proxy.getCollectionUsageThreshold());
				AssertJUnit.assertEquals(realBean.getCollectionUsageThresholdCount(),
						proxy.getCollectionUsageThresholdCount());
				AssertJUnit.assertEquals(realBean.isCollectionUsageThresholdExceeded(),
						proxy.isCollectionUsageThresholdExceeded());

				realBean.setCollectionUsageThreshold(200 * 1024);
				AssertJUnit.assertEquals(200 * 1024, proxy.getCollectionUsageThreshold());
			} else {
				try {
					realBean.getCollectionUsageThreshold();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.getCollectionUsageThreshold();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.getCollectionUsageThresholdCount();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.getCollectionUsageThresholdCount();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.isCollectionUsageThresholdExceeded();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.isCollectionUsageThresholdExceeded();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.setCollectionUsageThreshold(300 * 1024);
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.setCollectionUsageThreshold(300 * 1024);
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			} // end else

			String[] realMgrs = realBean.getMemoryManagerNames();
			String[] proxyMgrs = proxy.getMemoryManagerNames();
			AssertJUnit.assertTrue(Arrays.equals(realMgrs, proxyMgrs));

			// query via the proxy first because it takes a significantly
			// longer path and is likely to affect peak usage
			proxyUsage = proxy.getPeakUsage();
			directUsage = realBean.getPeakUsage();
			validateMemoryUsage(proxyUsage);
			validateMemoryUsage(directUsage);
			// we cannot simply assert the the two peak usage objects are equal
			// the committed size should, however, be non-decreasing
			if (directUsage != null && proxyUsage != null) {
				AssertJUnit.assertEquals("init should be the same", directUsage.getInit(), proxyUsage.getInit());
/* it is possible that the committed size of directUsage is smaller than one from proxyUsage, if GC happens between getPeakUsage() calls and GC decides to contrast the memory pool 
				AssertJUnit.assertTrue("committed should be non-decreasing",
						directUsage.getCommitted() >= proxyUsage.getCommitted());
*/
				AssertJUnit.assertEquals("max should be the same", directUsage.getMax(), proxyUsage.getMax());
			} else {
				// if either is null, the other should be also
				AssertJUnit.assertEquals(directUsage, proxyUsage);
			}

			AssertJUnit.assertEquals(realBean.getType(), proxy.getType());

			/* It is unfair to compare usages obtained by realBean and proxy; the gap
			 * between calls may lead to disparity in the values they report.
			 */
			logger.debug("Memory usage by the current memory pool (bean instance): " + realBean.getUsage());
			logger.debug("Memory usage by the current memory (bean proxy): " + proxy.getUsage());

			if (realBean.isUsageThresholdSupported()) {
				AssertJUnit.assertEquals(realBean.getUsageThreshold(), proxy.getUsageThreshold());
				AssertJUnit.assertEquals(realBean.getUsageThresholdCount(), proxy.getUsageThresholdCount());
				AssertJUnit.assertEquals(realBean.isCollectionUsageThresholdExceeded(),
						proxy.isCollectionUsageThresholdExceeded());

				realBean.setUsageThreshold(200 * 1024);
				AssertJUnit.assertEquals(200 * 1024, proxy.getUsageThreshold());

				try {
					proxy.setUsageThreshold(-23);
					Assert.fail("Should have thrown IllegalArgumentException");
				} catch (IllegalArgumentException e) {
					logger.debug("IllegalArgumentException occurred, as expected: " + e.getMessage());
				}

			} else {
				try {
					realBean.getUsageThreshold();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.getUsageThreshold();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.getUsageThresholdCount();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.getUsageThresholdCount();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.isUsageThresholdExceeded();
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.isUsageThresholdExceeded();
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					realBean.setUsageThreshold(300 * 1024);
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.setUsageThreshold(300 * 1024);
					Assert.fail("Should have thrown an exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			} // end else

			AssertJUnit.assertEquals(realBean.isValid(), proxy.isValid());

			proxy.resetPeakUsage();
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testOperatingSystemMXBeanProxy() {
		try {
			OperatingSystemMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=OperatingSystem",
					OperatingSystemMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=OperatingSystem", proxy.getObjectName().toString());

			AssertJUnit.assertEquals(proxy.getArch(), ManagementFactory.getOperatingSystemMXBean().getArch());
			AssertJUnit.assertEquals(proxy.getName(), ManagementFactory.getOperatingSystemMXBean().getName());
			AssertJUnit.assertEquals(proxy.getVersion(), ManagementFactory.getOperatingSystemMXBean().getVersion());
			AssertJUnit.assertEquals(proxy.getAvailableProcessors(),
					ManagementFactory.getOperatingSystemMXBean().getAvailableProcessors());
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/**
	 * Helper that returns the correct typing information for an Operating System
	 * interface, depending on whether the underlying OS is Unix or not.
	 */
	private Class<? extends com.ibm.lang.management.OperatingSystemMXBean> getOperatingSystemInterface() {
		if (TestUtil.isRunningOnUnix()) {
			return com.ibm.lang.management.UnixOperatingSystemMXBean.class;
		} else {
			return com.ibm.lang.management.OperatingSystemMXBean.class;
		}
	}

	/**
	 * For IBM extension on the OperatingSystemMXBean type
	 */
	@Test
	public void testExtOperatingSystemMXBeanProxyIsNotificationEmitter() {
		try {
			com.ibm.lang.management.OperatingSystemMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=OperatingSystem",
					getOperatingSystemInterface());
			OperatingSystemMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertFalse(proxy instanceof com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/**
	 * For IBM extension on the OperatingSystemMXBean
	 */
	@Test
	public void testExtOperatingSystemMXBeanProxyEmitsNotifications() {
		try {
			com.ibm.lang.management.OperatingSystemMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=OperatingSystem",
					getOperatingSystemInterface());
			OperatingSystemMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertFalse(proxy instanceof com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);

			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;
			// Add a listener with a handback object.
			MyTestListener listener = new MyTestListener();
			ArrayList<String> arr = new ArrayList<>();
			arr.add("Save your money for the children.");
			proxyEmitter.addNotificationListener(listener, null, arr);
			// Fire off a notification and ensure that the listener receives it.
			try {
				ProcessingCapacityNotificationInfo info = new ProcessingCapacityNotificationInfo(24);
				CompositeData cd = ProcessingCapacityNotificationInfoUtil.toCompositeData(info);
				Notification notification = new Notification(
						ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
						new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 12);
				notification.setUserData(cd);

				((ExtendedOperatingSystemMXBeanImpl)(ManagementFactory.getOperatingSystemMXBean()))
						.sendNotification(notification);
				AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

				// Verify that the handback is as expected.
				AssertJUnit.assertNotNull(listener.getHandback());
				AssertJUnit.assertSame(arr, listener.getHandback());
				ArrayList<?> arr2 = (ArrayList<?>)listener.getHandback();
				AssertJUnit.assertEquals(1, arr2.size());
				AssertJUnit.assertEquals("Save your money for the children.", arr2.get(0));
			} catch (MalformedObjectNameException e) {
				Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
				e.printStackTrace();
			}
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	/**
	 * For IBM extensions on the OperatingSystemMXBean
	 */
	@Test
	public void testExtOperatingSystemMXBeanProxyDoesRemoveListeners() {
		try {
			com.ibm.lang.management.OperatingSystemMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(
					ManagementFactory.getPlatformMBeanServer(), "java.lang:type=OperatingSystem",
					getOperatingSystemInterface());
			OperatingSystemMXBean stdProxy = proxy;
			AssertJUnit.assertNotNull(stdProxy);
			AssertJUnit.assertFalse(proxy instanceof com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl);
			AssertJUnit.assertTrue(proxy instanceof NotificationEmitter);

			NotificationEmitter proxyEmitter = (NotificationEmitter)proxy;

			MyTestListener listener = new MyTestListener();
			proxyEmitter.addNotificationListener(listener, null, null);

			// Fire off a notification and ensure that the listener receives it.
			try {
				ProcessingCapacityNotificationInfo info = new ProcessingCapacityNotificationInfo(240);
				CompositeData cd = ProcessingCapacityNotificationInfoUtil.toCompositeData(info);
				Notification notification = new Notification(
						ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
						new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 12);
				notification.setUserData(cd);

				((ExtendedOperatingSystemMXBeanImpl)(ManagementFactory.getOperatingSystemMXBean()))
						.sendNotification(notification);
				AssertJUnit.assertEquals(1, listener.getNotificationsReceivedCount());

				// Verify the user data of the notification.
				Notification n = listener.getNotification();
				AssertJUnit.assertNotNull(n);
				verifyProcessingCapacityNotificationUserData(n.getUserData());

				// Remove the listener
				proxyEmitter.removeNotificationListener(listener);

				// Fire off a notification and ensure that the listener does
				// *not* receive it.
				listener.resetNotificationsReceivedCount();
				notification = new Notification(ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
						new ObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME), 13);
				notification.setUserData(cd);
				((ExtendedOperatingSystemMXBeanImpl)(ManagementFactory.getOperatingSystemMXBean()))
						.sendNotification(notification);
				AssertJUnit.assertEquals(0, listener.getNotificationsReceivedCount());

				// Try and remove the listener one more time. Should result in a
				// ListenerNotFoundException being thrown.
				try {
					proxyEmitter.removeNotificationListener(listener);
					Assert.fail("Should have thrown a ListenerNotFoundException!");
				} catch (ListenerNotFoundException e) {
					logger.debug("ListenerNotFoundException occurred, as expected: " + e.getMessage());
				}
			} catch (MalformedObjectNameException e) {
				Assert.fail("Unexpected MalformedObjectNameException : " + e.getMessage());
				e.printStackTrace();
			}
		} catch (ListenerNotFoundException e) {
			Assert.fail("Unexpected ListenerNotFoundException : " + e.getMessage());
			e.printStackTrace();
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testRuntimeMXBeanProxy() {
		try {
			RuntimeMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Runtime", RuntimeMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertFalse(proxy instanceof NotificationEmitter);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=Runtime", proxy.getObjectName().toString());

			AssertJUnit.assertEquals(proxy.isBootClassPathSupported(),
					ManagementFactory.getRuntimeMXBean().isBootClassPathSupported());
			if (proxy.isBootClassPathSupported()) {
				String bootClassPath = proxy.getBootClassPath();
				AssertJUnit.assertEquals(bootClassPath, ManagementFactory.getRuntimeMXBean().getBootClassPath());
			} else {
				try {
					proxy.getBootClassPath();
					Assert.fail("Should have thrown an unsupported operation exception");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			}

			AssertJUnit.assertEquals(proxy.getClassPath(), ManagementFactory.getRuntimeMXBean().getClassPath());
			AssertJUnit.assertEquals(proxy.getInputArguments(),
					ManagementFactory.getRuntimeMXBean().getInputArguments());
			List<String> args = proxy.getInputArguments();
			AssertJUnit.assertNotNull(args);
			AssertJUnit.assertEquals(proxy.getLibraryPath(), ManagementFactory.getRuntimeMXBean().getLibraryPath());
			AssertJUnit.assertEquals(proxy.getManagementSpecVersion(),
					ManagementFactory.getRuntimeMXBean().getManagementSpecVersion());
			AssertJUnit.assertEquals(proxy.getName(), ManagementFactory.getRuntimeMXBean().getName());
			AssertJUnit.assertEquals(proxy.getSpecName(), ManagementFactory.getRuntimeMXBean().getSpecName());
			AssertJUnit.assertEquals(proxy.getSpecVendor(), ManagementFactory.getRuntimeMXBean().getSpecVendor());
			AssertJUnit.assertEquals(proxy.getSpecVersion(), ManagementFactory.getRuntimeMXBean().getSpecVersion());
			AssertJUnit.assertEquals(proxy.getStartTime(), ManagementFactory.getRuntimeMXBean().getStartTime());

			// System properties is a tricky one. In "local" Java code, returned
			// inside a Map<String, String>. When invoking "remotely", returned
			// as a TabularData where each row is a CompositeData holding one
			// key=value pair. Special care must be taken here to ensure that
			// the proxied call successfully manages the conversion of the
			// TabularData back to a Map<String, String>.
			Map<String, String> props = proxy.getSystemProperties();
			AssertJUnit.assertNotNull(props);
			AssertJUnit.assertEquals(proxy.getSystemProperties(),
					ManagementFactory.getRuntimeMXBean().getSystemProperties());
			AssertJUnit.assertEquals(proxy.getVmName(), ManagementFactory.getRuntimeMXBean().getVmName());
			AssertJUnit.assertEquals(proxy.getVmVendor(), ManagementFactory.getRuntimeMXBean().getVmVendor());
			AssertJUnit.assertEquals(proxy.getVmVersion(), ManagementFactory.getRuntimeMXBean().getVmVersion());
		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testThreadMXBeanProxy() {
		try {
			ThreadMXBean proxy = ManagementFactory.newPlatformMXBeanProxy(ManagementFactory.getPlatformMBeanServer(),
					"java.lang:type=Threading", ThreadMXBean.class);
			AssertJUnit.assertNotNull(proxy);
			AssertJUnit.assertNotNull(proxy.toString());
			AssertJUnit.assertEquals("java.lang:type=Threading", proxy.getObjectName().toString());

			AssertJUnit.assertEquals(proxy.isCurrentThreadCpuTimeSupported(),
					ManagementFactory.getThreadMXBean().isCurrentThreadCpuTimeSupported());
			if (!proxy.isCurrentThreadCpuTimeSupported()) {
				try {
					long tmp = proxy.getCurrentThreadCpuTime();
					Assert.fail("Should have thrown an unsuported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					long tmp = proxy.getCurrentThreadUserTime();
					Assert.fail("Should have thrown an unsuported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			}

			AssertJUnit.assertTrue(Arrays.equals(proxy.findMonitorDeadlockedThreads(),
					ManagementFactory.getThreadMXBean().findMonitorDeadlockedThreads()));
			AssertJUnit.assertEquals(proxy.getDaemonThreadCount(),
					ManagementFactory.getThreadMXBean().getDaemonThreadCount());
			AssertJUnit.assertEquals(proxy.getPeakThreadCount(),
					ManagementFactory.getThreadMXBean().getPeakThreadCount());
			AssertJUnit.assertEquals(proxy.getThreadCount(), ManagementFactory.getThreadMXBean().getThreadCount());

			AssertJUnit.assertEquals(proxy.isThreadCpuTimeSupported(),
					ManagementFactory.getThreadMXBean().isThreadCpuTimeSupported());
			if (!proxy.isThreadCpuTimeSupported()) {
				try {
					long tmp = proxy.getThreadCpuTime(Thread.currentThread().getId());
					Assert.fail("Should have thrown an unsuported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					long tmp = proxy.getThreadUserTime(Thread.currentThread().getId());
					Assert.fail("Should have thrown an unsuported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			}

			ThreadInfo ti1 = proxy.getThreadInfo(Thread.currentThread().getId());
			ThreadInfo ti2 = ManagementFactory.getThreadMXBean().getThreadInfo(Thread.currentThread().getId());
			logger.debug("ti1 : " + ti1.toString());
			logger.debug("ti2 : " + ti2.toString());
			AssertJUnit.assertEquals(ti2, ti1);

			AssertJUnit.assertEquals(proxy.getThreadInfo(Thread.currentThread().getId(), 2),
					ManagementFactory.getThreadMXBean().getThreadInfo(Thread.currentThread().getId(), 2));

			AssertJUnit.assertTrue(Arrays.equals(proxy.getThreadInfo(new long[] { Thread.currentThread().getId() }),
					ManagementFactory.getThreadMXBean().getThreadInfo(new long[] { Thread.currentThread().getId() })));

			AssertJUnit.assertTrue(Arrays.equals(proxy.getThreadInfo(new long[] { Thread.currentThread().getId() }, 2),
					ManagementFactory.getThreadMXBean().getThreadInfo(new long[] { Thread.currentThread().getId() },
							2)));

			AssertJUnit.assertEquals(proxy.getTotalStartedThreadCount(),
					ManagementFactory.getThreadMXBean().getTotalStartedThreadCount());

			AssertJUnit.assertEquals(proxy.isThreadContentionMonitoringSupported(),
					ManagementFactory.getThreadMXBean().isThreadContentionMonitoringSupported());

			if (proxy.isThreadContentionMonitoringSupported()) {
				AssertJUnit.assertEquals(proxy.isThreadContentionMonitoringEnabled(),
						ManagementFactory.getThreadMXBean().isThreadContentionMonitoringEnabled());

				// TODO : test out setThreadContentionMonitoringEnabled when the
				// VM hooks are all there.

			} else {
				try {
					AssertJUnit.assertEquals(proxy.isThreadContentionMonitoringEnabled(),
							ManagementFactory.getThreadMXBean().isThreadContentionMonitoringEnabled());
					Assert.fail("Should have thrown unsupported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}

				try {
					proxy.setThreadContentionMonitoringEnabled(true);
					Assert.fail("Should have thrown unsupported operation exception!");
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException occurred, as expected: " + e.getMessage());
				}
			} // end else

		} catch (IOException e) {
			Assert.fail("Unexpected IOException : " + e.getMessage());
			e.printStackTrace();
		}
	}

	@Test
	public void testGetAllPlatformMXBeanInterfacesStandardInterfaces() {
		// ManagementFactory.getAllPlatformMXBeanInterfaces();
		Set<Class<? extends PlatformManagedObject>> interfaces = ManagementFactory.getPlatformManagementInterfaces();

		logger.debug("ManagementFactory.getPlatformManagementInterfaces: ");
		logger.debug(interfaces);
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.ClassLoadingMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.CompilationMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.GarbageCollectorMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.MemoryMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.MemoryManagerMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.MemoryPoolMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.OperatingSystemMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.RuntimeMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(java.lang.management.ThreadMXBean.class));
		// AssertJUnit.assertTrue(interfaces.contains(java.util.logging.LoggingMXBean.class));
	}

	@Test
	public void testGetAllPlatformMXBeanInterfacesExtensionInterfaces() {
		Set<Class<? extends PlatformManagedObject>> interfaces = ManagementFactory.getPlatformManagementInterfaces();

		AssertJUnit.assertTrue(interfaces.contains(com.ibm.lang.management.GarbageCollectorMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(com.ibm.lang.management.MemoryMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(com.ibm.lang.management.MemoryPoolMXBean.class));
		AssertJUnit.assertTrue(interfaces.contains(com.ibm.lang.management.OperatingSystemMXBean.class));
	}

	@Test
	public void testGetPlatformMXBeans() throws IllegalArgumentException, IOException {
		MBeanServer localServer = ManagementFactory.getPlatformMBeanServer();
		Set<Class<? extends PlatformManagedObject>> interfaces = ManagementFactory.getPlatformManagementInterfaces();

		for (Class<? extends PlatformManagedObject> interfaceClass : interfaces) {
			List<? extends PlatformManagedObject> beanList1 = ManagementFactory.getPlatformMXBeans(interfaceClass);
			AssertJUnit.assertNotNull(beanList1);

			List<? extends PlatformManagedObject> beanList2 = ManagementFactory.getPlatformMXBeans(localServer,
					interfaceClass);
			AssertJUnit.assertNotNull(beanList2);

			AssertJUnit.assertEquals(beanList1.size(), beanList2.size());

			String beanNames1[] = new String[beanList1.size()];
			String beanNames2[] = new String[beanList2.size()];
			for (int i = 0; i < beanList1.size(); i++) {
				beanNames1[i] = beanList1.get(i).getObjectName().toString();
				beanNames2[i] = beanList2.get(i).getObjectName().toString();
			}

			Arrays.sort(beanNames1);
			Arrays.sort(beanNames2);
			for (int i = 0; i < beanList1.size(); i++) {
				AssertJUnit.assertEquals(beanNames1[i], beanNames2[i]);
			}
		}

	}
}
