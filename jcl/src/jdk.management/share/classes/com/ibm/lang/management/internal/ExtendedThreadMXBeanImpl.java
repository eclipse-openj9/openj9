/*[INCLUDE-IF Sidecar18-SE]*/
/*
 * Copyright IBM Corp. and others 2007
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.lang.management.internal;

import java.lang.management.ThreadInfo;

import com.ibm.java.lang.management.internal.ThreadMXBeanImpl;
import com.ibm.lang.management.ExtendedThreadInfo;
import com.ibm.lang.management.ThreadMXBean;

/**
 * Implementation of the extended ThreadMXBean.
 */
public final class ExtendedThreadMXBeanImpl extends ThreadMXBeanImpl implements ThreadMXBean {

	private static final ThreadMXBean instance = new ExtendedThreadMXBeanImpl();

	/**
	 * Singleton accessor method.
	 *
	 * @return the <code>ExtendedThreadMXBeanImpl</code> singleton.
	 */
	public static ThreadMXBean getInstance() {
		return instance;
	}

	private ExtendedThreadMXBeanImpl() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ExtendedThreadInfo[] dumpAllExtendedThreads(boolean lockedMonitors, boolean lockedSynchronizers)
			throws InternalError, SecurityException, UnsupportedOperationException {
		/* For security manager checks fall back on dumpAllThreads() to do this. */
		ThreadInfo[] threadInfos = dumpAllThreads(lockedMonitors, lockedSynchronizers);
		int infoCount = threadInfos.length;
		ExtendedThreadInfo[] resultArray = new ExtendedThreadInfo[infoCount];

		for (int index = 0; index < infoCount; ++index) {
			/* Construct the ExtendedThreadInfo object. */
			resultArray[index] = new ExtendedThreadInfoImpl(threadInfos[index]);
		}

		return resultArray;
	}

	private native static long getThreadAllocatedBytesImpl(long threadID);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getThreadAllocatedBytes(long threadId) {
		if (isThreadAllocatedMemorySupported()) {
			if (isThreadAllocatedMemoryEnabled()) {
				return getThreadAllocatedBytesImpl(threadId);
			} else {
				return -1;
			}
		} else {
			throw new UnsupportedOperationException();
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] getThreadAllocatedBytes(long[] threadIds) {
		int length = threadIds.length;
		long[] allocatedBytes = new long[length];

		for (int i = 0; i < length; i++) {
			allocatedBytes[i] = getThreadAllocatedBytes(threadIds[i]);
		}

		return allocatedBytes;
	}

	/*[IF Sidecar18-SE-OpenJ9]*/
	/**
	 * {@inheritDoc}
	 */
	@Override
	/*[ENDIF] Sidecar18-SE-OpenJ9 */
	public long getTotalThreadAllocatedBytes() {
		if (!isThreadAllocatedMemorySupported()) {
			throw new UnsupportedOperationException();
		}
		if (!isThreadAllocatedMemoryEnabled()) {
			return -1;
		}
		long total = 0;
		long[] allocatedBytes = getThreadAllocatedBytes(getAllThreadIds());
		for (long threadAllocated : allocatedBytes) {
			if (threadAllocated != -1) {
				total += threadAllocated;
			}
		}
		return total;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] getThreadCpuTime(long[] threadIds) {
		long[] result = new long[threadIds.length];
		for (int i = 0; i < threadIds.length; i++) {
			if (threadIds[i] <= 0) {
				/*[MSG "K05F7", "Thread id must be greater than 0."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
			}
			result[i] = -1;
		}
		if (isCurrentThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				for (int i = 0; i < threadIds.length; i++) {
					result[i] = getThreadCpuTime(threadIds[i]);
				}
			}
		} else {
			/*[MSG "K05F6", "CPU time measurement is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F6")); //$NON-NLS-1$
		}
		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] getThreadUserTime(long[] threadIds) {
		long[] result = new long[threadIds.length];
		for (int i = 0; i < threadIds.length; i++) {
			if (threadIds[i] <= 0) {
				/*[MSG "K05F7", "Thread id must be greater than 0."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
			}
			result[i] = -1;
		}
		if (isCurrentThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				for (int i = 0; i < threadIds.length; i++) {
					result[i] = getThreadUserTime(threadIds[i]);
				}
			}
		} else {
			/*[MSG "K05F6", "CPU time measurement is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F6")); //$NON-NLS-1$
		}
		return result;
	}

	private boolean isThreadAllocatedMemoryEnabled = true;

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadAllocatedMemorySupported() {
		/* Currently, this capability is always supported. */
		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadAllocatedMemoryEnabled() {
		return isThreadAllocatedMemoryEnabled && isThreadAllocatedMemorySupported();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setThreadAllocatedMemoryEnabled(boolean value) {
		isThreadAllocatedMemoryEnabled = value;
	}
}
