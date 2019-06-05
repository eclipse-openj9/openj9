/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
package java.lang.management;

import java.lang.Thread.State;
import java.util.Arrays;
import java.util.StringTokenizer;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.InvalidKeyException;

import com.ibm.java.lang.management.internal.LockInfoUtil;
import com.ibm.java.lang.management.internal.ManagementAccessControl;
import com.ibm.java.lang.management.internal.MonitorInfoUtil;
import com.ibm.java.lang.management.internal.StackTraceElementUtil;
import com.ibm.java.lang.management.internal.ThreadInfoUtil;
import com.ibm.oti.util.Util;

/**
 * Information about a snapshot of the state of a thread.
 *
 * @since 1.5
 */
public class ThreadInfo {

	/* Set up access to ThreadInfo shared secret.*/
	static {
		ManagementAccessControl.setThreadInfoAccess(new ThreadInfoAccessImpl());
	}

	private long threadId;

	private long nativeTId;

	private String threadName;

	private Thread.State threadState;

	private boolean suspended;

	private boolean inNative;

	private long blockedCount;

	private long blockedTime;

	private long waitedCount;

	private long waitedTime;

	private String lockName;

	private long lockOwnerId = -1;

	private String lockOwnerName;

	private StackTraceElement[] stackTraces = new StackTraceElement[0];

	private LockInfo lockInfo;

	private LockInfo[] lockedSynchronizers = new LockInfo[0];

	private MonitorInfo[] lockedMonitors = new MonitorInfo[0];

	private String TOSTRING_VALUE;

	/*[IF Sidecar19-SE]*/
	private boolean daemon;

	private int priority;
	/*[ENDIF]*/
	
	/**
	 * Creates a new <code>ThreadInfo</code> instance.
	 * 
	 * @param threadIdVal
	 * @param threadNameVal
	 * @param threadStateVal
	 * @param suspendedVal
	 * @param inNativeVal
	 * @param blockedCountVal
	 * @param blockedTimeVal
	 * @param waitedCountVal
	 * @param waitedTimeVal
	 * @param lockNameVal
	 * @param lockOwnerIdVal
	 * @param lockOwnerNameVal
	 * @param stackTraceVal
	 * @param lockInfo
	 * @param lockedMonitors
	 * @param lockedSynchronizers
/*[IF Sidecar19-SE]
	 * @param daemon
	 * @param priority
/*[ENDIF]
	 */
	private ThreadInfo(long threadIdVal, String threadNameVal,
			Thread.State threadStateVal, boolean suspendedVal,
			boolean inNativeVal, long blockedCountVal, long blockedTimeVal,
			long waitedCountVal, long waitedTimeVal, String lockNameVal,
			long lockOwnerIdVal, String lockOwnerNameVal,
			StackTraceElement[] stackTraceVal, LockInfo lockInfo,
			MonitorInfo[] lockedMonitors, LockInfo[] lockedSynchronizers
			/*[IF Sidecar19-SE]*/
			, boolean daemon, int priority
			/*[ENDIF]*/
	) {
		this.threadId = threadIdVal;
		this.threadName = threadNameVal;
		this.threadState = threadStateVal;
		this.suspended = suspendedVal;
		this.inNative = inNativeVal;
		this.blockedCount = blockedCountVal;
		this.blockedTime = blockedTimeVal;
		this.waitedCount = waitedCountVal;
		this.waitedTime = waitedTimeVal;
		this.lockName = lockNameVal;
		this.lockOwnerId = lockOwnerIdVal;
		this.lockOwnerName = lockOwnerNameVal;
		this.stackTraces = stackTraceVal;
		this.lockInfo = lockInfo;
		this.lockedMonitors = lockedMonitors;
		this.lockedSynchronizers = lockedSynchronizers;
		/*[IF Sidecar19-SE]*/
		this.daemon = daemon;
		this.priority = priority;
		/*[ENDIF]*/
	}

	/**
	 * @param thread
	 * @param state
	 * @param isSuspended
	 * @param isInNative
	 * @param blockedCount
	 * @param blockedTime
	 * @param waitedCount
	 * @param waitedTime
	 * @param stackTrace
	 * @param blockingMonitor
	 * @param lockOwner
	 */
	private ThreadInfo(Thread thread, long nativeId, int state, boolean isSuspended,
			boolean isInNative, long blockedCount, long blockedTime,
			long waitedCount, long waitedTime, StackTraceElement[] stackTrace,
			Object blockingMonitor, Thread lockOwner) {
		this.threadId = thread.getId();
		this.nativeTId = nativeId;
		this.threadName = thread.getName();
		/*[IF Sidecar19-SE]*/
		this.daemon = thread.isDaemon();
		this.priority = thread.getPriority();
		/*[ENDIF]*/

		// Use the supplied state int to index into the array returned
		// by values() which should be all thread states in the order
		// they have been declared. Doing this rather than just issuing a
		// call to thread.getState() because the Thread state may have
		// changed after the VM issued the call to this method.
		this.threadState = Thread.State.values()[state];
		this.suspended = isSuspended;
		this.inNative = isInNative;
		this.blockedCount = blockedCount;
		this.blockedTime = blockedTime;
		this.waitedCount = waitedCount;
		this.waitedTime = waitedTime;

		if (lockOwner != null) {
			this.lockOwnerId = lockOwner.getId();
			this.lockOwnerName = lockOwner.getName();
		}

		this.stackTraces = stackTrace;
		if (blockingMonitor != null) {
			this.lockName = blockingMonitor.getClass().getName()
					+ '@'
					+ Integer.toHexString(System
							.identityHashCode(blockingMonitor));
		}
	}

	/**
	 * @param thread
	 * @param state
	 * @param isSuspended
	 * @param isInNative
	 * @param blockedCount
	 * @param blockedTime
	 * @param waitedCount
	 * @param waitedTime
	 * @param stackTrace
	 * @param blockingObject 
	 * @param blockingObjectOwner 
	 * @param lockedMonitors
	 * @param lockedSynchronizers
	 */
	private ThreadInfo(Thread thread, long nativeId, int state, boolean isSuspended,
			boolean isInNative, long blockedCount, long blockedTime,
			long waitedCount, long waitedTime, StackTraceElement[] stackTrace,
			Object blockingObject, Thread blockingObjectOwner,
			MonitorInfo[] lockedMonitors, LockInfo[] lockedSynchronizers) {
		this(thread,nativeId,state,isSuspended,isInNative,blockedCount,blockedTime,waitedCount,waitedTime,stackTrace,blockingObject,blockingObjectOwner);

		this.lockedMonitors = lockedMonitors;
		this.lockedSynchronizers = lockedSynchronizers;

		if (blockingObject != null) {
			this.lockInfo = new LockInfo(blockingObject.getClass().getName(),
					System.identityHashCode(blockingObject));
		}

		if (this.lockInfo != null) {
			this.lockName = this.lockInfo.toString();
		}
	}

	/**
	 * Returns the number of times that the thread represented by this
	 * <code>ThreadInfo</code> has been blocked on any monitor objects. The
	 * count is from the start of the thread's life.
	 * 
	 * @return the number of times the corresponding thread has been blocked on
	 *         a monitor.
	 */
	public long getBlockedCount() {
		return this.blockedCount;
	}

	/**
	 * If thread contention monitoring is supported and enabled, returns the
	 * total amount of time that the thread represented by this
	 * <code>ThreadInfo</code> has spent blocked on any monitor objects. The
	 * time is measured in milliseconds and will be measured over the time period
	 * since thread contention was most recently enabled.
	 * 
	 * @return if thread contention monitoring is currently enabled, the number
	 *         of milliseconds that the thread associated with this
	 *         <code>ThreadInfo</code> has spent blocked on any monitors. If
	 *         thread contention monitoring is supported but currently disabled,
	 *         <code>-1</code>.
	 * @throws UnsupportedOperationException
	 *             if the virtual machine does not support thread contention
	 *             monitoring.
	 * @see ThreadMXBean#isThreadContentionMonitoringSupported()
	 * @see ThreadMXBean#isThreadContentionMonitoringEnabled()
	 */
	public long getBlockedTime() {
		return this.blockedTime;
	}

	/**
	 * If the thread represented by this <code>ThreadInfo</code> is currently
	 * blocked on or waiting on a monitor object, returns a string
	 * representation of that monitor object.
	 * <p>
	 * The monitor's string representation is comprised of the following
	 * component parts:
	 * <ul>
	 * <li><code>monitor</code> class name
	 * <li><code>@</code>
	 * <li><code>Integer.toHexString(System.identityHashCode(monitor))</code>
	 * </ul>
	 * @return if blocked or waiting on a monitor, a string representation of
	 *         the monitor object. Otherwise, <code>null</code>.
	 * @see Integer#toHexString(int)
	 * @see System#identityHashCode(java.lang.Object)
	 */
	public String getLockName() {
		return this.lockName;
	}

	/**
	 * If the thread represented by this <code>ThreadInfo</code> is currently
	 * blocked on or waiting on a monitor object, returns the thread identifier
	 * of the thread which owns the monitor.
	 * 
	 * @return the thread identifier of the other thread which holds the monitor
	 *         that the thread associated with this <code>ThreadInfo</code> is
	 *         blocked or waiting on. If this <code>ThreadInfo</code>'s
	 *         associated thread is currently not blocked or waiting, or there
	 *         is no other thread holding the monitor, returns a <code>-1</code>.
	 */
	public long getLockOwnerId() {
		return this.lockOwnerId;
	}

	/**
	 * If the thread represented by this <code>ThreadInfo</code> is currently
	 * blocked on or waiting on a monitor object, returns the name of the thread
	 * which owns the monitor.
	 * 
	 * @return the name of the other thread which holds the monitor that the
	 *         thread associated with this <code>ThreadInfo</code> is blocked
	 *         or waiting on. If this <code>ThreadInfo</code>'s associated
	 *         thread is currently not blocked or waiting, or there is no other
	 *         thread holding the monitor, returns a <code>null</code>
	 *         reference.
	 */
	public String getLockOwnerName() {
		return lockOwnerName;
	}

	/**
	 * If the thread corresponding to this <code>ThreadInfo</code> is blocked
	 * then this method returns a {@link LockInfo} object that contains details
	 * of the associated lock object.
	 * 
	 * @return a <code>LockInfo</code> object if this <code>ThreadInfo</code>'s
	 *         thread is currently blocked, else <code>null</code>.
	 */
	public LockInfo getLockInfo() {
		return this.lockInfo;
	}

	/**
	 * Returns the native thread identifier of the thread represented by this
	 * <code>ThreadInfo</code>.
	 * 
	 * @return the native identifier of the thread corresponding to this
	 *         <code>ThreadInfo</code>.
	 */
	long getNativeThreadId() {
		return this.nativeTId;
	}

	/**
	 * If available, returns the stack trace for the thread represented by this
	 * <code>ThreadInfo</code> instance. The stack trace is returned in an
	 * array of {@link StackTraceElement} objects with the &quot;top&quot; of the
	 * stack encapsulated in the first array element and the &quot;bottom&quot;
	 * of the stack in the last array element.
	 * <p>
	 * If this <code>ThreadInfo</code> was created without any stack trace
	 * information (e.g. by a call to {@link ThreadMXBean#getThreadInfo(long)})
	 * then the returned array will have a length of zero.
	 * </p>
	 * 
	 * @return the stack trace for the thread represented by this
	 *         <code>ThreadInfo</code>.
	 */
	public StackTraceElement[] getStackTrace() {
		return this.stackTraces.clone();
	}

	/**
	 * Returns the thread identifier of the thread represented by this
	 * <code>ThreadInfo</code>.
	 * 
	 * @return the identifier of the thread corresponding to this
	 *         <code>ThreadInfo</code>.
	 */
	public long getThreadId() {
		return this.threadId;
	}

	/**
	 * Returns the name of the thread represented by this
	 * <code>ThreadInfo</code>.
	 * 
	 * @return the name of the thread corresponding to this
	 *         <code>ThreadInfo</code>.
	 */
	public String getThreadName() {
		return this.threadName;
	}

	/**
	 * Returns the thread state value of the thread represented by this
	 * <code>ThreadInfo</code>.
	 * 
	 * @return the thread state of the thread corresponding to this
	 *         <code>ThreadInfo</code>.
	 * @see Thread#getState()
	 */
	public Thread.State getThreadState() {
		return this.threadState;
	}

	/**
	 * The number of times that the thread represented by this
	 * <code>ThreadInfo</code> has gone to the &quot;wait&quot; or &quot;timed
	 * wait&quot; state.
	 * 
	 * @return the number of times the corresponding thread has been in the
	 *         &quot;wait&quot; or &quot;timed wait&quot; state.
	 */
	public long getWaitedCount() {
		return this.waitedCount;
	}

	/**
	 * If thread contention monitoring is supported and enabled, returns the
	 * total amount of time that the thread represented by this
	 * <code>ThreadInfo</code> has spent waiting for notifications. The time
	 * is measured in milliseconds and will be measured over the time period
	 * since thread contention was most recently enabled.
	 * 
	 * @return if thread contention monitoring is currently enabled, the number
	 *         of milliseconds that the thread associated with this
	 *         <code>ThreadInfo</code> has spent waiting notifications. If
	 *         thread contention monitoring is supported but currently disabled,
	 *         <code>-1</code>.
	 * @throws UnsupportedOperationException
	 *             if the virtual machine does not support thread contention
	 *             monitoring.
	 * @see ThreadMXBean#isThreadContentionMonitoringSupported()
	 * @see ThreadMXBean#isThreadContentionMonitoringEnabled()
	 */
	public long getWaitedTime() {
		return this.waitedTime;
	}

	/**
	 * Returns a <code>boolean</code> indication of whether or not the thread
	 * represented by this <code>ThreadInfo</code> is currently in a native
	 * method.
	 * 
	 * @return if the corresponding thread <i>is </i> executing a native method
	 *         then <code>true</code>, otherwise <code>false</code>.
	 */
	public boolean isInNative() {
		return this.inNative;
	}

	/**
	 * Returns a <code>boolean</code> indication of whether or not the thread
	 * represented by this <code>ThreadInfo</code> is currently suspended.
	 * 
	 * @return if the corresponding thread <i>is </i> suspended then
	 *         <code>true</code>, otherwise <code>false</code>.
	 */
	public boolean isSuspended() {
		return this.suspended;
	}

	/**
	 * Returns an array of <code>MonitorInfo</code> objects, one for every
	 * monitor object locked by the <code>Thread</code> corresponding to this
	 * <code>ThreadInfo</code> when it was instantiated.
	 * 
	 * @return an array whose elements comprise of <code>MonitorInfo</code>
	 *         objects - one for each object monitor locked by this
	 *         <code>ThreadInfo</code> object's corresponding thread. If no
	 *         monitors are locked by the thread then the array will have a
	 *         length of zero.
	 */
	public MonitorInfo[] getLockedMonitors() {
		return this.lockedMonitors;
	}

	/**
	 * Returns an array of <code>LockInfo</code> objects, each one containing
	 * information on an ownable synchronizer (a synchronizer that makes use of
	 * the <code>AbstractOwnableSynchronizer</code> type and which is
	 * completely owned by a single thread) locked by the <code>Thread</code>
	 * corresponding to this <code>ThreadInfo</code> when it was instantiated.
	 * 
	 * @return an array whose elements comprise of <code>LockInfo</code>
	 *         objects - one for each ownable synchronizer locked by this
	 *         <code>ThreadInfo</code> object's corresponding thread. If no
	 *         ownable synchronizer are locked by the thread then the array will
	 *         have a length of zero.
	 */
	public LockInfo[] getLockedSynchronizers() {
		return this.lockedSynchronizers;
	}

	/**
	 * Receives a {@link CompositeData} representing a <code>ThreadInfo</code>
	 * object and attempts to return the root <code>ThreadInfo</code>
	 * instance.
	 * 
	 * @param cd
	 *            a <code>CompositeData</code> that represents a
	 *            <code>ThreadInfo</code>.
	 * @return if <code>cd</code> is non- <code>null</code>, returns a new
	 *         instance of <code>ThreadInfo</code>. If <code>cd</code> is
	 *         <code>null</code>, returns <code>null</code>.
	 * @throws IllegalArgumentException
	 *             if argument <code>cd</code> does not correspond to a
	 *             <code>ThreadInfo</code> with the following attributes:
	 *             <ul>
	 *             <li><code>threadId</code>(<code>java.lang.Long</code>)
	 *             <li><code>threadName</code>(
	 *             <code>java.lang.String</code>)
	 *             <li><code>threadState</code>(
	 *             <code>java.lang.String</code>)
	 *             <li><code>suspended</code>(
	 *             <code>java.lang.Boolean</code>)
	 *             <li><code>inNative</code>(<code>java.lang.Boolean</code>)
	 *             <li><code>blockedCount</code>(
	 *             <code>java.lang.Long</code>)
	 *             <li><code>blockedTime</code>(<code>java.lang.Long</code>)
	 *             <li><code>waitedCount</code>(<code>java.lang.Long</code>)
	 *             <li><code>waitedTime</code> (<code>java.lang.Long</code>)
/*[IF Sidecar19-SE]
	 *             <li><code>daemon</code> (<code>java.lang.Boolean</code>)
	 *             <li><code>priority</code> (<code>java.lang.Integer</code>)
/*[ENDIF]
	 *             <li><code>lockInfo</code> (<code>javax.management.openmbean.CompositeData</code>)
	 *             which holds the simple attributes <code>className</code>(<code>java.lang.String</code>),
	 *             <code>identityHashCode</code>(<code>java.lang.Integer</code>).  
	 *             In the event that the input <code>CompositeData</code> does not hold a 
	 *             <code>lockInfo</code> attribute, the value of the <code>lockName</code>
	 *             attribute is used for setting the returned object's 
	 *             <code>LockInfo</code> state. 
	 *             <li><code>lockName</code> (<code>java.lang.String</code>)
	 *             <li><code>lockOwnerId</code> (<code>java.lang.Long</code>)
	 *             <li><code>lockOwnerName</code> (<code>java.lang.String</code>)
	 *             <li><code>stackTrace</code> (<code>javax.management.openmbean.CompositeData[]</code>)
	 *             </ul>
	 *             Each element of the <code>stackTrace</code> array must 
	 *             correspond to a <code>java.lang.StackTraceElement</code>
	 *             and have the following attributes :
	 *             <ul>
/*[IF Sidecar19-SE]             
	 *             <li><code>moduleName</code>(<code>java.lang.String</code>)
	 *             <li><code>moduleVersion</code>(<code>java.lang.String</code>)
/*[ENDIF]     
	 *             <li><code>className</code> (<code>java.lang.String</code>)
	 *             <li><code>methodName</code> (<code>java.lang.String</code>)
	 *             <li><code>fileName</code> (<code>java.lang.String</code>)
	 *             <li><code>lineNumber</code> (<code>java.lang.Integer</code>)
	 *             <li><code>nativeMethod</code> (<code>java.lang.Boolean</code>)
	 *             </ul>
	 */
	public static ThreadInfo from(CompositeData cd) {
		ThreadInfo result = null;

		if (cd != null) {
			// Is the received CompositeData of the required type to create
			// a new ThreadInfo ?
			if (!ThreadInfoUtil.getCompositeType().isValue(cd)) {
				/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
			}

			long threadIdVal;
			String threadNameVal;
			String threadStateStringVal;
			Thread.State threadStateVal;
			boolean suspendedVal;
			boolean inNativeVal;
			long blockedCountVal;
			long blockedTimeVal;
			long waitedCountVal;
			long waitedTimeVal;
			String lockNameVal;
			long lockOwnerIdVal;
			String lockOwnerNameVal;
			StackTraceElement[] stackTraceVals;
			LockInfo lockInfoVal;
			MonitorInfo[] lockedMonitorVals;
			LockInfo[] lockedSynchronizerVals;
			
			/*[IF Sidecar19-SE]*/
			boolean daemonVal;
			int priorityVal;
			/*[ENDIF]*/

			try {
				// Set the mandatory attributes - if any of these are
				// missing we need to throw a IllegalArgumentException
				threadIdVal = ((Long) cd.get("threadId")).longValue(); //$NON-NLS-1$
				threadNameVal = (String) cd.get("threadName"); //$NON-NLS-1$
				threadStateStringVal = (String) cd.get("threadState"); //$NON-NLS-1$

				// Verify that threadStateStringVal contains a string that can
				// be successfully used to create a Thread.State.
				try {
					threadStateVal = Thread.State.valueOf(threadStateStringVal);
				} catch (IllegalArgumentException e) {
					/*[MSG "K0612", "CompositeData contains an unexpected threadState value."]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0612", e)); //$NON-NLS-1$
				}

				suspendedVal = ((Boolean) cd.get("suspended")).booleanValue(); //$NON-NLS-1$
				inNativeVal = ((Boolean) cd.get("inNative")).booleanValue(); //$NON-NLS-1$
				blockedCountVal = ((Long) cd.get("blockedCount")).longValue(); //$NON-NLS-1$
				blockedTimeVal = ((Long) cd.get("blockedTime")).longValue(); //$NON-NLS-1$
				waitedCountVal = ((Long) cd.get("waitedCount")).longValue(); //$NON-NLS-1$
				waitedTimeVal = ((Long) cd.get("waitedTime")).longValue(); //$NON-NLS-1$
				lockNameVal = (String) cd.get("lockName"); //$NON-NLS-1$
				lockOwnerIdVal = ((Long) cd.get("lockOwnerId")).longValue(); //$NON-NLS-1$
				lockOwnerNameVal = (String) cd.get("lockOwnerName"); //$NON-NLS-1$
				CompositeData[] stackTraceDataVal = (CompositeData[]) cd.get("stackTrace"); //$NON-NLS-1$
				stackTraceVals = StackTraceElementUtil.fromArray(stackTraceDataVal);
				
				/*[IF Sidecar19-SE]*/
				daemonVal = ((Boolean) cd.get("daemon")).booleanValue(); //$NON-NLS-1$
				priorityVal = ((Integer) cd.get("priority")).intValue(); //$NON-NLS-1$
				/*[ENDIF]*/
			} catch (NullPointerException | InvalidKeyException e) {
				// throw an IllegalArgumentException as the CompositeData
				// object does not contain an expected key
				/*[MSG "K05E6", "CompositeData object does not contain expected key."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E6")); //$NON-NLS-1$
			}

			// Attempt to set the optional attributes - if any of these
			// are missing we need to use the recommended default values.
			lockInfoVal = recoverLockInfoAttribute(cd, lockNameVal);
			lockedMonitorVals = recoverLockedMonitors(cd);
			lockedSynchronizerVals = recoverLockedSynchronizers(cd);

			// Finally, try and create the resulting ThreadInfo
			result = new ThreadInfo(threadIdVal, threadNameVal, threadStateVal,
					suspendedVal, inNativeVal, blockedCountVal, blockedTimeVal,
					waitedCountVal, waitedTimeVal, lockNameVal, lockOwnerIdVal,
					lockOwnerNameVal, stackTraceVals, lockInfoVal,
					lockedMonitorVals, lockedSynchronizerVals
					/*[IF Sidecar19-SE]*/
					, daemonVal, priorityVal
					/*[ENDIF]*/
			);
		}

		return result;
	}

	/**
	 * Helper method to retrieve an array of <code>LockInfo</code> from the
	 * supplied <code>CompositeData</code> object.
	 * 
	 * @param cd
	 *            a <code>CompositeData</code> that is expected to map to a
	 *            <code>ThreadInfo</code> object
	 * @return an array of <code>LockInfo</code>
	 */
	private static LockInfo[] recoverLockedSynchronizers(CompositeData cd) {
		LockInfo[] result;
		try {
			CompositeData[] lockedSynchronizersDataVal = (CompositeData[]) cd.get("lockedSynchronizers"); //$NON-NLS-1$
			result = LockInfoUtil.fromArray(lockedSynchronizersDataVal);
		} catch (InvalidKeyException e) {
			// create the recommended default for lockedSynchronizers,
			// an empty array of LockInfo
			result = new LockInfo[0];
		}
		return result;
	}

	/**
	 * Helper method to retrieve an array of <code>MonitorInfo</code> from the
	 * supplied <code>CompositeData</code> object.
	 * 
	 * @param cd
	 *            a <code>CompositeData</code> that is expected to map to a
	 *            <code>ThreadInfo</code> object
	 * @return an array of <code>MonitorInfo</code>
	 */
	private static MonitorInfo[] recoverLockedMonitors(CompositeData cd) {
		MonitorInfo[] result;
		try {
			CompositeData[] lockedMonitorsDataVal = (CompositeData[]) cd.get("lockedMonitors"); //$NON-NLS-1$
			result = MonitorInfoUtil.fromArray(lockedMonitorsDataVal);
		} catch (InvalidKeyException e) {
			// create the recommended default for lockedMonitors,
			// an empty array of MonitorInfo
			result = new MonitorInfo[0];
		}
		return result;
	}

	/**
	 * Helper method that retrieves a <code>LockInfo</code> value from the
	 * supplied <code>CompositeData</code> that is expected to map to a
	 * <code>ThreadInfo</code> instance.
	 * 
	 * @param cd
	 *            a <code>CompositeData</code> that maps to a
	 *            <code>ThreadInfo</code>
	 * @param lockNameVal
	 *            the name of a lock in the format specified by the
	 *            {@link #getLockName()} method. If the input <code>cd</code>
	 *            does not contain a &quot;lockInfo&quot; value this parameter
	 *            will be used to construct the return value
	 * @return a new <code>LockInfo</code> either constructed using data
	 *         recovered from the <code>cd</code> input or, if that fails,
	 *         from the <code>lockNameVal</code> parameter
	 */
	private static LockInfo recoverLockInfoAttribute(CompositeData cd, String lockNameVal) {
		LockInfo result = null;
		try {
			CompositeData lockInfoCDVal = cd.get("lockInfo") != null //$NON-NLS-1$
					? (CompositeData) cd.get("lockInfo") //$NON-NLS-1$
					: null;
			if (lockInfoCDVal != null) {
				// Is the received CompositeData of the required type to create
				// a new ThreadInfo ?
				if (!LockInfoUtil.getCompositeType().isValue(lockInfoCDVal)) {
					/*[MSG "K05E5", "CompositeData is not of the expected type."]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E5")); //$NON-NLS-1$
				}
				result = new LockInfo(
						((String) lockInfoCDVal.get("className")), //$NON-NLS-1$
						((Integer) lockInfoCDVal.get("identityHashCode")).intValue()); //$NON-NLS-1$
			}
		} catch (InvalidKeyException e) {
			// Create a default LockInfo using the information from
			// the lockName attribute which should have been recovered
			// above
			result = createLockInfoFromLockName(lockNameVal);
		}
		return result;
	}

	/*[IF Sidecar19-SE]*/
	
	/**
	 * Returns a <code>boolean</code> indication of whether or not the thread
	 * represented by this <code>ThreadInfo</code> is currently a daemon thread.
	 * 
	 * @return <code>true</code> if this thread is a daemon thread, otherwise <code>false</code> .
	 */
	public boolean isDaemon() {
		return daemon;
	}
	
	/**
	 * Returns the thread priority of the thread represented by this <code>ThreadInfo</code>.
	 * 
	 * @return The priority of the thread represented by this <code>ThreadInfo</code>.
	 */
	public int getPriority() {
		return priority;
	}
	
	/*[ENDIF]*/
	
	/**
	 * Returns a text description of this thread info.
	 * 
	 * @return a text description of this thread info.
	 */
	@Override
	public String toString() {
		// Since ThreadInfos are immutable the string value need only be
		// calculated the one time
		final String ls = System.lineSeparator();
		if (TOSTRING_VALUE == null) {
			StringBuilder result = new StringBuilder();
/*[IF Java11]*/
			result.append(String.format("\"%s\" prio=%d Id=%d %s",  //$NON-NLS-1$
					threadName, Integer.valueOf(priority), Long.valueOf(threadId), threadState));
/*[ELSE]*/
			result.append(String.format("\"%s\" Id=%d %s",  //$NON-NLS-1$
					threadName, Long.valueOf(threadId), threadState));
/*[ENDIF]*/
			if (State.BLOCKED == threadState) {
				result.append(String.format(" on %s owned by \"%s\" Id=%d",  //$NON-NLS-1$
						lockName, lockOwnerName, Long.valueOf(lockOwnerId)));
			}
			result.append(ls);
			if (stackTraces != null && stackTraces.length > 0) {
				MonitorInfo[] lockList = getLockedMonitors();
				MonitorInfo[] lockArray = new MonitorInfo[stackTraces.length];
				for (MonitorInfo mi: lockList) {
					int lockDepth = mi.getLockedStackDepth();
					lockArray[lockDepth] = mi;
				}
				int stackDepth = 0;
				for (StackTraceElement element : stackTraces) {
					result.append("\tat "); //$NON-NLS-1$
					Util.printStackTraceElement(element,
							null, result, true);
					result.append(ls);
					if (null != lockArray[stackDepth]) {
						MonitorInfo mi = lockArray[stackDepth];
						result.append(String.format("\t- locked %s%n",  //$NON-NLS-1$
								mi.toString()));
					}
					++stackDepth;
				}
			}
			TOSTRING_VALUE = result.toString();
		}
		return TOSTRING_VALUE;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		if (obj == null) {
			return false;
		}

		if (!(obj instanceof ThreadInfo)) {
			return false;
		}

		if (this == obj) {
			return true;
		}

		// Safe to cast if the instanceof test above was passed.
		ThreadInfo ti = (ThreadInfo) obj;
		if (!(ti.getBlockedCount() == this.getBlockedCount())) {
			return false;
		}

		if (!(ti.getBlockedTime() == this.getBlockedTime())) {
			return false;
		}

		// It is possible that the lock name is null
		if ((ti.getLockName() != null) && (this.getLockName() != null)) {
			// Both non-null values. Can string compare...
			if (!(ti.getLockName().equals(this.getLockName()))) {
				return false;
			}
		} else {
			// One or both are null. Only succeed if both are null
			if (ti.getLockName() != this.getLockName()) {
				return false;
			}
		}

		if (!(ti.getLockOwnerId() == this.getLockOwnerId())) {
			return false;
		}

		// It is possible that the lock owner name is null
		if ((ti.getLockOwnerName() != null)
				&& (this.getLockOwnerName() != null)) {
			// Both non-null values. Can string compare...
			if (!(ti.getLockOwnerName().equals(this.getLockOwnerName()))) {
				return false;
			}
		} else {
			// One or both are null. Only succeed if both are null
			if (ti.getLockOwnerName() != this.getLockOwnerName()) {
				return false;
			}
		}

		// It is possible that the lock info is null
		if ((ti.getLockInfo() != null) && (this.getLockInfo() != null)) {
			// Both non-null values. Can compare string representations...
			if (!(ti.getLockInfo().toString().equals(this.getLockInfo()
					.toString()))) {
				return false;
			}
		} else {
			// One or both are null. Only succeed if both are null
			if (ti.getLockInfo() != this.getLockInfo()) {
				return false;
			}
		}

		if (!(Arrays.equals(ti.getStackTrace(), this.getStackTrace()))) {
			return false;
		}

		if (!(ti.getThreadId() == this.getThreadId())) {
			return false;
		}

		if (!(ti.getThreadName().equals(this.getThreadName()))) {
			return false;
		}

		if (!(ti.getThreadState().equals(this.getThreadState()))) {
			return false;
		}

		if (!(ti.getWaitedCount() == this.getWaitedCount())) {
			return false;
		}

		if (!(ti.getWaitedTime() == this.getWaitedTime())) {
			return false;
		}

		if (!(ti.isInNative() == this.isInNative())) {
			return false;
		}

		if (!(ti.isSuspended() == this.isSuspended())) {
			return false;
		}

		/*[IF Sidecar19-SE]*/
		if (!(ti.isDaemon() == this.isDaemon())) {
			return false;
		}

		if (!(ti.getPriority() == this.getPriority())) {
			return false;
		}
		/*[ENDIF]*/
		
		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		return (Long.toString(this.getBlockedCount())
				+ this.getBlockedTime()
				+ getLockName()
				+ this.getLockOwnerId()
				+ getLockOwnerName()
				+ this.getStackTrace().length
				+ this.getThreadId()
				+ getThreadName()
				+ getThreadState()
				+ this.getWaitedCount()
				+ this.getWaitedTime()
				+ this.isInNative()
				+ this.isSuspended()
				/*[IF Sidecar19-SE]*/
				+ this.isDaemon()
				+ this.getPriority()
				/*[ENDIF]*/
		).hashCode();
	}

	/**
	 * Convenience method that attempts to create a new <code>LockInfo</code>
	 * from the supplied string which is expected to be a valid representation
	 * of a lock as described in {@link LockInfo#toString()}.
	 * 
	 * @param lockString
	 *            string value representing a lock
	 * @return if possible, a new <code>LockInfo</code> object initialized
	 *         from the data supplied in <code>lockString</code>. If
	 *         <code>lockString</code> is <code>null</code> or else is not
	 *         in the expected format then this method will return
	 *         <code>null</code>.
	 */
	private static LockInfo createLockInfoFromLockName(String lockString) {
		LockInfo result = null;

		if (lockString != null && lockString.length() > 0) {
			// Expected format is :
			// <f.q. class name of lock>@<lock identity hash code as hex string>
			StringTokenizer strTok = new StringTokenizer(lockString, "@"); //$NON-NLS-1$
			if (strTok.countTokens() == 2) {
				try {
					// TODO : can the recovered values be sanity checked?
					result = new LockInfo(strTok.nextToken(), Integer.parseInt(strTok.nextToken(), 16));
				} catch (NumberFormatException e) {
					// ignore and move on - the lockString is not in the correct format
				}
			}
		}

		return result;
	}

}
