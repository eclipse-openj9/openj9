/*[INCLUDE-IF Sidecar17]*/
/*
 * Copyright IBM Corp. and others 2016
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

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryType;
import java.security.PrivilegedAction;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.management.NotificationFilter;
import javax.management.NotificationListener;

import com.ibm.java.lang.management.internal.MemoryMXBeanImpl;
import com.ibm.lang.management.MemoryMXBean;
import com.ibm.oti.vm.VM;

/**
 * Runtime type for {@link com.ibm.lang.management.MemoryMXBean}.
 */
/*[IF JAVA_SPEC_VERSION >= 11]*/
@SuppressWarnings("removal")
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
public final class ExtendedMemoryMXBeanImpl extends MemoryMXBeanImpl implements MemoryMXBean {

	private static final ExtendedMemoryMXBeanImpl instance = new ExtendedMemoryMXBeanImpl();

	private static final ExtendedOperatingSystemMXBeanImpl osinstance = ExtendedOperatingSystemMXBeanImpl.getInstance();

	public static ExtendedMemoryMXBeanImpl getInstance() {
		return instance;
	}

	/*
	 * This remembers whether the notification thread has been started:
	 * There's no point in sending notifications before we have any listeners.
	 */
	private final AtomicBoolean notificationThreadStarted;

	private ExtendedMemoryMXBeanImpl() {
		super();
		notificationThreadStarted = new AtomicBoolean();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
			throws IllegalArgumentException {
		// Start the notification thread when we have our first listener.
		startNotificationThread();
		super.addNotificationListener(listener, filter, handback);
	}

	/**
	 * Ensure the notification thread is running.
	 */
	@Override
	protected void startNotificationThread() {
		if (!notificationThreadStarted.getAndSet(true)) {
			PrivilegedAction<Thread> createThread = () -> {
				Thread thread = VM.getVMLangAccess().createThread(new MemoryNotificationThread(this),
					"MemoryMXBean notification dispatcher", true, false, true, ClassLoader.getSystemClassLoader()); //$NON-NLS-1$
				thread.setPriority(Thread.NORM_PRIORITY + 1);
				return thread;
			};

			Thread notifier = java.security.AccessController.doPrivileged(createThread);
			notifier.start();
		}
	}

/*[IF JAVA_SPEC_VERSION < 19]*/
	/**
/*[IF JAVA_SPEC_VERSION >= 14]
	 * Deprecated. Use com.sun.management.OperatingSystemMXBean.getTotalMemorySize().
/*[ELSE] JAVA_SPEC_VERSION >= 14
	 * Deprecated. Use com.sun.management.OperatingSystemMXBean.getTotalPhysicalMemorySize().
/*[ENDIF] JAVA_SPEC_VERSION >= 14
	 */
	/*[IF JAVA_SPEC_VERSION > 8]*/
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE] JAVA_SPEC_VERSION > 8 */
	@Deprecated
	/*[ENDIF] JAVA_SPEC_VERSION > 8 */
	public long getTotalPhysicalMemory() {
		/*[IF JAVA_SPEC_VERSION >= 14]*/
		return osinstance.getTotalMemorySize();
		/*[ELSE] JAVA_SPEC_VERSION >= 14 */
		return osinstance.getTotalPhysicalMemorySize();
		/*[ENDIF] JAVA_SPEC_VERSION >= 14 */
	}
/*[ENDIF] JAVA_SPEC_VERSION < 19 */

/*[IF JAVA_SPEC_VERSION < 20]*/
	/*[IF JAVA_SPEC_VERSION > 8]*/
	@Deprecated(forRemoval = true, since = "19")
	/*[ENDIF] JAVA_SPEC_VERSION > 8 */
	public long getTotalPhysicalMemorySize() {
		/*[IF JAVA_SPEC_VERSION >= 14]*/
		return osinstance.getTotalMemorySize();
		/*[ELSE] JAVA_SPEC_VERSION >= 14 */
		return osinstance.getTotalPhysicalMemorySize();
		/*[ENDIF] JAVA_SPEC_VERSION >= 14 */
	}

	/*[IF JAVA_SPEC_VERSION > 8]*/
	@Deprecated(forRemoval = true, since = "19")
	/*[ENDIF] JAVA_SPEC_VERSION > 8 */
	public long getUsedPhysicalMemory() {
		return osinstance.getTotalPhysicalMemorySize() - osinstance.getFreePhysicalMemorySize();
	}
/*[ENDIF] JAVA_SPEC_VERSION < 20 */

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected GarbageCollectorMXBean makeGCBean(String domainName, String name, int internalID) {
		return new ExtendedGarbageCollectorMXBeanImpl(domainName, name, internalID, this);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected MemoryPoolMXBean makeMemoryPoolBean(String name, MemoryType type, int internalID) {
		return new MemoryPoolMXBeanImpl(name, type, internalID, this);
	}

}
