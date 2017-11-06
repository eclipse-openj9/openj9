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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.lang.management.internal;

import java.lang.management.ThreadInfo;
import java.lang.reflect.Field;
import java.security.AccessController;
import java.security.PrivilegedAction;

import com.ibm.lang.management.ExtendedThreadInfo;

final class ExtendedThreadInfoImpl implements ExtendedThreadInfo {

	/*
	 * @brief Statically resolve class ThreadInfo's field nativeTId via reflection
	 * and maintain this for repeated uses thereafter.
	 */
	private static final Field nativeTIDField = AccessController.doPrivileged(new PrivilegedAction<Field>() {
		@Override
		public Field run() {
			try {
				Field field = ThreadInfo.class.getDeclaredField("nativeTId"); //$NON-NLS-1$
				field.setAccessible(true);
				return field;
			} catch (NoSuchFieldException e) {
				/* Handle all sorts of internal errors arising due to reflection by rethrowing an InternalError. */
				/*[MSG "K05E4", "Internal error while obtaining thread identification."]*/
				InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K05E4")); //$NON-NLS-1$
				error.initCause(e);
				throw error;
			}
		}
	});

	private final long nativeTID; /* Native thread ID for the thread. */

	private final ThreadInfo threadInfo; /* The ThreadInfo object whose native ID we store. */

	ExtendedThreadInfoImpl(ThreadInfo threadInfo) {
		super();
		try {
			this.nativeTID = ((Long) nativeTIDField.get(threadInfo)).longValue();
			this.threadInfo = threadInfo;
		} catch (IllegalAccessException e) {
			/* Handle all sorts of internal errors arising due to reflection by rethrowing an InternalError. */
			/*[MSG "K05E4", "Internal error while obtaining thread identification."]*/
			InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K05E4")); //$NON-NLS-1$
			error.initCause(e);
			throw error;
		}
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
