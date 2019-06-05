/*[INCLUDE-IF Sidecar17 & !Sidecar19-SE]*/
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
package java.lang.management;

import java.io.IOException;
import java.lang.reflect.Proxy;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.MBeanServerConnection;
import javax.management.MBeanServerFactory;
import javax.management.MBeanServerPermission;
import javax.management.NotCompliantMBeanException;
import javax.management.NotificationEmitter;
import javax.management.ObjectName;

import com.ibm.java.lang.management.internal.ClassLoadingMXBeanImpl;
import com.ibm.java.lang.management.internal.CompilationMXBeanImpl;
import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.lang.management.internal.ExtendedMemoryMXBeanImpl;
import com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl;
import com.ibm.lang.management.internal.ExtendedRuntimeMXBeanImpl;
import com.ibm.lang.management.internal.ExtendedThreadMXBeanImpl;
import com.ibm.lang.management.internal.OpenTypeMappingIHandler;

/**
 * This class provides access to platform management interfaces.
 */
public class ManagementFactory {

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link ClassLoadingMXBean}.
	 */
	public static final String CLASS_LOADING_MXBEAN_NAME = "java.lang:type=ClassLoading"; //$NON-NLS-1$

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link CompilationMXBean}.
	 */
	public static final String COMPILATION_MXBEAN_NAME = "java.lang:type=Compilation"; //$NON-NLS-1$

	/**
	 * The prefix for all <code>ObjectName</code> strings which represent a
	 * {@link GarbageCollectorMXBean}. The unique <code>ObjectName</code> for
	 * a <code>GarbageCollectorMXBean</code> can be formed by adding
	 * &quot;,name= <i>collector name </i>&quot; to this constant.
	 */
	public static final String GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE = "java.lang:type=GarbageCollector"; //$NON-NLS-1$

	/**
	 * The prefix for all <code>ObjectName</code> strings which represent a
	 * {@link MemoryManagerMXBean}. The unique <code>ObjectName</code> for a
	 * <code>MemoryManagerMXBean</code> can be formed by adding
	 * &quot;,name=<i>manager name</i>&quot; to this constant.
	 */
	public static final String MEMORY_MANAGER_MXBEAN_DOMAIN_TYPE = "java.lang:type=MemoryManager"; //$NON-NLS-1$

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link MemoryMXBean}.
	 */
	public static final String MEMORY_MXBEAN_NAME = "java.lang:type=Memory"; //$NON-NLS-1$

	/**
	 * The prefix for all <code>ObjectName</code> strings which represent a
	 * {@link MemoryPoolMXBean}. The unique <code>ObjectName</code> for a
	 * <code>MemoryPoolMXBean</code> can be formed by adding &quot;,name=
	 * <i>memory pool name </i>&quot; to this constant.
	 */
	public static final String MEMORY_POOL_MXBEAN_DOMAIN_TYPE = "java.lang:type=MemoryPool"; //$NON-NLS-1$

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link OperatingSystemMXBean}.
	 */
	public static final String OPERATING_SYSTEM_MXBEAN_NAME = "java.lang:type=OperatingSystem"; //$NON-NLS-1$

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link RuntimeMXBean}.
	 */
	public static final String RUNTIME_MXBEAN_NAME = "java.lang:type=Runtime"; //$NON-NLS-1$

	/**
	 * The unique <code>ObjectName</code> string identifier for the virtual
	 * machine's singleton {@link ThreadMXBean}.
	 */
	public static final String THREAD_MXBEAN_NAME = "java.lang:type=Threading"; //$NON-NLS-1$

	/**
	 * Private constructor ensures that this class cannot be instantiated by users.
	 */
	private ManagementFactory() {
		super();
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the virtual machine's
	 * class loading system.
	 *
	 * @return the virtual machine's {@link ClassLoadingMXBean}
	 */
	public static ClassLoadingMXBean getClassLoadingMXBean() {
		return ClassLoadingMXBeanImpl.getInstance();
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the virtual machine's
	 * compilation system <i>if and only if the virtual machine has a
	 * compilation system enabled</i>. If no compilation exists for this
	 * virtual machine, a <code>null</code> is returned.
	 *
	 * @return the virtual machine's {@link CompilationMXBean} or
	 *         <code>null</code> if there is no compilation system for this
	 *         virtual machine.
	 */
	public static CompilationMXBean getCompilationMXBean() {
		return CompilationMXBeanImpl.getInstance();
	}

	/**
	 * Returns a list of all of the instances of {@link GarbageCollectorMXBean}
	 * in this virtual machine. Owing to the dynamic nature of this kind of
	 * <code>MXBean</code>, it is possible that instances may be created or
	 * destroyed between the invocation and return of this method.
	 *
	 * @return a list of all known <code>GarbageCollectorMXBean</code>s in
	 *         this virtual machine.
	 */
	public static List<GarbageCollectorMXBean> getGarbageCollectorMXBeans() {
		return ExtendedMemoryMXBeanImpl.getInstance().getGarbageCollectorMXBeans();
	}

	/**
	 * Returns a list of all of the instances of {@link MemoryManagerMXBean} in
	 * this virtual machine. Owing to the dynamic nature of this kind of
	 * <code>MXBean</code>, it is possible that instances may be created or
	 * destroyed between the invocation and return of this method.
	 * <p>
	 * Note that the list of <code>MemoryManagerMXBean</code> instances will
	 * include instances of <code>MemoryManagerMXBean</code> sub-types such as
	 * <code>GarbageCollectorMXBean</code>.
	 * </p>
	 *
	 * @return a list of all known <code>MemoryManagerMXBean</code>s in this
	 *         virtual machine.
	 */
	public static List<MemoryManagerMXBean> getMemoryManagerMXBeans() {
		return ExtendedMemoryMXBeanImpl.getInstance().getMemoryManagerMXBeans(true);
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the virtual machine's
	 * memory system.
	 *
	 * @return the virtual machine's {@link MemoryMXBean}
	 */
	public static MemoryMXBean getMemoryMXBean() {
		return ExtendedMemoryMXBeanImpl.getInstance();
	}

	/**
	 * Returns a list of all of the instances of {@link MemoryPoolMXBean} in
	 * this virtual machine. Owing to the dynamic nature of this kind of
	 * <code>MXBean</code>, it is possible that instances may be created or
	 * destroyed between the invocation and return of this method.
	 *
	 * @return a list of all known <code>MemoryPoolMXBean</code>s in this
	 *         virtual machine.
	 */
	public static List<MemoryPoolMXBean> getMemoryPoolMXBeans() {
		return ExtendedMemoryMXBeanImpl.getInstance().getMemoryPoolMXBeans(true);
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the operating system
	 * which the virtual machine runs on.
	 *
	 * @return the virtual machine's {@link OperatingSystemMXBean}
	 */
	public static OperatingSystemMXBean getOperatingSystemMXBean() {
		return ExtendedOperatingSystemMXBeanImpl.getInstance();
	}

	/**
	 * Returns the virtual machine's platform
	 * <code>MBeanServer</code>. This <code>MBeanServer</code> will have
	 * all of the platform <code>MXBean</code>s registered with it including
	 * any dynamic <code>MXBean</code>s (e.g. instances of
	 * {@link GarbageCollectorMXBean} that may be unregistered and destroyed at
	 * a later time.
	 * <p>
	 * In order to simplify the process of distribution and discovery of managed
	 * beans it is good practice to register all managed beans (in addition to
	 * the platform <code>MXBean</code>s) with this server.
	 * </p>
	 * <p>
	 * A custom <code>MBeanServer</code> can be created by this method if the
	 * System property <code>javax.management.builder.initial</code> has been
	 * set with the fully qualified name of a subclass of
	 * {@link javax.management.MBeanServerBuilder}.
	 * </p>
	 *
	 * @return the platform <code>MBeanServer</code>.
	 * @throws SecurityException
	 *             if there is a Java security manager in operation and the
	 *             caller of this method does not have
	 *             &quot;createMBeanServer&quot;
	 *             <code>MBeanServerPermission</code>.
	 * @see MBeanServer
	 * @see javax.management.MBeanServerPermission
	 */
	public static MBeanServer getPlatformMBeanServer() {
		/*[PR CMVC 93006]*/
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(new MBeanServerPermission("createMBeanServer")); //$NON-NLS-1$
		}

		return ServerHolder.platformServer;
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the virtual machine's
	 * runtime system.
	 *
	 * @return the virtual machine's {@link RuntimeMXBean}
	 */
	public static RuntimeMXBean getRuntimeMXBean() {
		return ExtendedRuntimeMXBeanImpl.getInstance();
	}

	/**
	 * Returns the singleton <code>MXBean</code> for the virtual machine's
	 * threading system.
	 *
	 * @return the virtual machine's {@link ThreadMXBean}
	 */
	public static ThreadMXBean getThreadMXBean() {
		return ExtendedThreadMXBeanImpl.getInstance();
	}

	/**
	 * Return a proxy for the named <code>MXBean</code>.
	 * @param <T> the type of the platform <code>MXBean</code>
	 * @param connection the connection to the server
	 * @param mxbeanName the name of the <code>MXBean</code>
	 * @param mxbeanInterface a management interface for a platform <code>MXBean</code>
	 * @return a new proxy object representing the named <code>MXBean</code>.
	 *         All subsequent method invocations on the proxy will be routed
	 *         through the supplied {@link MBeanServerConnection} object.
	 * @throws IOException if a communication problem
	 * occurs with the {@code MBeanServerConnection}
	 */
	public static <T> T newPlatformMXBeanProxy(MBeanServerConnection connection, String mxbeanName,
			Class<T> mxbeanInterface) throws IOException {
		// check that the named object implements the specified interface
		ObjectName mxbeanObjectName = ManagementUtils.checkNamedMXBean(mxbeanName, mxbeanInterface);
		Class<?>[] interfaces;

		if (ManagementUtils.isANotificationEmitter(connection, mxbeanObjectName)) {
			// Proxies of the MemoryMXBean and OperatingSystemMXBean interfaces
			// must also implement the NotificationEmitter interface.
			interfaces = new Class[] { mxbeanInterface, NotificationEmitter.class };
		} else {
			interfaces = new Class[] { mxbeanInterface };
		}

		OpenTypeMappingIHandler handler = new OpenTypeMappingIHandler(connection, mxbeanInterface, mxbeanObjectName);
		Object proxyInstance = Proxy.newProxyInstance(mxbeanInterface.getClassLoader(), interfaces, handler);

		return mxbeanInterface.cast(proxyInstance);
	}

	/**
	 * Returns the platform MXBean implementing
	 * the given {@code mxbeanInterface} which is specified
	 * to have one single instance in the Java virtual machine.
	 * This method may return {@code null} if the management interface
	 * is not implemented in the Java virtual machine (for example,
	 * a Java virtual machine with no compilation system does not
	 * implement {@link CompilationMXBean});
	 * otherwise, this method is equivalent to calling:
	 * <pre>
	 *    {@link #getPlatformMXBeans(Class)
	 *      getPlatformMXBeans(mxbeanInterface)}.get(0);
	 * </pre>
	 *
	 * @param <T> the type of the platform <code>MXBean</code>
	 *
	 * @param mxbeanInterface a management interface for a platform <code>MXBean</code>
	 *     with one single instance in the Java virtual machine if implemented.
	 *
	 * @return the platform MXBean that implements
	 * {@code mxbeanInterface}, or {@code null} if it does not exist.
	 *
	 * @throws IllegalArgumentException if {@code mxbeanInterface}
	 * is not a platform management interface or
	 * not a singleton platform MXBean.
	 *
	 * @since 1.7
	 */
	public static <T extends PlatformManagedObject> T getPlatformMXBean(Class<T> mxbeanInterface) {
		return ManagementUtils.getPlatformMXBean(mxbeanInterface);
	}

	/**
	 * Returns a list of all of the platform <code>MXBean</code>
	 * objects which implement the specified management interface.
	 *
	 * @param <T> the type of the platform <code>MXBean</code>s
	 * @param mxbeanInterface a management interface for the platform <code>MXBean</code>s
	 * @return list of MXBean objects implementing the <code>mxbeanInterface</code>
	 * @throws IllegalArgumentException if {@code mxbeanInterface}
	 * is not a platform management interface
	 *
	 * @since 1.7
	 */
	public static <T extends PlatformManagedObject> List<T> getPlatformMXBeans(Class<T> mxbeanInterface)
			throws IllegalArgumentException {
		return ManagementUtils.getPlatformMXBeans(mxbeanInterface);
	}

	/**
	 * Returns the platform MXBean proxy for
	 * {@code mxbeanInterface} which is specified to have one single
	 * instance in a Java virtual machine and the proxy will
	 * forward the method calls through the given {@code MBeanServerConnection}.
	 * This method may return {@code null} if the management interface
	 * is not implemented in the Java virtual machine being monitored
	 * (for example, a Java virtual machine with no compilation system
	 * does not implement {@link CompilationMXBean});
	 * otherwise, this method is equivalent to calling:
	 * <pre>
	 *     {@link #getPlatformMXBeans(MBeanServerConnection, Class)
	 *        getPlatformMXBeans(connection, mxbeanInterface)}.get(0);
	 * </pre>
	 *
	 * @param <T> the type of the platform <code>MXBean</code>s
	 * @param connection the {@code MBeanServerConnection} to forward to.
	 * @param mxbeanInterface a management interface for a platform
	 *     MXBean with one single instance in the Java virtual machine
	 *     being monitored, if implemented.
	 *
	 * @return the platform MXBean proxy for
	 * forwarding the method calls of the {@code mxbeanInterface}
	 * through the given {@code MBeanServerConnection},
	 * or {@code null} if not exist.
	 *
	 * @throws IllegalArgumentException if {@code mxbeanInterface}
	 * is not a platform management interface or
	 * not a singleton platform MXBean.
	 * @throws IOException if a communication problem
	 * occurred when accessing the {@code MBeanServerConnection}.
	 *
	 * @see #newPlatformMXBeanProxy
	 * @since 1.7
	 */
	public static <T extends PlatformManagedObject> T getPlatformMXBean(MBeanServerConnection connection,
			Class<T> mxbeanInterface) throws IOException {
		ManagementUtils.checkSupportedSingleton(mxbeanInterface);

		String interfaceName = mxbeanInterface.getName();
		/*[IF]*/
		// FIXME build a more specific query
		/*[ENDIF]*/
		Set<ObjectName> beanNames = connection.queryNames(null, null);

		for (ObjectName objectName : beanNames) {
			try {
				if (!connection.isInstanceOf(objectName, interfaceName)) {
					continue;
				}
			} catch (InstanceNotFoundException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
				continue;
			}

			return newPlatformMXBeanProxy(connection, objectName.toString(), mxbeanInterface);
		}

		return null;
	}

	/**
	 * Return a list of MXBean proxies that can proxy the <code>mxbeanInterface</code>
	 * using the specified <code>MBeanServerConnection</code>.
	 *
	 * @param <T> the type of the platform <code>MXBean</code>s
	 * @param connection the {@code MBeanServerConnection} to forward to
	 * @param mxbeanInterface a management interface for the platform <code>MXBean</code>s
	 * @return list of MXBean proxies that can proxy the <code>mxbeanInterface</code>
	 *         using the specified <code>MBeanServerConnection</code>.
	 * @throws IllegalArgumentException if {@code mxbeanInterface} is not a platform management interface
	 * @throws IOException if a communication problem occurs with the {@code MBeanServerConnection}
	 *
	 * @since 1.7
	 */
	public static <T extends PlatformManagedObject> List<T> getPlatformMXBeans(MBeanServerConnection connection,
			Class<T> mxbeanInterface) throws IllegalArgumentException, IOException {
		ManagementUtils.checkSupported(mxbeanInterface);

		List<T> matchedBeans = new LinkedList<>();
		/*[IF]*/
		// FIXME build a more specific query
		/*[ENDIF]*/
		Set<ObjectName> beanNames = connection.queryNames(null, null);

		for (ObjectName objectName : beanNames) {
			try {
				if (!connection.isInstanceOf(objectName, mxbeanInterface.getName())) {
					continue;
				}
			} catch (InstanceNotFoundException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
				continue;
			}

			matchedBeans.add(newPlatformMXBeanProxy(connection, objectName.toString(), mxbeanInterface));
		}

		return matchedBeans;
	}

	/**
	 * Return the set of all platform <code>MXBean</code> interface classes.
	 *
	 * @return Set of all platform <code>MXBean</code> interface classes.
	 *
	 * @since 1.7
	 */
	public static Set<Class<? extends PlatformManagedObject>> getPlatformManagementInterfaces() {
		return ManagementUtils.getPlatformManagementInterfaces();
	}

	private static final class ServerHolder {

		static final MBeanServer platformServer;

		static {
			platformServer = MBeanServerFactory.createMBeanServer();

			final class Registration implements PrivilegedAction<Void> {

				private final MBeanServer server;

				Registration(MBeanServer server) {
					super();
					this.server = server;
				}

				@Override
				public Void run() {
					for (PlatformManagedObject bean : ManagementUtils.getAllAvailableMXBeans()) {
						ObjectName objectName = bean.getObjectName();

						if (server.isRegistered(objectName)) {
							continue;
						}

						try {
							server.registerMBean(bean, objectName);
						} catch (InstanceAlreadyExistsException | MBeanRegistrationException | NullPointerException e) {
							if (ManagementUtils.VERBOSE_MODE) {
								e.printStackTrace(System.err);
							}
						} catch (NotCompliantMBeanException e) {
							e.printStackTrace();
							if (ManagementUtils.VERBOSE_MODE) {
								e.printStackTrace(System.err);
							}
						}
					}

					return null;
				}

			}

			AccessController.doPrivileged(new Registration(platformServer));
		}

	}

}
