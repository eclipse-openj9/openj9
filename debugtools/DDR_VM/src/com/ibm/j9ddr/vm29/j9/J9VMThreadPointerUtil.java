/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_BLOCKED;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_DEAD;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_INTERRUPTED;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_PARKED;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_SLEEPING;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_SUSPENDED;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_TIMER_SET;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.J9THREAD_FLAG_WAITING;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_THREAD_BLOCKED;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_THREAD_PARKED;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_THREAD_SLEEPING;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_THREAD_TIMED;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_PUBLIC_FLAGS_THREAD_WAITING;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_BLOCKED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_DEAD;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_INTERRUPTED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_PARKED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_PARKED_TIMED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_RUNNING;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_SLEEPING;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_SUSPENDED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_UNKNOWN;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_WAITING;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_WAITING_TIMED;

import java.util.HashMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9VMThreadPointerUtil {
	
	// Constants from com.ibm.dtfj.java.JavaThread version 1.3
	// Copied here because we should not require the DTFJ interface.
	// Mapping should NOT be done at DTFJ layer because J9State constants
	// could be different for different J9Versions.  Likewise DTFJ constants
	// could (but probably are not) different for different DTFJ versions
	
	/** The thread is alive	 */
	private static int STATE_ALIVE						= 0x00000001;
	/** The thread has terminated */
	private static int STATE_TERMINATED				= 0x00000002;
	/** The thread can be run although may not be actually running */
	private static int STATE_RUNNABLE					= 0x00000004;
	/** The thread is waiting on a monitor with no timeout value set */
	private static int STATE_WAITING_INDEFINITELY		= 0x00000010;
	/** The thread is waiting on a monitor but will timeout at some point */
	private static int STATE_WAITING_WITH_TIMEOUT		= 0x00000020;
	/** The thread is in the Thread.sleep method	 */
	private static int STATE_SLEEPING					= 0x00000040;
	/** The thread is in a waiting state in native code	 */
	private static int STATE_WAITING					= 0x00000080;
	/** The thread is in Object.wait */
	private static int STATE_IN_OBJECT_WAIT			= 0x00000100;
	/** The thread has been deliberately removed from scheduling */
	private static int STATE_PARKED					= 0x00000200;
	/** The thread is waiting to enter an object monitor	 */
	private static int STATE_BLOCKED_ON_MONITOR_ENTER	= 0x00000400;
	/** The thread has been suspended by Thread.suspend */
	private static int STATE_SUSPENDED					= 0x00100000;
	/** The thread has a pending interrupt */
	@SuppressWarnings("unused")
	private static int STATE_INTERRUPTED				= 0x00200000;
	/** The thread is in native code */
	@SuppressWarnings("unused")
	private static int STATE_IN_NATIVE					= 0x00400000;
	/** The thread is in a vendor specific state */
	@SuppressWarnings("unused")
	private static int STATE_VENDOR_1					= 0x10000000;
	/** The thread is in a vendor specific state */
	@SuppressWarnings("unused")
	private static int STATE_VENDOR_2					= 0x20000000;
	/** The thread is in a vendor specific state */
	private static int STATE_VENDOR_3					= 0x40000000;
	
	private static final int J9THREAD_MONITOR_OBJECT = 0x60000;  
	
	private static final HashMap<Long, Integer>threadStateMap = new HashMap<Long, Integer>(9); 
	
	static {
		threadStateMap.put(J9VMTHREAD_STATE_DEAD, STATE_TERMINATED);
		threadStateMap.put(J9VMTHREAD_STATE_SUSPENDED ,STATE_ALIVE | STATE_SUSPENDED);
		threadStateMap.put(J9VMTHREAD_STATE_RUNNING, STATE_ALIVE | STATE_RUNNABLE);
		threadStateMap.put(J9VMTHREAD_STATE_BLOCKED, STATE_ALIVE | STATE_BLOCKED_ON_MONITOR_ENTER);
		threadStateMap.put(J9VMTHREAD_STATE_WAITING, STATE_ALIVE | STATE_WAITING | STATE_WAITING_INDEFINITELY | STATE_IN_OBJECT_WAIT);
		threadStateMap.put(J9VMTHREAD_STATE_WAITING_TIMED, STATE_ALIVE | STATE_WAITING | STATE_WAITING_WITH_TIMEOUT | STATE_IN_OBJECT_WAIT);
		threadStateMap.put(J9VMTHREAD_STATE_SLEEPING, STATE_ALIVE | STATE_WAITING | STATE_SLEEPING);
		threadStateMap.put(J9VMTHREAD_STATE_PARKED, STATE_ALIVE | STATE_WAITING | STATE_WAITING_INDEFINITELY | STATE_PARKED);
		threadStateMap.put(J9VMTHREAD_STATE_PARKED_TIMED, STATE_ALIVE | STATE_WAITING | STATE_WAITING_WITH_TIMEOUT | STATE_PARKED);
	};
	
	private static class ThreadState {
		
		private UDATA flags;
		private J9ThreadMonitorPointer blocker;
		private J9ThreadPointer owner;
		
		public ThreadState(J9ThreadPointer j9self) throws CorruptDataException {
			super();
			if (j9self.isNull()) {
				flags = new UDATA(0);
				blocker = J9ThreadMonitorPointer.NULL;
				owner = J9ThreadPointer.NULL;
			} else {
				flags = j9self.flags();
				blocker = j9self.monitor();
				if (blocker.isNull()) {
					owner = J9ThreadPointer.NULL;
				} else {
					owner = blocker.owner();
				}
			}
		}
	}
	
	public static int getDTFJState(J9VMThreadPointer vmThread) throws CorruptDataException {
		try {
			return threadStateMap.get(getJ9State(vmThread).state);
		} catch (NullPointerException e) {
			return STATE_VENDOR_3;
		}
	}
	
	
	// Taken from thrinfo.c getVMThreadStateHelper
	//Note that DDR has access to a wrapper for getting information about object monitors
	//this should be used as the single point of access for obtaining this information rather
	//than local copies of the functionality e.g. monitorPeekTable.
	public static ThreadInfo getJ9State(J9VMThreadPointer targetThread) throws CorruptDataException {
		final ThreadInfo thrinfo = new ThreadInfo();
		
		if (targetThread.isNull()) {
			thrinfo.state = new UDATA(J9VMTHREAD_STATE_UNKNOWN).longValue(); 
			return thrinfo;
		}
		
		thrinfo.thread = targetThread;
		long vmstate = J9VMTHREAD_STATE_RUNNING;
		UDATA publicFlags = targetThread.publicFlags();
		J9ThreadPointer j9self = targetThread.osThread();
		
		/* j9self may be NULL if this function is used by RAS on a corrupt VM */
		ThreadState j9state = new ThreadState(j9self);
		
		if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_BLOCKED | J9_PUBLIC_FLAGS_THREAD_WAITING)) {

			/* Assert_VMUtil_true(targetThread->blockingEnterObject != NULL); */
	
			thrinfo.lockObject = targetThread.blockingEnterObject();

			//use the DDR object monitor wrapper
			ObjectMonitor monitor = ObjectMonitor.fromJ9Object(thrinfo.lockObject);
			
			// isInflated
			if(monitor.isInflated()) {

			/*
			 * If the monitor is out-of-line and NULL, then it was never entered,
			 * so the target thread is still runnable.
			 */
			
				/* count = READU(objmon->count); */
				
				if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_BLOCKED)) {
					if (monitor.getOwner().notNull() && !monitor.getOwner().equals(j9self)) {
						/* 
						 * The omrthread may be accessing other raw monitors, but
						 * the vmthread is blocked while the object monitor is
						 * owned by a competing thread.
						 */
						vmstate = J9VMTHREAD_STATE_BLOCKED;
						thrinfo.rawLock = J9ThreadMonitorPointer.cast(monitor.getInflatedMonitor());
					}
				} else {
					if (j9self.isNull()) {
						if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_TIMED)) {
							vmstate = J9VMTHREAD_STATE_WAITING_TIMED;
						} else {
							vmstate = J9VMTHREAD_STATE_WAITING;
						}
						thrinfo.rawLock = J9ThreadMonitorPointer.cast(monitor.getInflatedMonitor());
					}  else if (monitor.getInflatedMonitor().equals(j9state.blocker)) {
						vmstate = getInflatedMonitorState(j9self, j9state, thrinfo);
					}
					/* 
					 * If the omrthread is using a different monitor, it must be for vm access.
					 * So the vmthread is either not waiting yet or already awakened.
					 */
				}
			} else {
				/* 
				 * Can't wait on an uninflated object monitor, so the thread
				 * must be blocked.
				 */
				
				/* Assert_VMUtil_true(publicFlags & J9_PUBLIC_FLAGS_THREAD_BLOCKED); */
				
				if (monitor.getOwner().notNull() && (!monitor.getOwner().equals(targetThread))) {
					/* count = J9_FLATLOCK_COUNT(lockWord); */
					/* rawLock = (omrthread_monitor_t)monitorTablePeekMonitor(targetThread->javaVM, lockObject); */
					vmstate = J9VMTHREAD_STATE_BLOCKED;
				}
			}
			/* 
			 * targetThread may be blocked attempting to reacquire VM access, after 
			 * succeeding to acquire the object monitor. In this case, the returned 
			 * vmstate depends on includeRawMonitors.
			 * includeRawMonitors == FALSE: the vmstate is RUNNING.
			 * includeRawMonitors == TRUE: the vmstate depends on the state of
			 * the omrthread. e.g. The omrthread may be blocked on publicFlagsMutex.
			 */
			
		} else if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_PARKED)) {
			/* if the osthread is not parked, then the thread is runnable */
			if (j9self.isNull() || (j9state.flags.anyBitsIn(J9THREAD_FLAG_PARKED))) {
				thrinfo.lockObject = targetThread.blockingEnterObject();
				if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_TIMED)) {
					vmstate = J9VMTHREAD_STATE_PARKED_TIMED;
				} else {
					vmstate = J9VMTHREAD_STATE_PARKED;
				}
			}
				
		} else if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_THREAD_SLEEPING)) {
			/* if the osthread is not sleeping, then the thread is runnable */
			if (j9self.isNull() || (j9state.flags.anyBitsIn(J9THREAD_FLAG_SLEEPING))) {
				vmstate = J9VMTHREAD_STATE_SLEEPING;
			}

		} else { 
			/* no vmthread flags apply, so go through the omrthread flags */
			if (j9self.isNull()) {
				vmstate = J9VMTHREAD_STATE_UNKNOWN;
			} else if (j9state.flags.anyBitsIn(J9THREAD_FLAG_PARKED)) {
				if (j9state.flags.anyBitsIn(J9THREAD_FLAG_TIMER_SET)) {
					vmstate = J9VMTHREAD_STATE_PARKED_TIMED;
				} else {
					vmstate = J9VMTHREAD_STATE_PARKED;
				}
			} else if (j9state.flags.anyBitsIn(J9THREAD_FLAG_SLEEPING)) {
				vmstate = J9VMTHREAD_STATE_SLEEPING;
			} else if (j9state.flags.anyBitsIn(J9THREAD_FLAG_DEAD)) {
				vmstate = J9VMTHREAD_STATE_DEAD;
			} 
		}
		
		if (J9VMTHREAD_STATE_RUNNING == vmstate) {
			/* check if the omrthread is blocked/waiting on a raw monitor */
			thrinfo.lockObject = null;
			vmstate = getInflatedMonitorState(j9self, j9state, thrinfo);
		}

		if((thrinfo.lockObject == null) || thrinfo.lockObject.isNull()) {
			if(!((thrinfo.rawLock == null) || thrinfo.rawLock.isNull())) {
				if(thrinfo.rawLock.flags().allBitsIn(J9THREAD_MONITOR_OBJECT)) {
					thrinfo.lockObject = J9ObjectPointer.cast(thrinfo.rawLock.userData());
				}
			}
		}
		
		/*
		 * Refer to the JVMTI docs for Get Thread State.
		 * INTERRUPTED and SUSPENDED are not mutually exclusive with the other states.
		 */
		/* j9state was zeroed if j9self is NULL */
		if (j9state.flags.anyBitsIn(J9THREAD_FLAG_INTERRUPTED)) {
			vmstate |= J9VMTHREAD_STATE_INTERRUPTED;
		}
		
		if (j9state.flags.anyBitsIn(J9THREAD_FLAG_SUSPENDED)) {
			vmstate |= J9VMTHREAD_STATE_SUSPENDED;
		}
		
		if (publicFlags.anyBitsIn(J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND)) {
			vmstate |= J9VMTHREAD_STATE_SUSPENDED;
		}
		
		thrinfo.state = new UDATA(vmstate).longValue();
		return thrinfo;
	}



	
//	#define READMON_ADDR(fieldAddr) dbgReadU32((U_32 *)(fieldAddr))
	
//	#define J9OBJECT_MONITOR_EA_SAFE(vmToken, object) TMP_J9OBJECT_MONITOR_EA(object)
//	private static UDATA J9OBJECT_MONITOR_EA_SAFE(J9VMThreadPointer vmtoken, J9ObjectPointer object) throws CorruptDataException {
//		return tmpJ9ObjectMonitorEA(object);
//	}
	
//	#define TMP_J9OBJECT_MONITOR_EA(object) ((j9objectmonitor_t*)((U_8 *)object + TMP_LOCKWORD_OFFSET(object)))
//	private static UDATA tmpJ9ObjectMonitorEA(J9ObjectPointer object) throws CorruptDataException {
//		return new UDATA(new U8(object.getAddress()).add(tmpLockwordOffset(object)));
//	}

	
//	#define TMP_LOCKWORD_OFFSET(object) (dbgReadUDATA((UDATA*)((U_8*)((UDATA)(dbgReadU32((U_32*)(((U_8*)object) + offsetof(J9Object, clazz))))+ offsetof(J9Class,lockOffset)))))
//	private static UDATA tmpLockwordOffset(J9ObjectPointer object) throws CorruptDataException {
//		return object.clazz().lockOffset();
//	}

	private static long getInflatedMonitorState(J9ThreadPointer j9self, ThreadState j9state, ThreadInfo thrinfo) {
		if (j9self.isNull()) {
			return J9VMTHREAD_STATE_UNKNOWN;
		}
		
		if (j9state.flags.anyBitsIn(J9THREAD_FLAG_BLOCKED)) {
			/* Check for BLOCKED before WAITING to catch waiting threads that
			 * have been notified.
			 */
			if (j9state.owner.notNull() && (!j9state.owner.equals(j9self))) {
				thrinfo.rawLock = j9state.blocker;
				return J9VMTHREAD_STATE_BLOCKED;
			}
		} 
		
		if (j9state.flags.anyBitsIn(J9THREAD_FLAG_WAITING)) {
			/* The blocking object of a waiting thread need not be owned. */
			if (!j9state.owner.equals(j9self)) {
				thrinfo.rawLock = j9state.blocker;
				if (j9state.flags.anyBitsIn(J9THREAD_FLAG_TIMER_SET)) {
					return J9VMTHREAD_STATE_WAITING_TIMED;
				} else {
					return J9VMTHREAD_STATE_WAITING;
				}
			}
		}

		return J9VMTHREAD_STATE_RUNNING;
	}
	
	public static class ThreadInfo {
		private long state = 0;
		private J9ObjectPointer lockObject;
		private J9ThreadMonitorPointer rawLock;
		private J9VMThreadPointer lockOwner;
		private J9VMThreadPointer thread;
		
		private ThreadInfo() {
			//can only be created by parent class
		}

		public long getState() {
			return state;
		}

		public J9ObjectPointer getLockObject() {
			return lockObject;
		}

		public J9ThreadMonitorPointer getRawLock() {
			return rawLock;
		}
		
		public J9VMThreadPointer getLockOwner() {
			return lockOwner;
		}
	
		public J9VMThreadPointer getThread() {
			return thread;
		}
	}
}
