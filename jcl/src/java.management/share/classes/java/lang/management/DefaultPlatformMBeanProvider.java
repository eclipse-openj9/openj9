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
package java.lang.management;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.ibm.java.lang.management.internal.BufferPoolMXBeanImpl;
import com.ibm.java.lang.management.internal.ClassLoadingMXBeanImpl;
import com.ibm.java.lang.management.internal.CompilationMXBeanImpl;
import com.ibm.java.lang.management.internal.ComponentBuilder;
import com.ibm.java.lang.management.internal.LoggingMXBeanImpl;
import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.java.lang.management.internal.MemoryMXBeanImpl;
import com.ibm.java.lang.management.internal.OperatingSystemMXBeanImpl;
import com.ibm.java.lang.management.internal.RuntimeMXBeanImpl;
import com.ibm.java.lang.management.internal.ThreadMXBeanImpl;

/**
 * A replacement for the default provider used by ManagementFactory
 * to substitute J9 implementations in place of those from RI.
 */
final class DefaultPlatformMBeanProvider extends sun.management.spi.PlatformMBeanProvider {

	private static final List<PlatformComponent<?>> components;

	static {
		List<PlatformComponent<Object>> allComponents = new ArrayList<>();

		// register standard singleton beans
		ComponentBuilder.create(ManagementFactory.CLASS_LOADING_MXBEAN_NAME, ClassLoadingMXBeanImpl.getInstance())
			.addInterface(java.lang.management.ClassLoadingMXBean.class)
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.MEMORY_MXBEAN_NAME, MemoryMXBeanImpl.getInstance())
			.addInterface(java.lang.management.MemoryMXBean.class)
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME, OperatingSystemMXBeanImpl.getInstance())
			.addInterface(java.lang.management.OperatingSystemMXBean.class)
			.register(allComponents);

		LoggingMXBeanImpl logging = LoggingMXBeanImpl.getInstance();

		/*
		 * The logging bean will be included only if the java.logging module is present.
		 */
		if (logging != null) {
			/*
			 * This is the publicly documented name of the logging bean.
			 * We can't refer to LogManager.LOGGING_MXBEAN_NAME
			 * because the java.logging module is not accessible here.
			 */
			String LOGGING_MXBEAN_NAME = "java.util.logging:type=Logging"; //$NON-NLS-1$
			ComponentBuilder.create(LOGGING_MXBEAN_NAME, logging)
				.addInterface(java.lang.management.PlatformLoggingMXBean.class)
				.register(allComponents);
		}

		ComponentBuilder.create(ManagementFactory.RUNTIME_MXBEAN_NAME, RuntimeMXBeanImpl.getInstance())
			.addInterface(java.lang.management.RuntimeMXBean.class)
			.register(allComponents);

		ComponentBuilder.create(ManagementFactory.THREAD_MXBEAN_NAME, ThreadMXBeanImpl.getInstance())
			.addInterface(java.lang.management.ThreadMXBean.class)
			.register(allComponents);

		// register standard optional beans
		ComponentBuilder.create(ManagementFactory.COMPILATION_MXBEAN_NAME, CompilationMXBeanImpl.getInstance())
			.addInterface(java.lang.management.CompilationMXBean.class)
			.register(allComponents);

		// register beans with zero or more instances

		{
			List<BufferPoolMXBean> beans = BufferPoolMXBeanImpl.getBufferPoolMXBeans();
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (BufferPoolMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementUtils.BUFFERPOOL_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterface(java.lang.management.BufferPoolMXBean.class)
				.register(allComponents);
		}

		{
			List<GarbageCollectorMXBean> beans = MemoryMXBeanImpl.getInstance().getGarbageCollectorMXBeans();
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (GarbageCollectorMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementFactory.GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterface(java.lang.management.GarbageCollectorMXBean.class)
				.addInterface(java.lang.management.MemoryManagerMXBean.class)
				.register(allComponents);
		}

		{
			// exclude garbage collector beans handled above
			List<MemoryManagerMXBean> beans = excluding(MemoryMXBeanImpl.getInstance().getMemoryManagerMXBeans(false), java.lang.management.GarbageCollectorMXBean.class);
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (MemoryManagerMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementFactory.MEMORY_MANAGER_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterface(java.lang.management.MemoryManagerMXBean.class)
				.register(allComponents);
		}

		{
			List<MemoryPoolMXBean> beans = MemoryMXBeanImpl.getInstance().getMemoryPoolMXBeans(false);
			int beanCount = beans.size();
			List<String> names = new ArrayList<>(beanCount);

			for (MemoryPoolMXBean bean : beans) {
				names.add(bean.getName());
			}

			ComponentBuilder.create(ManagementFactory.MEMORY_POOL_MXBEAN_DOMAIN_TYPE, names, beans)
				.addInterface(java.lang.management.MemoryPoolMXBean.class)
				.register(allComponents);
		}

		components = Collections.unmodifiableList(allComponents);
	}

	private static <T> List<T> excluding(List<T> objects, Class<?> excludedType) {
		List<T> filtered = new ArrayList<>();

		for (T object : objects) {
			if (!excludedType.isInstance(object)) {
				filtered.add(object);
			}
		}

		return filtered;
	}

	@Override
	public List<PlatformComponent<?>> getPlatformComponentList() {
		return components;
	}

}
