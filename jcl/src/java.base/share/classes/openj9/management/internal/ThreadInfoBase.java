/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package openj9.management.internal;

import java.lang.Thread.State;
import java.util.Arrays;
import java.util.Objects;

import com.ibm.oti.util.Util;

/**
 * Container for thread information for use by classes which have access only to the
 * java.base module.
 * This is typically wrapped in a ThreadInfo object.
 */
public class ThreadInfoBase {

	private final long threadId;
	private final long nativeTId;
	private final String threadName;
	private final Thread.State threadState;
	private final boolean suspended;
	private final boolean inNative;
	private final long blockedCount;
	private final long blockedTime;
	private final long waitedCount;
	private final long waitedTime;
	private final String lockName;
	private final long lockOwnerId;
	private final String lockOwnerName;
	private StackTraceElement[] stackTrace = new StackTraceElement[0];
	private final LockInfoBase blockingLockInfo;
	private LockInfoBase[] lockedSynchronizers = new LockInfoBase[0];
	private MonitorInfoBase[] lockedMonitors = new MonitorInfoBase[0];
	private String cachedToStringResult;
/*[IF Sidecar19-SE]*/
	private final boolean daemon;
	private final int priority;
/*[ENDIF]*/

	/**
	 * used by ThreadInfo's constructor only
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
	 * @param lockInfoVal
	 * @param lockedMonitorsVal
	 * @param lockedSynchronizersVal
/*[IF Sidecar19-SE]
	 * @param daemon
	 * @param priority
/*[ENDIF]
	 */
	public ThreadInfoBase(long threadIdVal, String threadNameVal, Thread.State threadStateVal, boolean suspendedVal,
			boolean inNativeVal, long blockedCountVal, long blockedTimeVal, long waitedCountVal, long waitedTimeVal,
			String lockNameVal, long lockOwnerIdVal, String lockOwnerNameVal, StackTraceElement[] stackTraceVal,
			LockInfoBase lockInfoVal, MonitorInfoBase[] lockedMonitorsVal, LockInfoBase[] lockedSynchronizersVal
/*[IF Sidecar19-SE]*/
			, boolean daemonVal, int priorityVal
/*[ENDIF]*/
	) {
		threadId = threadIdVal;
		nativeTId = -1;
		threadName = threadNameVal;
		threadState = threadStateVal;
		suspended = suspendedVal;
		inNative = inNativeVal;
		blockedCount = blockedCountVal;
		blockedTime = blockedTimeVal;
		waitedCount = waitedCountVal;
		waitedTime = waitedTimeVal;
		lockName = lockNameVal;
		lockOwnerId = lockOwnerIdVal;
		lockOwnerName = lockOwnerNameVal;
		stackTrace = stackTraceVal;
		blockingLockInfo = lockInfoVal;
		lockedMonitors = lockedMonitorsVal;
		lockedSynchronizers = lockedSynchronizersVal;
/*[IF Sidecar19-SE]*/
		daemon = daemonVal;
		priority = priorityVal;
/*[ENDIF]*/
	}

	@SuppressWarnings("unused")
	/* used by native code */
	private ThreadInfoBase(Thread thread, long nativeId, int state, boolean isSuspended, boolean isInNative,
			long blockedCountVal, long blockedTimeVal, long waitedCountVal, long waitedTimeVal, StackTraceElement[] stackTraceVal,
			Object blockingObject, Thread blockingObjectOwner, MonitorInfoBase[] lockedMonitorsVal,
			LockInfoBase[] lockedSynchronizersVal) {
		threadId = thread.getId();
		nativeTId = nativeId;
		threadName = thread.getName();
		/*[IF Sidecar19-SE]*/
		daemon = thread.isDaemon();
		priority = thread.getPriority();
		/*[ENDIF]*/

		// Use the supplied state int to index into the array returned
		// by values() which should be all thread states in the order
		// they have been declared. Doing this rather than just issuing a
		// call to thread.getState() because the Thread state may have
		// changed after the VM issued the call to this method.
		threadState = Thread.State.values()[state];
		suspended = isSuspended;
		inNative = isInNative;
		blockedCount = blockedCountVal;
		blockedTime = blockedTimeVal;
		waitedCount = waitedCountVal;
		waitedTime = waitedTimeVal;

		if (blockingObjectOwner != null) {
			lockOwnerId = blockingObjectOwner.getId();
			lockOwnerName = blockingObjectOwner.getName();
		} else {
			lockOwnerId = -1;
			lockOwnerName = null;
		}

		stackTrace = stackTraceVal;
		lockedMonitors = lockedMonitorsVal;
		lockedSynchronizers = lockedSynchronizersVal;

		if (blockingObject != null) {
			blockingLockInfo = new LockInfoBase(blockingObject.getClass().getName(),
					System.identityHashCode(blockingObject));
			lockName = blockingLockInfo.toString();
		} else {
			blockingLockInfo = null;
			lockName = null;
		}
	}

	/**
	 * @return a text description of this thread info.
	 */
	@Override
	public String toString() {
		/* Since ThreadInfoBases are immutable the string value need be calculated only once. */
		String ls = System.lineSeparator();
		if (cachedToStringResult == null) {
			StringBuilder result = new StringBuilder();
/*[IF Java11]*/
			result.append(String.format("\"%s\" prio=%d Id=%d %s", //$NON-NLS-1$
					threadName, Integer.valueOf(priority), Long.valueOf(threadId), threadState));
/*[ELSE]*/
			result.append(String.format("\"%s\" Id=%d %s", //$NON-NLS-1$
					threadName, Long.valueOf(threadId), threadState));
/*[ENDIF]*/
			if (State.BLOCKED == threadState) {
				result.append(String.format(" on %s owned by \"%s\" Id=%d", //$NON-NLS-1$
						lockName, lockOwnerName, Long.valueOf(lockOwnerId)));
			}
			result.append(ls);
			if ((stackTrace != null) && (stackTrace.length > 0)) {
				MonitorInfoBase[] lockArray = new MonitorInfoBase[stackTrace.length];
				for (MonitorInfoBase mi : lockedMonitors) {
					lockArray[mi.getStackDepth()] = mi;
				}
				int stackDepth = 0;
				for (StackTraceElement element : stackTrace) {
					result.append("\tat "); //$NON-NLS-1$
					Util.printStackTraceElement(element, null, result, true);
					result.append(ls);
					MonitorInfoBase mi = lockArray[stackDepth];
					if (null != mi) {
						result.append(String.format("\t- locked %s%n", //$NON-NLS-1$
								mi.toString()));
					}
					stackDepth += 1;
				}
			}
			cachedToStringResult = result.toString();
		}
		return cachedToStringResult;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}

		boolean result = false;
		if (obj instanceof ThreadInfoBase) {
			// Safe to cast if the instanceof test passed.
			ThreadInfoBase ti = (ThreadInfoBase) obj;
			result =
					(ti.threadId == threadId)
					&& (ti.blockedCount == blockedCount)
					&& (ti.blockedTime == blockedTime)
					&& (ti.lockOwnerId == lockOwnerId)
					&& (ti.waitedCount == waitedCount)
					&& (ti.waitedTime == waitedTime)
					&& (ti.inNative == inNative)
					&& (ti.suspended == suspended)
					/*[IF Sidecar19-SE]*/
					&& (ti.daemon == daemon)
					&& (ti.priority == priority)
					/*[ENDIF]*/
					&& ti.threadName.equals(threadName)
					&& ti.threadState.equals(threadState)
					&& Objects.equals(ti.lockName, lockName)
					&& Objects.equals(ti.lockOwnerName , lockOwnerName)
					&& Objects.equals(ti.blockingLockInfo, blockingLockInfo)
					&& Arrays.equals(ti.stackTrace, stackTrace);
		}
		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		return (Long.toString(blockedCount) + blockedTime + lockName + lockOwnerId
				+ lockOwnerName + stackTrace.length + threadId + threadName
				+ threadState + waitedCount + waitedTime + inNative
				+ suspended
/*[IF Sidecar19-SE]*/
				+ daemon + priority
/*[ENDIF]*/
		).hashCode();
	}

	/**
	 * @return thread ID
	 */
	public long getThreadId() {
		return threadId;
	}

	/**
	 * @return native thread ID
	 */
	public long getNativeTId() {
		return nativeTId;
	}

	/**
	 * @return thread name
	 */
	public String getThreadName() {
		return threadName;
	}

	/**
	 * @return thread state
	 */
	public Thread.State getThreadState() {
		return threadState;
	}

	/**
	 * @return true if thread is suspended
	 */
	public boolean isSuspended() {
		return suspended;
	}

	/**
	 * @return true if in native code 
	 */
	public boolean isInNative() {
		return inNative;
	}

	/**
	 * @return number of times the thread has tried to enter a locked monitor
	 */
	public long getBlockedCount() {
		return blockedCount;
	}

	/**
	 * @return time spent waiting to enter a monitor, in milliseconds
	 */
	public long getBlockedTime() {
		return blockedTime;
	}

	/**
	 * @return number of times the thread has stopped for notification
	 */
	public long getWaitedCount() {
		return waitedCount;
	}

	/**
	 * @return time spent waiting for notification
	 */
	public long getWaitedTime() {
		return waitedTime;
	}

	/**
	 * @return name of the object on which the thread is blocked
	 */
	public String getLockName() {
		return lockName;
	}

	/**
	 * @return ID of thread owning the object on which the thread is blocked
	 */
	public long getLockOwnerId() {
		return lockOwnerId;
	}

	/**
	 * @return name of thread owning the object on which the thread is blocked
	 */
	public String getLockOwnerName() {
		return lockOwnerName;
	}

	/**
	 * @return stack trace for this thread
	 */
	public StackTraceElement[] getStackTrace() {
		return stackTrace;
	}

	/**
	 * @return information on the object on which the thread is blocked
	 */
	public LockInfoBase getBlockingLockInfo() {
		return blockingLockInfo;
	}

	/**
	 * @return information on ownable synchronizers locked by this thread
	 */
	public LockInfoBase[] getLockedSynchronizers() {
		return lockedSynchronizers;
	}

	/**
	 * @return information on monitors locked by this thread
	 */
	public MonitorInfoBase[] getLockedMonitors() {
		return lockedMonitors;
	}
	/*[IF Sidecar19-SE]*/

	/**
	 * @return true if this is a daemon thread
	 */
	public boolean isDaemon() {
		return daemon;
	}

	/**
	 * @return priority of this thread
	 */
	public int getPriority() {
		return priority;
	}
	/*[ENDIF]*/
}
