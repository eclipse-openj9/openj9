/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2007, 2018 IBM Corp. and others
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

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

import javax.management.ObjectName;

/**
 * Runtime type for {@link java.lang.management.ThreadMXBean}.
 *
 * @since 1.5
 */
public class ThreadMXBeanImpl implements ThreadMXBean {

	private static final ThreadMXBeanImpl instance = new ThreadMXBeanImpl();
	private static Boolean isThreadCpuTimeEnabled = null;
	private static Boolean isThreadCpuTimeSupported = null;

	private final ObjectName objectName;

	/**
	 * Protected constructor to limit instantiation.
	 * Sets the metadata for this bean.
	 */
	protected ThreadMXBeanImpl() {
		super();
		objectName = ManagementUtils.createObjectName(ManagementFactory.THREAD_MXBEAN_NAME);
	}

	/**
	 * Singleton accessor method.
	 * 
	 * @return the <code>ThreadMXBeanImpl</code> singleton.
	 */
	public static ThreadMXBean getInstance() {
		return instance;
	}

	/**
	 * @return an array of the identifiers of every thread in the virtual
	 *         machine that has been detected as currently being in a deadlock
	 *         situation over an object monitor. The return will not include the
	 *         id of any threads blocked on ownable synchronizers. The
	 *         information returned from this method will be a subset of that
	 *         returned from {@link #findDeadlockedThreadsImpl()}.
	 * @see #findMonitorDeadlockedThreads()
	 */
	private native long[] findMonitorDeadlockedThreadsImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] findMonitorDeadlockedThreads() {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		return this.findMonitorDeadlockedThreadsImpl();
	}

	/**
	 * @return the identifiers of all of the threads currently alive in the
	 *         virtual machine.
	 * @see #getAllThreadIds()
	 */
	private native long[] getAllThreadIdsImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] getAllThreadIds() {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		return this.getAllThreadIdsImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getCurrentThreadCpuTime() {
		long result = -1;
		if (isCurrentThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				result = this.getThreadCpuTimeImpl(Thread.currentThread().getId());
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
	public long getCurrentThreadUserTime() {
		long result = -1;
		if (isCurrentThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				result = this.getThreadUserTimeImpl(Thread.currentThread().getId());
			}
		} else {
			/*[MSG "K05F6", "CPU time measurement is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F6")); //$NON-NLS-1$
		}
		return result;
	}

	/**
	 * @return the number of currently alive daemon threads.
	 * @see #getDaemonThreadCount()
	 */
	private native int getDaemonThreadCountImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getDaemonThreadCount() {
		return this.getDaemonThreadCountImpl();
	}

	/**
	 * @return the peak number of live threads
	 * @see #getPeakThreadCount()
	 */
	private native int getPeakThreadCountImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getPeakThreadCount() {
		return this.getPeakThreadCountImpl();
	}

	/**
	 * @return the number of currently alive threads.
	 * @see #getThreadCount()
	 */
	private native int getThreadCountImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getThreadCount() {
		return this.getThreadCountImpl();
	}

	/**
	 * @param id
	 *            the identifier for a thread. Must be a positive number greater
	 *            than zero.
	 * @return on virtual machines where thread CPU timing is supported and
	 *         enabled, and there is a living thread with identifier
	 *         <code>id</code>, the number of nanoseconds CPU time used by
	 *         the thread. On virtual machines where thread CPU timing is
	 *         supported but not enabled, or where there is no living thread
	 *         with identifier <code>id</code> present in the virtual machine,
	 *         a value of <code>-1</code> is returned.
	 * @see #getThreadCpuTime(long)
	 */
	private native long getThreadCpuTimeImpl(long id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getThreadCpuTime(long id) {
		// Validate input.
		if (id <= 0) {
			/*[MSG "K05F7", "Thread id must be greater than 0."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
		}

		long result = -1;
		if (isThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				result = this.getThreadCpuTimeImpl(id);
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
	public ThreadInfo getThreadInfo(long id) {
		return getThreadInfo(id, 0);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ThreadInfo[] getThreadInfo(long[] ids) {
		return getThreadInfo(ids, 0);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ThreadInfo[] getThreadInfo(long[] ids, int maxDepth) {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}

		// Validate inputs
		for (long id : ids) {
			if (id <= 0) {
				/*[MSG "K05F7", "Thread id must be greater than 0."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
			}
		}

		if (maxDepth < 0) {
			/*[MSG "K05F8", "maxDepth value cannot be negative."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F8")); //$NON-NLS-1$
		}

		// Create an array and populate with individual ThreadInfos
		return this.getMultiThreadInfoImpl(ids, maxDepth, false, false);
	}

	/**
	 * Get together information for threads and create instances of the
	 * ThreadInfo class.
	 * <p>
	 * Assumes that caller has already carried out error checking on the
	 * <code>id</code> and <code>maxDepth</code> arguments.
	 * </p>
	 * 
	 * @param ids
	 *            thread ids
	 * @param lockedMonitors
	 *            if <code>true</code> attempt to set the returned
	 *            <code>ThreadInfo</code> with details of object monitors
	 *            locked by the specified thread
	 * @param lockedSynchronizers
	 *            if <code>true</code> attempt to set the returned
	 *            <code>ThreadInfo</code> with details of ownable
	 *            synchronizers locked by the specified thread
	 * @return information for threads
	 */
	@Override
	public ThreadInfo[] getThreadInfo(long[] ids, boolean lockedMonitors,
			boolean lockedSynchronizers) {
		return getThreadInfoCommon(ids, lockedMonitors, lockedSynchronizers, Integer.MAX_VALUE);
	}

	/*[IF Java10]*/
	@Override
	public ThreadInfo[] getThreadInfo(long[] ids, boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth) {
		/*[MSG "K0662", "maxDepth must not be negative."]*/
		if (maxDepth < 0) {
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0662")); //$NON-NLS-1$
		}
		return getThreadInfoCommon(ids, lockedMonitors, lockedSynchronizers, maxDepth);
	}
	/*[ENDIF]*/ // Java10

	private ThreadInfo[] getThreadInfoCommon(long[] ids, boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth) {
		// Verify inputs
		if (lockedMonitors && !isObjectMonitorUsageSupported()) {
			/*[MSG "K05FC", "Monitoring of object monitors is not supported on this virtual machine"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FC")); //$NON-NLS-1$
		}
		if (lockedSynchronizers && !isSynchronizerUsageSupported()) {
			/*[MSG "K05FB", "Monitoring of ownable synchronizer usage is not supported on this virtual machine"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FB")); //$NON-NLS-1$
		}

		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}

		// Validate thread ids
		for (long id : ids) {
			if (id <= 0) {
				/*[MSG "K05F7", "Thread id must be greater than 0."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
			}
		}

		// Create an array and populate with individual ThreadInfos
		return this.getMultiThreadInfoImpl(ids, maxDepth,
				lockedMonitors, lockedSynchronizers);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ThreadInfo getThreadInfo(long id, int maxDepth) {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}

		// Validate inputs
		if (id <= 0) {
			/*[MSG "K05F7", "Thread id must be greater than 0."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
		}
		if (maxDepth < 0) {
			/*[MSG "K05F8", "maxDepth value cannot be negative."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F8")); //$NON-NLS-1$
		}
		return this.getThreadInfoImpl(id, maxDepth);
	}

	/**
	 * Get together information for a thread and create an instance of the
	 * ThreadInfo class.
	 * <p>
	 * Assumes that caller has already carried out error checking on the
	 * <code>id</code> and <code>maxDepth</code> arguments.
	 * </p>
	 * 
	 * @param id
	 *            thread id
	 * @param maxStackDepth
	 *            maximum depth of the stack trace
	 * @return ThreadInfo of the give thread
	 */
	private native ThreadInfo getThreadInfoImpl(long id, int maxStackDepth);

	/**
	 * @param id
	 *            the identifier for a thread. Must be a positive number greater
	 *            than zero.
	 * @return on virtual machines where thread CPU timing is supported and
	 *         enabled, and there is a living thread with identifier
	 *         <code>id</code>, the number of nanoseconds CPU time used by
	 *         the thread running in user mode. On virtual machines where thread
	 *         CPU timing is supported but not enabled, or where there is no
	 *         living thread with identifier <code>id</code> present in the
	 *         virtual machine, a value of <code>-1</code> is returned.
	 *         <p>
	 *         If thread CPU timing was disabled when the thread was started
	 *         then the virtual machine is free to choose any measurement start
	 *         time between when the virtual machine started up and when thread
	 *         CPU timing was enabled with a call to
	 *         {@link #setThreadCpuTimeEnabled(boolean)}.
	 *         </p>
	 * @see #getThreadUserTime(long)
	 */
	private native long getThreadUserTimeImpl(long id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getThreadUserTime(long id) {
		// Validate input.
		if (id <= 0) {
			/*[MSG "K05F7", "Thread id must be greater than 0."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F7")); //$NON-NLS-1$
		}

		long result = -1;
		if (isThreadCpuTimeSupported()) {
			if (isThreadCpuTimeEnabled()) {
				result = this.getThreadUserTimeImpl(id);
			}
		} else {
			/*[MSG "K05F6", "CPU time measurement is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F6")); //$NON-NLS-1$
		}
		return result;
	}

	/**
	 * @return the total number of started threads.
	 * @see #getTotalStartedThreadCount()
	 */
	private native long getTotalStartedThreadCountImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getTotalStartedThreadCount() {
		return this.getTotalStartedThreadCountImpl();
	}

	/**
	 * @return <code>true</code> if CPU timing of the current thread is
	 *         supported, otherwise <code>false</code>.
	 * @see #isCurrentThreadCpuTimeSupported()
	 */
	private native boolean isCurrentThreadCpuTimeSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isCurrentThreadCpuTimeSupported() {
		return this.isCurrentThreadCpuTimeSupportedImpl();
	}

	/**
	 * @return <code>true</code> if thread contention monitoring is enabled,
	 *         <code>false</code> otherwise.
	 * @see #isThreadContentionMonitoringEnabled()
	 */
	private native boolean isThreadContentionMonitoringEnabledImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadContentionMonitoringEnabled() {
		if (!isThreadContentionMonitoringSupported()) {
			/*[MSG "K05FA", "Thread contention monitoring is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FA")); //$NON-NLS-1$
		}
		return this.isThreadContentionMonitoringEnabledImpl();
	}

	/**
	 * @return <code>true</code> if thread contention monitoring is supported,
	 *         <code>false</code> otherwise.
	 * @see #isThreadContentionMonitoringSupported()
	 */
	private native boolean isThreadContentionMonitoringSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadContentionMonitoringSupported() {
		return this.isThreadContentionMonitoringSupportedImpl();
	}

	/**
	 * @return <code>true</code> if thread CPU timing is enabled,
	 *         <code>false</code> otherwise.
	 * @see #isThreadCpuTimeEnabled()
	 */
	private native boolean isThreadCpuTimeEnabledImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadCpuTimeEnabled() {
		if (!isThreadCpuTimeSupported() && !isCurrentThreadCpuTimeSupported()) {
			/*[MSG "K05F9", "Thread CPU timing is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F9")); //$NON-NLS-1$
		}
		if (isThreadCpuTimeEnabled == null) {
			isThreadCpuTimeEnabled = Boolean.valueOf(this.isThreadCpuTimeEnabledImpl());
		}
		return isThreadCpuTimeEnabled.booleanValue();
	}

	/**
	 * @return <code>true</code> if the virtual machine supports the CPU
	 *         timing of threads, <code>false</code> otherwise.
	 * @see #isThreadCpuTimeSupported()
	 */
	private native boolean isThreadCpuTimeSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isThreadCpuTimeSupported() {
		if (isThreadCpuTimeSupported == null) {
			isThreadCpuTimeSupported = Boolean.valueOf(this.isThreadCpuTimeSupportedImpl());
		}
		return isThreadCpuTimeSupported.booleanValue();
	}

	/**
	 * @see #resetPeakThreadCount()
	 */
	private native void resetPeakThreadCountImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void resetPeakThreadCount() {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		this.resetPeakThreadCountImpl();
	}

	/**
	 * @param enable
	 *            enable thread contention monitoring if <code>true</code>,
	 *            otherwise disable thread contention monitoring.
	 */
	private native void setThreadContentionMonitoringEnabledImpl(boolean enable);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setThreadContentionMonitoringEnabled(boolean enable) {
		if (!isThreadContentionMonitoringSupported()) {
			/*[MSG "K05FA", "Thread contention monitoring is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FA")); //$NON-NLS-1$
		}

		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		this.setThreadContentionMonitoringEnabledImpl(enable);
	}

	/**
	 * @param enable
	 *            enable thread CPU timing if <code>true</code>, otherwise
	 *            disable thread CPU timing
	 */
	private native void setThreadCpuTimeEnabledImpl(boolean enable);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setThreadCpuTimeEnabled(boolean enable) {
		if (!isThreadCpuTimeSupported() && !isCurrentThreadCpuTimeSupported()) {
			/*[MSG "K05F9", "Thread CPU timing is not supported on this virtual machine."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F9")); //$NON-NLS-1$
		}

		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		this.setThreadCpuTimeEnabledImpl(enable);
		isThreadCpuTimeEnabled = Boolean.valueOf(enable);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isObjectMonitorUsageSupported() {
		return this.isObjectMonitorUsageSupportedImpl();
	}

	/**
	 * @return <code>true</code> if the VM supports monitoring of object
	 *         monitors, <code>false</code> otherwise
	 */
	private native boolean isObjectMonitorUsageSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isSynchronizerUsageSupported() {
		return this.isSynchronizerUsageSupportedImpl();
	}

	/**
	 * @return <code>true</code> if the VM supports the monitoring of ownable
	 *         synchronizers, <code>false</code> otherwise
	 */
	private native boolean isSynchronizerUsageSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long[] findDeadlockedThreads() {
		if (!isSynchronizerUsageSupported()) {
			/*[MSG "K05FB", "Monitoring of ownable synchronizer usage is not supported on this virtual machine"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FB")); //$NON-NLS-1$
		}

		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		return this.findDeadlockedThreadsImpl();
	}

	/**
	 * @return an array of the identifiers of every thread in the virtual
	 *         machine that has been detected as currently being in a deadlock
	 *         situation over an object monitor or an ownable synchronizer.
	 * @see #findDeadlockedThreads()
	 */
	private native long[] findDeadlockedThreadsImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ThreadInfo[] dumpAllThreads(boolean lockedMonitors, boolean lockedSynchronizers) {
		return dumpAllThreadsCommon(lockedMonitors, lockedSynchronizers, Integer.MAX_VALUE);
	}

	private ThreadInfo[] dumpAllThreadsCommon(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth) {
		if (lockedMonitors && !isObjectMonitorUsageSupported()) {
			/*[MSG "K05FC", "Monitoring of object monitors is not supported on this virtual machine"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FC")); //$NON-NLS-1$
		}
		if (lockedSynchronizers && !isSynchronizerUsageSupported()) {
			/*[MSG "K05FB", "Monitoring of ownable synchronizer usage is not supported on this virtual machine"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05FB")); //$NON-NLS-1$
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		return dumpAllThreadsImpl(lockedMonitors, lockedSynchronizers, maxDepth);
	}

	/*[IF Java10]*/
	/**
	 * {@inheritDoc}
	 */
	@Override
	public ThreadInfo[] dumpAllThreads(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth) {
		/*[MSG "K0662", "maxDepth must not be negative."]*/
		if (maxDepth < 0) {
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0662")); //$NON-NLS-1$
		}
		return dumpAllThreadsCommon(lockedMonitors, lockedSynchronizers, maxDepth);
	}
	/*[ENDIF]*/ // Java10
	
	/**
	 * @param lockedMonitors
	 *            if <code>true</code> then include details of locked object
	 *            monitors in returned array
	 * @param lockedSynchronizers
	 *            if <code>true</code> then include details of locked ownable
	 *            synchronizers in returned array
	 * @param maxDepth
	 *            Limit the number of frames returned.  Negative values indicate no limit.
	 * @return an array of <code>ThreadInfo</code> objects&nbsp;-&nbsp;one for
	 *         each thread running in the virtual machine
	 * @since 10
	 */
	private native ThreadInfo[] dumpAllThreadsImpl(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth);

	/**
	 * Answers an array of instances of the ThreadInfo class according to ids
	 * 
	 * @param ids
	 *            thread ids
	 * @param maxStackDepth 
	 *            the max stack depth
	 * @param getLockedMonitors
	 *            if <code>true</code> attempt to set the returned
	 *            <code>ThreadInfo</code> with details of object monitors
	 *            locked by the specified thread
	 * @param getLockedSynchronizers
	 *            if <code>true</code> attempt to set the returned
	 *            <code>ThreadInfo</code> with details of ownable
	 *            synchronizers locked by the specified thread
	 * @return new {@link ThreadInfo} instance
	 */   
	private native ThreadInfo[] getMultiThreadInfoImpl(long[] ids, int maxStackDepth,
			boolean getLockedMonitors, boolean getLockedSynchronizers);

	/**
	 * To satisfy com.ibm.lang.management.ThreadMXBean.
	 */
	public long[] getNativeThreadIds(long[] threadIDs) throws IllegalArgumentException, SecurityException {
		SecurityManager security = System.getSecurityManager();
		if (null != security) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		/* Prevent users from modifying this after we've validated it. */
		long[] localThreadIDs = threadIDs.clone();
		for (long iter : localThreadIDs) {
			if (iter <= 0) {
				/*[MSG "K05FD", "Invalid thread identifier ({0}) specified."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05FD", iter)); //$NON-NLS-1$
			}
		}
		/* Allocate space for the output array to hold native IDs. */
		long[] nativeIDs = new long[threadIDs.length];
		getNativeThreadIdsImpl(localThreadIDs, nativeIDs);
		return nativeIDs;
	}

	/**
	 * To satisfy com.ibm.lang.management.
	 */
	public long getNativeThreadId(long threadId) throws IllegalArgumentException, SecurityException {
		SecurityManager security = System.getSecurityManager();
		if (null != security) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
		if (threadId <= 0) {
			/*[MSG "K05FD", "Invalid thread identifier ({0}) specified."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05FD", threadId)); //$NON-NLS-1$
		}
		return findNativeThreadIDImpl(threadId);
	}

	private static native long findNativeThreadIDImpl(long threadId);

	private native void getNativeThreadIdsImpl(long[] tids, long[] nativeTIDs);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		return objectName;
	}

}
