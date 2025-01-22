/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2006
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
package com.ibm.jvm;

import java.util.Objects;

/**
 *
 * The <code>Log</code> class contains methods for controlling system log options
 * This class cannot be instantiated.
 */
public class Log {

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private static final String LEGACY_LOG_PERMISSION_PROPERTY = "com.ibm.jvm.enableLegacyLogSecurity"; //$NON-NLS-1$

	private static final LogPermission LOG_PERMISSION = new LogPermission();
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	/**
	 * Query the log options. Returns a String representation of the log options.
	 * @return The current log options
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to query the log settings
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static String QueryOptions() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkLegacySecurityPermssion();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		return QueryOptionsImpl();
	}

	/**
	 * Set the log options.
	 * Use the same syntax as the -Xsyslog command-line option, with the initial -Xsyslog: omitted.
	 *
	 * @param options The command line log flags.
	 * @return status 0 on success otherwise a RuntimeException is thrown
	 *
	 * @throws RuntimeException if there is a problem setting the log options
	/*[IF JAVA_SPEC_VERSION < 24]
	 * @throws SecurityException if there is a security manager and it doesn't allow the checks required to change the log settings
	/*[ENDIF] JAVA_SPEC_VERSION < 24
	 */
	public static int SetOptions(String options) {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkLegacySecurityPermssion();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		Objects.requireNonNull(options, "options"); //$NON-NLS-1$
		return SetOptionsImpl(options);
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	/**
	 * Check the caller has permission to use the Log API for calls that existed pre-Java 8
	 * when security was added. Public API added after Java 8 should call checkLogSecurityPermssion()
	 * directly.
	 * @throws SecurityException
	 */
	private static void checkLegacySecurityPermssion() throws SecurityException {
		if (!("false".equalsIgnoreCase(com.ibm.oti.vm.VM //$NON-NLS-1$
				.internalGetProperties().getProperty(LEGACY_LOG_PERMISSION_PROPERTY)))
		) {
			checkLogSecurityPermssion();
		}
	}

	private static void checkLogSecurityPermssion() throws SecurityException {
		/* Check the caller has LogPermission. */
		@SuppressWarnings("removal")
		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(LOG_PERMISSION);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	/*
	 * Log should not be instantiated.
	 */
	private Log() {
	}

	private static native String QueryOptionsImpl();
	private static native int SetOptionsImpl(String options);
}
