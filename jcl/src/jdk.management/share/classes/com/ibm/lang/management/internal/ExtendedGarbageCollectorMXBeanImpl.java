/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

import javax.management.MBeanNotificationInfo;
import javax.management.ObjectName;

import com.ibm.java.lang.management.internal.GarbageCollectorMXBeanImpl;
import com.ibm.lang.management.GarbageCollectorMXBean;
import com.sun.management.GarbageCollectionNotificationInfo;
import com.sun.management.GcInfo;
import com.sun.management.internal.GcInfoUtil;

import java.lang.management.MemoryUsage;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.ManagementFactory;
import java.util.HashMap;
import java.util.Map;
import java.util.List;

/**
 * Runtime type for {@link com.ibm.lang.management.GarbageCollectorMXBean}.
 */
public final class ExtendedGarbageCollectorMXBeanImpl
		extends GarbageCollectorMXBeanImpl
		implements GarbageCollectorMXBean {
	
	ExtendedGarbageCollectorMXBeanImpl(ObjectName objectName, String name, int id, ExtendedMemoryMXBeanImpl memBean) {
		super(objectName, name, id, memBean);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MBeanNotificationInfo[] getNotificationInfo() {
		// We know what kinds of notifications we can emit whereas the
		// notifier delegate does not. So, for this method, no delegating.
		// Instead respond using our own metadata.
		MBeanNotificationInfo info = new MBeanNotificationInfo(
				new String[] { GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION },
				javax.management.Notification.class.getName(),
				"GarbageCollection Notification"); //$NON-NLS-1$
		return new MBeanNotificationInfo[] { info };
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public GcInfo getLastGcInfo() {
		return this.getLastGcInfoImpl(id);
	}

	/**
	 * @return the gc information of the most recent collection
	 * @see #getLastGcInfo()
	 */
	private native GcInfo getLastGcInfoImpl(int id);
 
	static GcInfo buildGcInfo(long index, long startTime, long endTime,
							long[] initialSize, long[] preUsed, long[] preCommitted, long[] preMax,
							long[] postUsed, long[] postCommitted, long[] postMax) {

		/* retrieve the names of MemoryPools*/
		List<MemoryPoolMXBean> memoryPoolList = ManagementFactory.getMemoryPoolMXBeans();
		String[] poolNames = new String[memoryPoolList.size()];
		int idx = 0;
		for (MemoryPoolMXBean bean : memoryPoolList) {
			poolNames[idx++] = bean.getName();
		}
		Map<String,MemoryUsage> usageBeforeGc = new HashMap<>(poolNames.length); 
		Map<String,MemoryUsage> usageAfterGc = new HashMap<>(poolNames.length);
		for (int count = 0; count < poolNames.length; ++count) {
			usageBeforeGc.put(poolNames[count], new MemoryUsage(initialSize[count], preUsed[count],  preCommitted[count],  preMax[count]));
			usageAfterGc.put(poolNames[count], new MemoryUsage(initialSize[count], postUsed[count], postCommitted[count], postMax[count]));
		}
		return GcInfoUtil.newGcInfoInstance(index, startTime, endTime, usageBeforeGc, usageAfterGc);
	}
}
