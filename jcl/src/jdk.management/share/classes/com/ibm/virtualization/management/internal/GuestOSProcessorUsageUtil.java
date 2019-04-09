/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.virtualization.management.internal;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.virtualization.management.GuestOSProcessorUsage;

/**
 * Support for the {@link GuestOSProcessorUsage} class.
 */
public final class GuestOSProcessorUsageUtil {

	private static CompositeType compositeType;

	/**
	 * @return an instance of {@link CompositeType} for the {@link GuestOSProcessorUsage} class
	 */
	public static CompositeType getCompositeType() {
		if (null == compositeType) {
			try {
				String[] names = { "cpuTime", "timestamp", //$NON-NLS-1$ //$NON-NLS-2$
						"cpuEntitlement", "hostCpuClockSpeed" }; //$NON-NLS-1$ //$NON-NLS-2$
				String[] descs = { "cpuTime", "timestamp", //$NON-NLS-1$ //$NON-NLS-2$
						"cpuEntitlement", "hostCpuClockSpeed" }; //$NON-NLS-1$ //$NON-NLS-2$
				OpenType<?>[] types = { SimpleType.LONG, SimpleType.LONG, SimpleType.FLOAT, SimpleType.LONG };

				compositeType = new CompositeType(
						GuestOSProcessorUsage.class.getName(),
						GuestOSProcessorUsage.class.getName(),
						names, descs, types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return compositeType;
	}

	/**
	 * @param usage a {@link GuestOSProcessorUsage} object
	 * @return a {@link CompositeData} object that represents the supplied <code>usage</code> object
	 */
	public static CompositeData toCompositeData(GuestOSProcessorUsage usage) {
		CompositeData result = null;

		if (null != usage) {
			CompositeType type = getCompositeType();
			String[] names = { "cpuTime", "timestamp", //$NON-NLS-1$ //$NON-NLS-2$
					"cpuEntitlement", "hostCpuClockSpeed" }; //$NON-NLS-1$ //$NON-NLS-2$
			Object[] values = {
					Long.valueOf(usage.getCpuTime()),
					Long.valueOf(usage.getTimestamp()),
					Float.valueOf(usage.getCpuEntitlement()),
					Long.valueOf(usage.getHostCpuClockSpeed()) };

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

	private GuestOSProcessorUsageUtil() {
		super();
	}

}
