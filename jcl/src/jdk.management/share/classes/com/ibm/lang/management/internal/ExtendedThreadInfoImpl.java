/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.ibm.lang.management.internal;

import java.lang.management.ThreadInfo;
import java.lang.reflect.Field;
import java.security.AccessController;
import java.security.PrivilegedAction;

import com.ibm.lang.management.ExtendedThreadInfo;

final class ExtendedThreadInfoImpl implements ExtendedThreadInfo {

	private final long nativeTID; /* Native thread ID for the thread. */

	private final ThreadInfo threadInfo; /* The ThreadInfo object whose native ID we store. */

	ExtendedThreadInfoImpl(ThreadInfo threadInfo) {
		super();
		this.nativeTID = com.ibm.java.lang.management.internal.ManagementAccessControl.getThreadInfoAccess().getNativeTId(threadInfo);
		this.threadInfo = threadInfo;
	}

	/**
	 * @return The native thread ID.
	 */
	@Override
	public long getNativeThreadId() {
		return nativeTID;
	}

	/**
	 * @return The {@link ThreadInfo} instance that {@link ExtendedThreadInfo} harbors.
	 */
	@Override
	public ThreadInfo getThreadInfo() {
		return threadInfo;
	}

}
