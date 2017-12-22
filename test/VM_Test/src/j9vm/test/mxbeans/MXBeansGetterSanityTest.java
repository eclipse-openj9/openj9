package j9vm.test.mxbeans;

/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
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

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Tests all MXBean getter methods to make sure calling them
 * does not crash the VM.
 *
 * @author Alexei Svitkine
 *
 */
public class MXBeansGetterSanityTest {
	private static void testMXBeanGetters(Object mxBean) {
		if (mxBean != null) {
			Class<?> mxBeanClass = mxBean.getClass();
			for (Method method : mxBeanClass.getMethods()) {
				String methodName = method.getName();
				if (methodName.startsWith("get") && method.getParameterTypes().length == 0) {
					try {
						System.out.println("Calling " + methodName + " on " + mxBeanClass.getName());
						method.invoke(mxBean);
					} catch (IllegalArgumentException e) {
						e.printStackTrace();
					} catch (IllegalAccessException e) {
						e.printStackTrace();
					} catch (InvocationTargetException e) {
						if (mxBeanClass.getName().equals("com.ibm.lang.management.internal.MemoryPoolMXBeanImpl")
								&& (methodName.equals("getCollectionUsageThreshold")
										|| methodName.equals("getCollectionUsageThresholdCount")
										|| methodName.equals("getUsageThreshold")
										|| methodName.equals("getUsageThresholdCount"))) {
							/* above the methods of MemoryPoolMXBeanImpl could be unsupported depends on the individual MemoryPoolMXBean */
						} else if (mxBeanClass.getName().equals("com.ibm.lang.management.internal.UnixExtendedOperatingSystem")
								&& methodName.equals("getHardwareModel")) {
							/* above the methods of UnixExtendedOperatingSystem could be unsupported depends on the individual OS */
						} else if (mxBeanClass.getName().equals("com.ibm.lang.management.internal.ExtendedRuntimeMXBeanImpl")
								&& methodName.equals("getBootClassPath")) {
							/* above the methods of ExtendedRuntimeMXBeanImpl could be unsupported depends on the individual JVM version */
						} else {
							e.printStackTrace();
						}
					}
				}
			}
		}
	}

	public static void main(String[] args) {
		testMXBeanGetters(ManagementFactory.getClassLoadingMXBean());
		testMXBeanGetters(ManagementFactory.getCompilationMXBean());
		for (GarbageCollectorMXBean mxBean : ManagementFactory.getGarbageCollectorMXBeans()) {
			testMXBeanGetters(mxBean);
		}
		for (MemoryManagerMXBean mxBean : ManagementFactory.getMemoryManagerMXBeans()) {
			testMXBeanGetters(mxBean);
		}
		testMXBeanGetters(ManagementFactory.getMemoryMXBean());
		for (MemoryPoolMXBean mxBean : ManagementFactory.getMemoryPoolMXBeans()) {
			testMXBeanGetters(mxBean);
		}
		testMXBeanGetters(ManagementFactory.getOperatingSystemMXBean());
		testMXBeanGetters(ManagementFactory.getRuntimeMXBean());
		testMXBeanGetters(ManagementFactory.getThreadMXBean());
	}
}
