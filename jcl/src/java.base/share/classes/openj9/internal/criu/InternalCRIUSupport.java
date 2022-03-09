/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package openj9.internal.criu;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * Internal CRIU Support API
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@SuppressWarnings({ "deprecation", "removal" })
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public final class InternalCRIUSupport {

	private static boolean criuSupportEnabled;
	private static boolean nativeLoaded;
	private static boolean initComplete;

	private static native boolean isCRIUSupportEnabledImpl();

	private static native boolean isCheckpointAllowedImpl();

	private static void init() {
		if (!initComplete) {
			if (loadNativeLibrary()) {
				criuSupportEnabled = isCRIUSupportEnabledImpl();
			}
			initComplete = true;
		}
	}

	/**
	 * Queries if CRIU support is enabled.
	 *
	 * @return true is support is enabled, false otherwise
	 */
	public synchronized static boolean isCRIUSupportEnabled() {
		if (!initComplete) {
			init();
		}

		return criuSupportEnabled;
	}

	/**
	 * Returns an error message describing why isCRIUSupportEnabled() returns false,
	 * and what can be done to remediate the issue.
	 *
	 * @return null if isCRIUSupportEnabled() returns true. Otherwise the error
	 *         message
	 */
	public static String getErrorMessage() {
		String s = null;
		if (!isCRIUSupportEnabled()) {
			if (nativeLoaded) {
				s = "To enable criu support, please run java with the `-XX:+EnableCRIUSupport` option.";
			} else {
				s = "There was a problem loaded the criu native library.\n"
						+ "Please check that criu is installed on the machine by running `criu check`.\n"
						+ "Also, please ensure that the JDK is criu enabled by contacting your JDK provider.";
			}
		}
		return s;
	}

	private static boolean loadNativeLibrary() {
		if (!nativeLoaded) {
			try {
				AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
					System.loadLibrary("j9criu29"); //$NON-NLS-1$
					nativeLoaded = true;
					return null;
				});
			} catch (UnsatisfiedLinkError e) {
				if (System.getProperty("enable.j9internal.checkpoint.hook.api.debug") != null) { //$NON-NLS-1$
					e.printStackTrace();
				}
			}
		}

		return nativeLoaded;
	}

	/**
	 * Queries if CRIU Checkpoint is allowed. isCRIUSupportEnabled() is invoked
	 * first to ensure proper j9criu29 library loading.
	 *
	 * @return true if Checkpoint is allowed, otherwise false
	 */
	public static boolean isCheckpointAllowed() {
		boolean checkpointAllowed = false;
		if (isCRIUSupportEnabled()) {
			checkpointAllowed = isCheckpointAllowedImpl();
		}

		return checkpointAllowed;
	}
}
