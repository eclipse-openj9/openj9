/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

import com.ibm.lang.management.UnixOperatingSystemMXBean;

/*
 * Implementation class for the interface UnixOperatingSystemMXBean.
 */
@SuppressWarnings("javadoc")
public final class UnixExtendedOperatingSystem extends ExtendedOperatingSystemMXBeanImpl implements UnixOperatingSystemMXBean {

	private static final UnixExtendedOperatingSystem instance = new UnixExtendedOperatingSystem();

	public static UnixExtendedOperatingSystem getInstance() {
		return instance;
	}

	private UnixExtendedOperatingSystem() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getMaxFileDescriptorCount() {
		return getMaxFileDescriptorCountImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getOpenFileDescriptorCount() {
		return getOpenFileDescriptorCountImpl();
	}

	private static native long getMaxFileDescriptorCountImpl();

	private static native long getOpenFileDescriptorCountImpl();

}
