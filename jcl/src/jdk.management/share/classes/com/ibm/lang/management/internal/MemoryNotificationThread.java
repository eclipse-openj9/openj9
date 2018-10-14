/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryUsage;
import java.security.AccessController;
import java.security.PrivilegedAction;

import javax.management.Notification;

import com.ibm.java.lang.management.internal.GarbageCollectorMXBeanImpl;
import com.ibm.java.lang.management.internal.MemoryNotificationInfoUtil;
import com.sun.management.GarbageCollectionNotificationInfo;
import com.sun.management.GcInfo;
import com.sun.management.internal.GarbageCollectionNotificationInfoUtil;

/**
 * A thread that monitors and dispatches memory usage notifications from
 * an internal queue.
 *
 * @since 1.5
 */
final class MemoryNotificationThread extends Thread {

	private final ExtendedMemoryMXBeanImpl memBean;

	/**
	 * Basic constructor
	 * @param mem The memory bean to send notifications through
	 * @param gcs The garbage collector beans to send notifications through
	 */
	MemoryNotificationThread(ExtendedMemoryMXBeanImpl mem) {
		super();
		memBean = mem;
	}

	/**
     * A helper used by processNotificationLoop to construct and dispatch
     * garbage collection notification objects
     *
     * @param gcName
     *            the name of garbage collector which we are sending notifications on behalf of
     * @param gcAction
     *            the action of the performed by the garbage collector
     * @param gcCause
     *            the cause the garbage collection
     * @param index
     *            the identifier of this garbage collection which is the number of collections that this collector has done
     * @param startTime
     *            the start time of this GC in milliseconds since the Java virtual machine was started
     * @param endTime
     *            the end time of this GC in milliseconds since the Java virtual machine was started
     * @param initialSize
     *            the initial amount of memory of all memory pools
     * @param preUsed
     *            the amounts of memory used of all memory pools before the garbage collection
     * @param preCommitted
     *            the amounts of all memory pools that is guaranteed to be available for use before the garbage collection
     * @param preMax
     *            the maximum amounts of memory pools that can be used before the garbage collection
     * @param postUsed
     *            the amounts of memory used of all memory pools after the garbage collection
     * @param postCommitted
     *            the amounts of all memory pools that is guaranteed to be available for use after the garbage collection
     * @param postMax
     *            the maximum amounts of memory pools that can be used after the garbage collection
     * @param sequenceNumber
     *            the sequence identifier of the current notification
     */
	private void dispatchGCNotificationHelper(String gcName, String gcAction, String gcCause, long index,
			long startTime, long endTime, long[] initialSize, long[] preUsed,
			long[] preCommitted, long[] preMax, long[] postUsed, long[] postCommitted, long[] postMax,
			long sequenceNumber) {
		GcInfo gcInfo = ExtendedGarbageCollectorMXBeanImpl.buildGcInfo(index, startTime, endTime, initialSize, preUsed, preCommitted, preMax, postUsed, postCommitted, postMax);
		GarbageCollectionNotificationInfo info = new GarbageCollectionNotificationInfo(gcName, gcAction, gcCause, gcInfo);

		for (MemoryManagerMXBean bean : memBean.getMemoryManagerMXBeans(false)) {
			if (bean instanceof GarbageCollectorMXBeanImpl && bean.getName().equals(gcName)) {
				Notification notification = new Notification(
						GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION,
						"java.lang:type=GarbageCollector", //$NON-NLS-1$
						sequenceNumber);
				notification.setUserData(GarbageCollectionNotificationInfoUtil.toCompositeData(info));
				((GarbageCollectorMXBeanImpl) bean).sendNotification(notification);
				break;
			}
		}
    }

	/**
     * A helper used by processNotificationLoop to construct and dispatch
     * memory threshold notification objects
     *
     * @param poolName
     *            the name of pool which we are sending notifications on behalf of
     * @param min
     *            the initial amount in bytes of memory that can be allocated by
     *            this virtual machine
     * @param used
     *            the number of bytes currently used for memory
     * @param committed
     *            the number of bytes of committed memory
     * @param max
     *            the maximum number of bytes that can be used for memory
     *            management purposes
     * @param count
     *            the number of times that the memory usage of the memory pool
     *            in question has met or exceeded the relevant threshold
     * @param sequenceNumber
     *            the sequence identifier of the current notification
     * @param isCollectionUsageNotification
     *            a <code>boolean</code> indication of whether or not the new
     *            notification is as a result of the collection threshold being
     *            exceeded. If this value is <code>false</code> then the
     *            implication is that a memory threshold has been exceeded.
     */
	private void dispatchMemoryNotificationHelper(String poolName, long min, long used, long committed, long max,
			long count, long sequenceNumber, boolean isCollectionUsageNotification) {
		MemoryNotificationInfo info = new MemoryNotificationInfo(poolName, new MemoryUsage(min, used, committed, max), count);
        Notification notification = new Notification(
                isCollectionUsageNotification
		                ? MemoryNotificationInfo.MEMORY_COLLECTION_THRESHOLD_EXCEEDED
		                : MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED,
                "java.lang:type=Memory", //$NON-NLS-1$
                sequenceNumber);
		notification.setUserData(MemoryNotificationInfoUtil.toCompositeData(info));
		memBean.sendNotification(notification);
	}

	/**
	 * Process notifications on an internal VM queue until a shutdown
	 * request is received.
	 */
	private native void processNotificationLoop();

	private boolean registerShutdownHandler() {
		Thread notifier = new MemoryNotificationThreadShutdown(this);
		PrivilegedAction<Boolean> action = () -> {
			try {
				Runtime.getRuntime().addShutdownHook(notifier);
				return Boolean.TRUE;
			} catch (IllegalStateException e) {
				/*
				 * This exception will occur if the VM is already shutting down:
				 * by returning false we avoid starting the notification loop
				 * that cannot be signalled to terminate.
				 */
				return Boolean.FALSE;
			}
		};

		return AccessController.doPrivileged(action).booleanValue();
	}

	/**
	 * If we're able to register a shutdown handler that will signal this thread
	 * to terminate, then enter the native that services an internal notification queue.
	 */
	@Override
	public void run() {
		if (registerShutdownHandler()) {
			processNotificationLoop();
		}
	}

}
