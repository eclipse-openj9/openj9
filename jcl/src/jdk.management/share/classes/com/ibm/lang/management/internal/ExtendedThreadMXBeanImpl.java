/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2023 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
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

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getThreadAllocatedBytes(long threadId) {
		throw new UnsupportedOperationException();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] getThreadAllocatedBytes(long[] threadIds) {
		throw new UnsupportedOperationException();
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

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadAllocatedMemorySupported() {
		return false;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadAllocatedMemoryEnabled() {
		throw new UnsupportedOperationException();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setThreadAllocatedMemoryEnabled(boolean value) {
		throw new UnsupportedOperationException();
	}
}
