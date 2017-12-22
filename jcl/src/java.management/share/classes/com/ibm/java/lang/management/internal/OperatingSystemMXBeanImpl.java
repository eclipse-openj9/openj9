/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
import java.lang.management.OperatingSystemMXBean;

import javax.management.MBeanNotificationInfo;
import javax.management.NotificationBroadcasterSupport;
import javax.management.ObjectName;

/**
 * Runtime type for {@link java.lang.management.OperatingSystemMXBean}.
 *
 * @since 1.5
 */
public class OperatingSystemMXBeanImpl extends NotificationBroadcasterSupport implements OperatingSystemMXBean {

	private static final OperatingSystemMXBeanImpl instance = new OperatingSystemMXBeanImpl();

	/**
	 * Singleton accessor method.
	 * 
	 * @return the {@link OperatingSystemMXBeanImpl} singleton.
	 */
	public static OperatingSystemMXBeanImpl getInstance() {
		return instance;
	}

	private final ObjectName objectName;

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean.
	 */
	protected OperatingSystemMXBeanImpl() {
		super();
		objectName = ManagementUtils.createObjectName(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getArch() {
        return System.getProperty("os.arch"); //$NON-NLS-1$
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public final int getAvailableProcessors() {
        return Runtime.getRuntime().availableProcessors();
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public final String getName() {
		return System.getProperty("os.name"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MBeanNotificationInfo[] getNotificationInfo() {
		// This base implementation doesn't provide for any notifications.
		return new MBeanNotificationInfo[0];
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ObjectName getObjectName() {
		return objectName;
	}

	/**
	 * {@inheritDoc}
	 */
    @Override
	public final double getSystemLoadAverage() {
		return this.getSystemLoadAverageImpl();
	}

    /**
     * @return the time-averaged value of the sum of the number of runnable
     *         entities running on the available processors together with the
     *         number of runnable entities ready and queued to run on the
     *         available processors.
     * @see #getSystemLoadAverage()
     */
    private native double getSystemLoadAverageImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
    public final String getVersion() {
		return System.getProperty("os.version"); //$NON-NLS-1$
	}

}
