/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2016 IBM Corp. and others
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

import java.lang.management.GarbageCollectorMXBean;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.openmbean.CompositeData;

/**
 * Runtime type for {@link java.lang.management.GarbageCollectorMXBean}.
 * <p>
 * In addition to implementing the &quot;standard&quot; management interface
 * <code>java.lang.management.GarbageCollectorMXBean</code>, this class also
 * provides an implementation of most of the IBM extension interface
 * <code>com.ibm.lang.management.GarbageCollectorMXBean</code>.
 * The type does not need to be modeled as a DynamicMBean, as it is structured 
 * statically, without attributes, operations, notifications, etc., configured,
 * on the fly. The StandardMBean model is sufficient for the bean type.  
 * </p>
 * @since 1.5
 */
public class GarbageCollectorMXBeanImpl
		extends MemoryManagerMXBeanImpl
        implements GarbageCollectorMXBean, NotificationEmitter {

	/**
	 * The delegate for all notification management.
	 */
	private final NotificationBroadcasterSupport notifier;

	/**
	 * @param objectName The ObjectName of this bean
	 * @param name The name of this collector
	 * @param id An internal id number representing this collector
	 * @param memBean The memory bean that receives notification events from pools managed by this collector
	 */
	protected GarbageCollectorMXBeanImpl(ObjectName objectName, String name, int id, MemoryMXBeanImpl memBean) {
		super(objectName, name, id, memBean);
		notifier = new NotificationBroadcasterSupport();
	}

	/**
	 * @return the total number of garbage collections that have been carried
	 *         out by the associated garbage collector.
	 * @see #getCollectionCount()
	 */
	private native long getCollectionCountImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getCollectionCount() {
		return this.getCollectionCountImpl(id);
	}

	/**
	 * @return the number of milliseconds that have been spent in performing
	 *         garbage collection. This is a cumulative figure.
	 * @see #getCollectionTime()
	 */
	private native long getCollectionTimeImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getCollectionTime() {
		return this.getCollectionTimeImpl(id);
	}

	/**
	 * @return the start time of the most recent collection
	 * @see #getLastCollectionStartTime()
	 */
	private native long getLastCollectionStartTimeImpl(int id);

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getLastCollectionStartTime() {
		return this.getLastCollectionStartTimeImpl(id);
	}

	/**
	 * @return the end time of the most recent collection
	 * @see #getLastCollectionEndTime()
	 */
	private native long getLastCollectionEndTimeImpl(int id);

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getLastCollectionEndTime() {
		return this.getLastCollectionEndTimeImpl(id);
	}

	/**
	 * Returns the amount of heap memory used by objects that are managed
	 * by the collector corresponding to this bean object.
	 * 
	 * @return memory used in bytes
	 * @see #getMemoryUsed()
	 */
	private native long getMemoryUsedImpl(int id);

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getMemoryUsed() {
		return this.getMemoryUsedImpl(id);
	}

	/**
	 * Returns the cumulative total amount of memory freed, in bytes, by the
	 * garbage collector corresponding to this bean object.
	 *
	 * @return memory freed in bytes
	 * @see #getTotalMemoryFreed()
	 */
	private native long getTotalMemoryFreedImpl(int id);

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getTotalMemoryFreed() {
		return this.getTotalMemoryFreedImpl(id);
	}

	/**
	 * Returns the cumulative total number of compacts that was performed by
	 * garbage collector corresponding to this bean object.
	 *
	 * @return number of compacts performed
	 * @see #getTotalCompacts()
	 */
	private native long getTotalCompactsImpl(int id);

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getTotalCompacts() {
		return this.getTotalCompactsImpl(id);
	}

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public final long getAllocatedHeapSizeTarget() {
		return MemoryMXBeanImpl.getInstance().getMaxHeapSizeLimit();
	}

	/**
	 * To satisfy com.ibm.lang.management.GarbageCollectorMXBean.
	 */
	public void setAllocatedHeapSizeTarget(long size) {
		MemoryMXBeanImpl.getInstance().setMaxHeapSize(size);
	}

	/**
	 * TODO Was this intended to be exposed in com.ibm.lang.management.GarbageCollectorMXBean?
	 */
	public final String getStrategy() {
		return MemoryMXBeanImpl.getInstance().getGCMode();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final void removeNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
			throws ListenerNotFoundException {
		notifier.removeNotificationListener(listener, filter, handback);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
			throws IllegalArgumentException {
		notifier.addNotificationListener(listener, filter, handback);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException {
		notifier.removeNotificationListener(listener);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MBeanNotificationInfo[] getNotificationInfo() {
		// The base implementation provides no notifications.
		return new MBeanNotificationInfo[0];
	}

	/**
	 * Send notifications to registered listeners. This will be called at the end of Garbage Collections.
	 * 
	 * @param notification For this type of bean the user data will consist of
	 *     a {@link CompositeData} instance that represents
	 *     a {@link com.sun.management.GarbageCollectionNotificationInfo} object
	 */
	public final void sendNotification(Notification notification) {
		notifier.sendNotification(notification);
	}

}
