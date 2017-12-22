/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

import com.ibm.lang.management.JvmCpuMonitorInfo;
import com.ibm.lang.management.JvmCpuMonitorMXBean;

/**
 * Runtime type for {@link JvmCpuMonitorMXBean}.
 * <p>
 * Implements getting CPU Usage for various JVM thread categories.
 * </p>
 */
public class JvmCpuMonitor implements JvmCpuMonitorMXBean {

	/**
	 * This is the default thread category of every application thread 
	 */
	private enum Category {
		THREAD_CATEGORY_INVALID(-1, "Invalid"),							//$NON-NLS-1$
		THREAD_CATEGORY_UNKNOWN(0, "UnKnown"),							//$NON-NLS-1$
		THREAD_CATEGORY_SYSTEM_JVM(1, "System-JVM"),					//$NON-NLS-1$
		THREAD_CATEGORY_GC(2, "GC"),									//$NON-NLS-1$
		THREAD_CATEGORY_JIT(3, "JIT"),									//$NON-NLS-1$
		/**
		 * This is a special thread category for a thread(s) that the application
		 * chooses as the "Monitor" using the setThreadCategory API.
		 * Monitor threads are typically used for monitoring resource usage.
		 */
		THREAD_CATEGORY_RESOURCE_MONITOR(10, "Resource-Monitor"),		//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION(100, "Application"),				//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION_USER1(101, "Application-User1"),	//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION_USER2(102, "Application-User2"),	//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION_USER3(103, "Application-User3"),	//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION_USER4(104, "Application-User4"),	//$NON-NLS-1$
		THREAD_CATEGORY_APPLICATION_USER5(105, "Application-User5");	//$NON-NLS-1$

		private final int catValue;
		private final String catName;
		private Category(int value, String name) {
			this.catValue = value;
			this.catName = name;
		}
		public int categoryValue() {
			return this.catValue;
		}
		public String categoryName() {
			return this.catName;
		}
		public static Category fromInt(int catId) {
			switch (catId) {
			case 0:
				return THREAD_CATEGORY_UNKNOWN;
			case 1:
				return THREAD_CATEGORY_SYSTEM_JVM;
			case 2:
				return THREAD_CATEGORY_GC;
			case 3:
				return THREAD_CATEGORY_JIT;
			case 10:
				return THREAD_CATEGORY_RESOURCE_MONITOR;
			case 100:
				return THREAD_CATEGORY_APPLICATION;
			case 101:
				return THREAD_CATEGORY_APPLICATION_USER1;
			case 102:
				return THREAD_CATEGORY_APPLICATION_USER2;
			case 103:
				return THREAD_CATEGORY_APPLICATION_USER3;
			case 104:
				return THREAD_CATEGORY_APPLICATION_USER4;
			case 105:
				return THREAD_CATEGORY_APPLICATION_USER5;
			default:
				return THREAD_CATEGORY_INVALID;
			}
		}
		public static Category fromString(String catName) {
			if (catName != null) {
				for (Category c : Category.values()) {
					if (catName.equalsIgnoreCase(c.catName)) {
						return c;
					}
				}
			}
			return THREAD_CATEGORY_INVALID;
		}
	};

	private static JvmCpuMonitor instance = new JvmCpuMonitor();
	
	/**
	 * Singleton accessor method. Returns an instance of {@link JvmCpuMonitor}
	 * 
	 * @return a static instance of {@link JvmCpuMonitor}
	 */
	public static JvmCpuMonitor getInstance() {
		return instance;
	}

	/**
	 * Returns the object name of the MXBean
	 * 
	 * @return objectName representing the MXBean
	 */
	public ObjectName getObjectName() {
		try {
			ObjectName name = new ObjectName("com.ibm.lang.management:type=JvmCpuMonitor"); //$NON-NLS-1$
			return name;
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}
	
	/**
	 * {@inheritDoc}
	 */
	public JvmCpuMonitorInfo getThreadsCpuUsage(JvmCpuMonitorInfo jcmInfo) {
		if (null == jcmInfo) {
			throw new NullPointerException();
		}
		return getThreadsCpuUsageImpl(jcmInfo);
	}

	/**
	 * {@inheritDoc}
	 */
	public JvmCpuMonitorInfo getThreadsCpuUsage() {
		JvmCpuMonitorInfo jcmInfo = new JvmCpuMonitorInfo();

		return getThreadsCpuUsageImpl(jcmInfo);
	}

	/**
	 * {@inheritDoc}
	 */
	public int setThreadCategory(long id, String category) {
		if (id <= 0) {
			throw new IllegalArgumentException();
		}
		Category catId = Category.fromString(category);
		switch (catId) {
		case THREAD_CATEGORY_RESOURCE_MONITOR:
		case THREAD_CATEGORY_APPLICATION:
		case THREAD_CATEGORY_APPLICATION_USER1:
		case THREAD_CATEGORY_APPLICATION_USER2:
		case THREAD_CATEGORY_APPLICATION_USER3:
		case THREAD_CATEGORY_APPLICATION_USER4:
		case THREAD_CATEGORY_APPLICATION_USER5:
			break;
		default:
			throw new IllegalArgumentException();
		}
		
		return setThreadCategoryImpl(id, catId.categoryValue());
	}

	/**
	 * {@inheritDoc}
	 */
	public String getThreadCategory(long id) {
		if (id <= 0) {
			throw new IllegalArgumentException();
		}
		int catId = getThreadCategoryImpl(id);
		Category threadCat = Category.fromInt(catId);
		return threadCat.categoryName();
	}

	/* Native implementation that returns the CPU usage statistics filled in */
	private native JvmCpuMonitorInfo getThreadsCpuUsageImpl(JvmCpuMonitorInfo jcmInfo);
	private native int setThreadCategoryImpl(long id, int category);
	private native int getThreadCategoryImpl(long id);
}
