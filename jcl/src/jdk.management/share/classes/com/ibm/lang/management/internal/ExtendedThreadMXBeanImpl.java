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

import com.ibm.java.lang.management.internal.ThreadMXBeanImpl;
import com.ibm.lang.management.ExtendedThreadInfo;
import com.ibm.lang.management.ThreadMXBean;

/**
 * Implementation of the extended ThreadMXBean.
 */
public final class ExtendedThreadMXBeanImpl extends ThreadMXBeanImpl implements ThreadMXBean {

	private static final ThreadMXBean instance = new ExtendedThreadMXBeanImpl();

	/**
	 * Singleton accessor method.
	 * 
	 * @return the <code>ExtendedThreadMXBeanImpl</code> singleton.
	 */
	public static ThreadMXBean getInstance() {
		return instance;
	}

	private ExtendedThreadMXBeanImpl() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ExtendedThreadInfo[] dumpAllExtendedThreads(boolean lockedMonitors, boolean lockedSynchronizers)
			throws InternalError, SecurityException, UnsupportedOperationException {
		/* For security manager checks fall back on dumpAllThreads() to do this. */
		ThreadInfo[] threadInfos = dumpAllThreads(lockedMonitors, lockedSynchronizers);
		int infoCount = threadInfos.length;
		ExtendedThreadInfo[] resultArray = new ExtendedThreadInfo[infoCount];

		for (int index = 0; index < infoCount; ++index) {
			/* Construct the ExtendedThreadInfo object. */
			resultArray[index] = new ExtendedThreadInfoImpl(threadInfos[index]);
		}

		return resultArray;
	}

}
