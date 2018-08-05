/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics.data;

import java.io.Serializable;
import java.lang.management.LockInfo;
import java.lang.management.MonitorInfo;
import java.lang.management.ThreadInfo;

import com.ibm.lang.management.ExtendedThreadInfo;

/**
 * Serializable {@link ThreadInfo} 
 * @see ExtendedThreadInfo
 */
public class ThreadInfoResponse implements Serializable {

	private static final long serialVersionUID = 1L;

	private long threadId;

	private long nativeTId;

	private String threadName;

	private String threadState;

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

	private LockInfoResponse lockInfo;

	private LockInfoResponse[] lockedSynchronizers = new LockInfoResponse[0];

	private MonitorInfoResponse[] lockedMonitors = new MonitorInfoResponse[0];

	/**
	 * Null-Safe constructor
	 * @param info ExtendedThreadInfo to wrap
	 * @return ThreadInfo able to serialize 
	 */
	public static ThreadInfoResponse fromThreadInfo(ExtendedThreadInfo info) {
		if(info==null || info.getThreadInfo()==null) {
			return null;
		}
		
		ThreadInfoResponse result = new ThreadInfoResponse();
		result.threadId = info.getThreadInfo().getThreadId();
		result.nativeTId = info.getNativeThreadId();
		result.threadName = info.getThreadInfo().getThreadName();
		result.threadState = info.getThreadInfo().getThreadState().toString();
		result.suspended = info.getThreadInfo().isSuspended();
		result.inNative = info.getThreadInfo().isInNative();
		result.blockedCount = info.getThreadInfo().getBlockedCount();
		result.blockedTime = info.getThreadInfo().getBlockedTime();
		result.waitedCount = info.getThreadInfo().getWaitedCount();
		result.waitedTime = info.getThreadInfo().getWaitedTime();
		result.lockName = info.getThreadInfo().getLockName();
		result.lockOwnerId = info.getThreadInfo().getLockOwnerId();
		result.lockOwnerName = info.getThreadInfo().getLockOwnerName();
		result.stackTraces = info.getThreadInfo().getStackTrace();
		result.lockInfo = LockInfoResponse.fromLockInfo(info.getThreadInfo().getLockInfo());
		
		if(info.getThreadInfo().getLockedSynchronizers()!=null) {
			result.lockedSynchronizers = new LockInfoResponse[info.getThreadInfo().getLockedSynchronizers().length];
			for(int i=0;i<result.lockedSynchronizers.length;i++) {
				result.lockedSynchronizers[i] = LockInfoResponse.fromLockInfo(info.getThreadInfo().getLockedSynchronizers()[i]);
			}
		}

		if(info.getThreadInfo().getLockedMonitors()!=null) {
			result.lockedMonitors = new MonitorInfoResponse[info.getThreadInfo().getLockedMonitors().length];
			for(int i=0;i<result.lockedMonitors.length;i++) {
				result.lockedMonitors[i] = MonitorInfoResponse.fromMonitorInfo(info.getThreadInfo().getLockedMonitors()[i]);
			}
		}
		return result;
	}

	/**
	 * @return {@link ThreadInfo#getThreadId()}
	 */
	public long getThreadId() {
		return threadId;
	}

	/**
	 * @return {@link ExtendedThreadInfo#getNativeThreadId()}
	 */
	public long getNativeTId() {
		return nativeTId;
	}

	/**
	 * @return {@link ThreadInfo#getThreadName()}
	 */
	public String getThreadName() {
		return threadName;
	}

	/**
	 * @return String presentation of {@link ThreadInfo#getThreadState()}
	 */
	public String getThreadState() {
		return threadState;
	}

	/**
	 * @return {@link ThreadInfo#isSuspended()}
	 */
	public boolean isSuspended() {
		return suspended;
	}

	/**
	 * @return {@link ThreadInfo#isInNative()}
	 */
	public boolean isInNative() {
		return inNative;
	}

	/**
	 * @return {@link ThreadInfo#getBlockedCount()}
	 */
	public long getBlockedCount() {
		return blockedCount;
	}

	/**
	 * @return {@link ThreadInfo#getBlockedTime()}
	 */
	public long getBlockedTime() {
		return blockedTime;
	}

	/**
	 * @return {@link ThreadInfo#getWaitedCount()}
	 */
	public long getWaitedCount() {
		return waitedCount;
	}

	/**
	 * @return {@link ThreadInfo#getWaitedTime()}
	 */
	public long getWaitedTime() {
		return waitedTime;
	}

	/**
	 * @return {@link ThreadInfo#getLockName()}
	 */
	public String getLockName() {
		return lockName;
	}

	/**
	 * @return {@link ThreadInfo#getLockOwnerId()}
	 */
	public long getLockOwnerId() {
		return lockOwnerId;
	}

	/**
	 * @return {@link ThreadInfo#getLockOwnerName()}
	 */
	public String getLockOwnerName() {
		return lockOwnerName;
	}

	/**
	 * @return {@link ThreadInfo#getStackTrace()}
	 */
	public StackTraceElement[] getStackTraces() {
		return stackTraces;
	}

	/**
	 * @return serializable wrapper for {@link ThreadInfo#getLockInfo()}
	 */
	public LockInfoResponse getLockInfo() {
		return lockInfo;
	}

	/**
	 * @return wrapped {@link ThreadInfo#getLockedSynchronizers()}
	 */
	public LockInfoResponse[] getLockedSynchronizers() {
		return lockedSynchronizers;
	}

	/**
	 * @return wrapped {@link ThreadInfo#getLockedMonitors()}
	 */
	public LockInfoResponse[] getLockedMonitors() {
		return lockedMonitors;
	}

	/**
	 * Serializable {@link LockInfo}
	 */
	public static class LockInfoResponse implements Serializable {
		private static final long serialVersionUID = 1L;
		private String className;
		private int identityHashCode;

		/**
		 * Null-Safe constructor
		 * @param info data to wrap
		 * @return wrapped {@link LockInfo}
		 */
		public static LockInfoResponse fromLockInfo(LockInfo info) {
			if(info==null) {
				return null;
			}
			LockInfoResponse result = new LockInfoResponse();
			result.className = info.getClassName();
			result.identityHashCode = info.getIdentityHashCode();
			return result;
		}

		/**
		 * @return @see LockInfo#getClassName()
		 */
		public String getClassName() {
			return className;
		}

		/**
		 * @return @see LockInfo#getIdentityHashCode()
		 */
		public int getIdentityHashCode() {
			return identityHashCode;
		}

		/**
		 * Sets class name @see LockInfo#getClassName()
		 * @param className name of class
		 */
		public void setClassName(String className) {
			this.className = className;
		}

		/**
		 * Sets identity hash code @see LockInfo#getIdentityHashCode()
		 * @param identityHashCode hash code
		 */
		public void setIdentityHashCode(int identityHashCode) {
			this.identityHashCode = identityHashCode;
		}
		
	}
	
	/**
	 * Serializable {@link MonitorInfo}
	 */
	public static class MonitorInfoResponse extends LockInfoResponse {
		private static final long serialVersionUID = 1L;
		private int stackDepth;
		private StackTraceElement stackFrame;

		/**
		 * Null-Safe constructor
		 * @param info data to wrap
		 * @return wrapped {@link MonitorInfo}
		 */
		public static MonitorInfoResponse fromMonitorInfo(MonitorInfo info) {
			if(info==null) {
				return null;
			}
			MonitorInfoResponse result = new MonitorInfoResponse();
			result.stackDepth = info.getLockedStackDepth();
			result.stackFrame = info.getLockedStackFrame();
			result.setClassName(info.getClassName());
			result.setIdentityHashCode(info.getIdentityHashCode());

			return result;
		}

		/**
		 * @return @see MonitorInfo#getLockedStackDepth()
		 */
		public int getLockedStackDepth() {
			return stackDepth;
		}

		/**
		 * @return @see MonitorInfo#getLockedStackFrame()
		 */
		public StackTraceElement getLockedStackFrame() {
			return stackFrame;
		}
	}
}
