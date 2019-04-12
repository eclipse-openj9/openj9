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
package com.sun.management.internal;

import java.lang.management.MemoryUsage;
import java.lang.reflect.Constructor;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Map;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.java.lang.management.internal.MemoryUsageUtil;
import com.sun.management.GcInfo;

/**
 * Support for the {@link GcInfo} class. 
 */
public final class GcInfoUtil {

	private static CompositeType compositeType;

	private static Constructor<GcInfo> gcInfoPrivateConstructor = null;
	private static Constructor<GcInfo> getGcInfoPrivateConstructor() {
		if (null == gcInfoPrivateConstructor) {
			gcInfoPrivateConstructor = AccessController.doPrivileged(new PrivilegedAction<Constructor<GcInfo>>() {
				@Override
				public Constructor<GcInfo> run() {
					try {
						Constructor<GcInfo> constructor = GcInfo.class.getDeclaredConstructor(Long.TYPE, Long.TYPE, Long.TYPE, Map.class, Map.class);
						constructor.setAccessible(true);
						return constructor;
					} catch (NoSuchMethodException e) {
						/* Handle all sorts of internal errors arising due to reflection by rethrowing an InternalError. */
						/*[MSG "K0660", "Internal error while obtaining private constructor."]*/
						InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K0660")); //$NON-NLS-1$
						error.initCause(e);
						throw error;
					}
				}
			});
		}
		return gcInfoPrivateConstructor;
	}

	/**
	 * @return an instance of {@link CompositeType} for the {@link GcInfo} class
	 */
	public static CompositeType getCompositeType() {
		if (null == compositeType) {
			String[] names = { "index", "startTime", "endTime", "usageBeforeGc", "usageAfterGc" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
			String[] descs = { "index", "startTime", "endTime", "usageBeforeGc", "usageAfterGc" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
			OpenType<?>[] types = { SimpleType.LONG, SimpleType.LONG, SimpleType.LONG,
					MemoryUsageUtil.getTabularType(), MemoryUsageUtil.getTabularType() };

			try {
				compositeType = new CompositeType(
						GcInfo.class.getName(),
						GcInfo.class.getName(),
						names,
						descs,
						types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return compositeType;
	}

	/**
	 * @param info the garbage collection information
	 * @return a new {@link CompositeData} instance that represents the supplied <code>info</code> object
	 */
	public static CompositeData toCompositeData(GcInfo info) {
		CompositeData result = null;
		if (null != info) {
			CompositeType type = getCompositeType();
			String[] names = { "index", "startTime", "endTime", "usageBeforeGc", "usageAfterGc" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
			Object[] values = {
					Long.valueOf(info.getId()),
					Long.valueOf(info.getStartTime()),
					Long.valueOf(info.getEndTime()),
					MemoryUsageUtil.toTabularData(info.getMemoryUsageBeforeGc()),
					MemoryUsageUtil.toTabularData(info.getMemoryUsageAfterGc()) };

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
     * @param index
     * 			  the identifier of this garbage collection which is the number of collections that this collector has done
     * @param startTime
     * 			  the start time of the collection in milliseconds since the Java virtual machine was started.
     * @param endTime
     * 			  the end time of the collection in milliseconds since the Java virtual machine was started.
     * @param usageBeforeGc
     * 			  the memory usage of all memory pools at the beginning of this GC.
     * @param usageAfterGc
     * 			  the memory usage of all memory pools at the end of this GC.
	 * @return a <code>GcInfo</code> object
	 */
	public static GcInfo newGcInfoInstance(long index, long startTime, long endTime, Map<String,MemoryUsage> usageBeforeGc, Map<String,MemoryUsage> usageAfterGc) {
		GcInfo gcInfo = null;
		Constructor<GcInfo> gcInfoConstructor = getGcInfoPrivateConstructor();
		try {
			gcInfo = gcInfoConstructor.newInstance(index, startTime, endTime, usageBeforeGc, usageAfterGc);
		} catch (Exception e) {
			/*[MSG "K0661", "Internal error while obtaining GcInfo instance."]*/
			InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K0661")); //$NON-NLS-1$
			error.initCause(e);
			throw error;
		}
		return gcInfo;
	}

	private GcInfoUtil() {
		super();
	}

}
