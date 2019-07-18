/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
package com.ibm.lang.management.internal;

import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.ibm.java.lang.management.internal.ComponentBuilder;
import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.lang.management.JvmCpuMonitorMXBean;
import com.ibm.virtualization.management.internal.GuestOS;
import com.ibm.virtualization.management.internal.HypervisorMXBeanImpl;
import openj9.lang.management.OpenJ9DiagnosticsMXBean;
import openj9.lang.management.internal.OpenJ9DiagnosticsMXBeanImpl;

/**
 * This class implements the service-provider interface to make OpenJ9-specific
 * MXBeans available. These beans are either in addition to the basic set or
 * implement non-standard interfaces.
 */
public final class PlatformMBeanProvider extends sun.management.spi.PlatformMBeanProvider {

	private static final List<PlatformComponent<?>> components;

	static {
		List<PlatformComponent<Object>> allComponents = new ArrayList<>();

		/*
		 * Inherited from DefaultPlatformMBeanProvider:
		 *     BufferPoolMXBean
		 *     ClassLoadingMXBean
		 *     CompilationMXBean
		 *     PlatformLoggingMXBean
		 */

		// register OpenJ9 extensions of standard singleton beans
		ComponentBuilder.create(ManagementFactory.MEMORY_MXBEAN_NAME, ExtendedMemoryMXBeanImpl.getInstance())
			.addInterface(com.ibm.lang.management.MemoryMXBean.class)
			.addInterface(java.lang.management.MemoryMXBean.class)
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME, ExtendedOperatingSystemMXBeanImpl.getInstance())
			.addInterface(com.ibm.lang.management.OperatingSystemMXBean.class)
			.addInterface(com.sun.management.OperatingSystemMXBean.class)
			.addInterface(java.lang.management.OperatingSystemMXBean.class)
			.addInterfaceIf(com.ibm.lang.management.UnixOperatingSystemMXBean.class, ManagementUtils.isRunningOnUnix())
			.addInterfaceIf(com.sun.management.UnixOperatingSystemMXBean.class, ManagementUtils.isRunningOnUnix())
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.RUNTIME_MXBEAN_NAME, ExtendedRuntimeMXBeanImpl.getInstance())
			.addInterface(com.ibm.lang.management.RuntimeMXBean.class)
			.addInterface(java.lang.management.RuntimeMXBean.class)
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.THREAD_MXBEAN_NAME, ExtendedThreadMXBeanImpl.getInstance())
			.addInterface(com.ibm.lang.management.ThreadMXBean.class)
			.addInterface(java.lang.management.ThreadMXBean.class)
			.register(allComponents);

		// register OpenJ9-specific singleton beans
		ComponentBuilder.create("com.ibm.virtualization.management:type=GuestOS", GuestOS.getInstance()) //$NON-NLS-1$
			.addInterface(com.ibm.virtualization.management.GuestOSMXBean.class)
			.register(allComponents);

		ComponentBuilder.create("com.ibm.virtualization.management:type=Hypervisor", HypervisorMXBeanImpl.getInstance()) //$NON-NLS-1$
			.addInterface(com.ibm.virtualization.management.HypervisorMXBean.class)
			.register(allComponents);

		ComponentBuilder.create("com.ibm.lang.management:type=JvmCpuMonitor", JvmCpuMonitor.getInstance()) //$NON-NLS-1$
			.addInterface(JvmCpuMonitorMXBean.class)
			.register(allComponents);

		/* OpenJ9DiagnosticsMXBeanImpl depends on openj9.jvm. If openj9.jvm is not
		 * available exclude this component.
		 */
		if (ModuleLayer.boot().findModule("openj9.jvm").isPresent()) { //$NON-NLS-1$
			ComponentBuilder.create("openj9.lang.management:type=OpenJ9Diagnostics", OpenJ9DiagnosticsMXBeanImpl.getInstance()) //$NON-NLS-1$
				.addInterface(OpenJ9DiagnosticsMXBean.class)
				.register(allComponents);
		}

		// register beans with zero or more instances

		{
			List<java.lang.management.GarbageCollectorMXBean> beans = ExtendedMemoryMXBeanImpl.getInstance().getGarbageCollectorMXBeans();
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (java.lang.management.GarbageCollectorMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementFactory.GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterfaceIf(com.ibm.lang.management.GarbageCollectorMXBean.class, true)
				.addInterfaceIf(com.sun.management.GarbageCollectorMXBean.class, true)
				.addInterface(java.lang.management.GarbageCollectorMXBean.class)
				.addInterface(java.lang.management.MemoryManagerMXBean.class)
				.register(allComponents);
		}

		{
			List<java.lang.management.MemoryPoolMXBean> beans = ExtendedMemoryMXBeanImpl.getInstance().getMemoryPoolMXBeans(false);
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (java.lang.management.MemoryPoolMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementFactory.MEMORY_POOL_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterfaceIf(com.ibm.lang.management.MemoryPoolMXBean.class, true)
				.addInterface(java.lang.management.MemoryPoolMXBean.class)
				.register(allComponents);
		}

		components = Collections.unmodifiableList(allComponents);
	}

	@Override
	public List<PlatformComponent<?>> getPlatformComponentList() {
		return components;
	}

}
