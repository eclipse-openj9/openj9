/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
package com.ibm.java.lang.management.internal;

import java.lang.management.MemoryUsage;
import java.util.Map;
import java.util.Map.Entry;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularData;
import javax.management.openmbean.TabularDataSupport;
import javax.management.openmbean.TabularType;

/**
 * Support for the {@link MemoryUsage} class.
 */
public final class MemoryUsageUtil {

	private static CompositeType compositeType;

	private static CompositeType tabularRowType;

	private static TabularType tabularType;

	/**
	 * @return an instance of {@link CompositeType} for the {@link MemoryUsage} class
	 */
	public static CompositeType getCompositeType() {
		if (compositeType == null) {
			String[] names = { "init", "used", "committed", "max" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
			String[] descs = { "init", "used", "committed", "max" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
			OpenType<?>[] types = { SimpleType.LONG, SimpleType.LONG, SimpleType.LONG, SimpleType.LONG };

			try {
				compositeType = new CompositeType(
						MemoryUsage.class.getName(),
						MemoryUsage.class.getName(),
						names, descs, types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return compositeType;
	}

	private static CompositeType getTabularRowType() {
		if (null == tabularRowType) {
			String[] names = { "key", "value" }; //$NON-NLS-1$ //$NON-NLS-2$
			String[] descs = { "key", "value" }; //$NON-NLS-1$ //$NON-NLS-2$
			OpenType<?>[] types = { SimpleType.STRING, getCompositeType() };

			try {
				tabularRowType = new CompositeType(
						"MemoryUsageKeyValue", //$NON-NLS-1$
						"MemoryUsageKeyValue", //$NON-NLS-1$
						names, descs, types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return tabularRowType;
	}

	/**
	 * @return an instance of {@link TabularType} for the {@link MemoryUsage} class
	 */
	public static TabularType getTabularType() {
		if (null == tabularType) {
			CompositeType rowType = getTabularRowType();

			try {
				tabularType = new TabularType(
						"Memory Usage Map", //$NON-NLS-1$
						"Memory Usage Map", //$NON-NLS-1$
						rowType, new String[] { "key" }); //$NON-NLS-1$
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return tabularType;
	}

	/**
	 * @param usage a {@link MemoryUsage} object
	 * @return a {@link CompositeData} object that represents the supplied <code>usage</code> object
	 */
	public static CompositeData toCompositeData(MemoryUsage usage) {
		CompositeData result = null;

		if (usage != null) {
			CompositeType type = getCompositeType();
			String[] names = { "init", "used", "committed", "max" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
			Object[] values = { Long.valueOf(usage.getInit()), Long.valueOf(usage.getUsed()),
					Long.valueOf(usage.getCommitted()), Long.valueOf(usage.getMax()) };

			try {
				result = new CompositeDataSupport(type, names, values);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return result;
	}

	/**
	 * @param map a set of named {@link MemoryUsage} objects
	 * @return a {@link TabularData} object that represents the supplied map
	 */
	public static TabularData toTabularData(Map<String, MemoryUsage> map) {
		TabularData table = null;

		if (null != map) {
			CompositeType type = getTabularRowType();
			String[] names = { "key", "value" }; //$NON-NLS-1$ //$NON-NLS-2$

			table = new TabularDataSupport(getTabularType());

			for (Entry<String, MemoryUsage> entry : map.entrySet()) {
				Object[] values = { entry.getKey(), toCompositeData(entry.getValue()) };
				CompositeData row = null;

				try {
					row = new CompositeDataSupport(type, names, values);
				} catch (OpenDataException e) {
					if (ManagementUtils.VERBOSE_MODE) {
						e.printStackTrace(System.err);
					}
				}

				table.put(row);
			}
		}

		return table;
	}

	private MemoryUsageUtil() {
		super();
	}

}
