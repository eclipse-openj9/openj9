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

import java.lang.management.LockInfo;
import java.lang.management.MonitorInfo;
import java.lang.management.ThreadInfo;

import javax.management.openmbean.ArrayType;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Support for the {@link ThreadInfo} class. 
 */
public final class ThreadInfoUtil {

	private static CompositeType compositeType;

	/**
	 * @return an instance of {@link CompositeType} for the {@link ThreadInfo} class
	 */
	public static CompositeType getCompositeType() {
		if (compositeType == null) {
			try {
				String[] names = { "threadId", "threadName", "threadState", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
						"suspended", "inNative", "blockedCount", "blockedTime", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
						"waitedCount", "waitedTime", "lockInfo", "lockName", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
						"lockOwnerId", "lockOwnerName", "stackTrace", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
						"lockedMonitors", "lockedSynchronizers" //$NON-NLS-1$ //$NON-NLS-2$
						/*[IF Sidecar19-SE]*/
						, "daemon", "priority" //$NON-NLS-1$ //$NON-NLS-2$
						/*[ENDIF]*/
				};
				String[] descs = { "threadId", "threadName", "threadState", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
						"suspended", "inNative", "blockedCount", "blockedTime", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
						"waitedCount", "waitedTime", "lockInfo", "lockName", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
						"lockOwnerId", "lockOwnerName", "stackTrace", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
						"lockedMonitors", "lockedSynchronizers" //$NON-NLS-1$ //$NON-NLS-2$
						/*[IF Sidecar19-SE]*/
						, "daemon", "priority" //$NON-NLS-1$ //$NON-NLS-2$
						/*[ENDIF]*/
				};
				OpenType<?>[] types = { SimpleType.LONG, SimpleType.STRING, SimpleType.STRING, SimpleType.BOOLEAN,
						SimpleType.BOOLEAN, SimpleType.LONG, SimpleType.LONG, SimpleType.LONG, SimpleType.LONG,
						LockInfoUtil.getCompositeType(), SimpleType.STRING, SimpleType.LONG, SimpleType.STRING,
						new ArrayType<>(1, StackTraceElementUtil.getCompositeType()),
						new ArrayType<>(1, MonitorInfoUtil.getCompositeType()),
						new ArrayType<>(1, LockInfoUtil.getCompositeType())
						/*[IF Sidecar19-SE]*/
						, SimpleType.BOOLEAN, SimpleType.INTEGER
						/*[ENDIF]*/
				};

				compositeType = new CompositeType(
						ThreadInfo.class.getName(),
						ThreadInfo.class.getName(),
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
	 * @param info a {@link ThreadInfo} object
	 * @return a {@link CompositeData} object that represents the supplied <code>info</code> object
	 */
	public static CompositeData toCompositeData(ThreadInfo info) {
		CompositeData result = null;

		if (info != null) {
			// deal with the array types first
			StackTraceElement[] st = info.getStackTrace();
			CompositeData[] stArray = new CompositeData[st.length];

			for (int i = 0; i < stArray.length; ++i) {
				stArray[i] = StackTraceElementUtil.toCompositeData(st[i]);
			}

			MonitorInfo[] lockedMonitors = info.getLockedMonitors();
			CompositeData[] lmArray = new CompositeData[lockedMonitors.length];

			for (int i = 0; i < lmArray.length; ++i) {
				lmArray[i] = MonitorInfoUtil.toCompositeData(lockedMonitors[i]);
			}

			LockInfo[] lockedSynchronizers = info.getLockedSynchronizers();
			CompositeData[] lsArray = new CompositeData[lockedSynchronizers.length];

			for (int i = 0; i < lsArray.length; ++i) {
				lsArray[i] = LockInfoUtil.toCompositeData(lockedSynchronizers[i]);
			}

			CompositeType type = ThreadInfoUtil.getCompositeType();
			String[] names = { "threadId", "threadName", "threadState", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
					"suspended", "inNative", "blockedCount", "blockedTime", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
					"waitedCount", "waitedTime", "lockInfo", "lockName", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
					"lockOwnerId", "lockOwnerName", "stackTrace", "lockedMonitors", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
					"lockedSynchronizers" //$NON-NLS-1$
					/*[IF Sidecar19-SE]*/
					, "daemon", "priority" //$NON-NLS-1$ //$NON-NLS-2$
					/*[ENDIF]*/
			};
			Object[] values = {
					Long.valueOf(info.getThreadId()),
					info.getThreadName(),
					info.getThreadState().name(),
					Boolean.valueOf(info.isSuspended()),
					Boolean.valueOf(info.isInNative()),
					Long.valueOf(info.getBlockedCount()),
					Long.valueOf(info.getBlockedTime()),
					Long.valueOf(info.getWaitedCount()),
					Long.valueOf(info.getWaitedTime()),
					LockInfoUtil.toCompositeData(info.getLockInfo()),
					info.getLockName(),
					Long.valueOf(info.getLockOwnerId()),
					info.getLockOwnerName(),
					stArray, lmArray, lsArray
					/*[IF Sidecar19-SE]*/
					, Boolean.valueOf(info.isDaemon()),
					Integer.valueOf(info.getPriority())
					/*[ENDIF]*/
			};

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


	private ThreadInfoUtil() {
		super();
	}

}
