/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.java.lang.management.internal;

import java.util.concurrent.atomic.AtomicReference;
import java.util.function.UnaryOperator;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;

/**
 * A class that lazily delegates to NotificationBroadcasterSupport.
 */
class LazyDelegatingNotifier implements NotificationEmitter {

	private static final UnaryOperator<NotificationBroadcasterSupport> creator() {
		return new UnaryOperator<NotificationBroadcasterSupport>() {
			@Override
			public NotificationBroadcasterSupport apply(NotificationBroadcasterSupport support) {
				return (support != null) ? support : new NotificationBroadcasterSupport();
			}
		};
	}

	private final AtomicReference<NotificationBroadcasterSupport> reference;

	LazyDelegatingNotifier() {
		super();
		reference = new AtomicReference<>();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
			throws IllegalArgumentException {
		createAndGet().addNotificationListener(listener, filter, handback);
	}

	private NotificationBroadcasterSupport createAndGet() {
		NotificationBroadcasterSupport notifier = reference.get();

		return (notifier != null) ? notifier : reference.updateAndGet(creator());
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MBeanNotificationInfo[] getNotificationInfo() {
		return createAndGet().getNotificationInfo();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException {
		createAndGet().removeNotificationListener(listener);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final void removeNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
			throws ListenerNotFoundException {
		createAndGet().removeNotificationListener(listener, filter, handback);
	}

	/**
	 * Send notifications to registered listeners.
	 *
	 * @param notification a notification to be sent
	 */
	public final void sendNotification(Notification notification) {
		NotificationBroadcasterSupport notifier = reference.get();

		if (notifier != null) {
			notifier.sendNotification(notification);
		}
	}

}
