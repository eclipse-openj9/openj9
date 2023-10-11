/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 *******************************************************************************/
package openj9.internal.criu;

/**
 * Internal CRIU Support API
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@SuppressWarnings({ "deprecation", "removal" })
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public final class InternalCRIUSupport {
	private static final boolean criuSupportEnabled = isCRIUSupportEnabledImpl();
	private static long checkpointRestoreNanoTimeDelta;

	private static native boolean isCRIUSupportEnabledImpl();
	private static native boolean isCheckpointAllowedImpl();
	private static native long getCheckpointRestoreNanoTimeDeltaImpl();
	private static native long getLastRestoreTimeImpl();

	/**
	 * Retrieve the elapsed time between Checkpoint and Restore.
	 * Only support one Checkpoint.
	 *
	 * @return the time in nanoseconds.
	 */
	public static long getCheckpointRestoreNanoTimeDelta() {
		if (checkpointRestoreNanoTimeDelta == 0) {
			checkpointRestoreNanoTimeDelta = getCheckpointRestoreNanoTimeDeltaImpl();
		}
		return checkpointRestoreNanoTimeDelta;
	}

	/**
	 * Retrieve the time when the last restore occurred. In the case of multiple
	 * restores the previous times are overwritten.
	 *
	 * @return the time in milliseconds since the start of the epoch, -1 if restore
	 *         has not occurred.
	 */
	public static long getLastRestoreTime() {
		return getLastRestoreTimeImpl();
	}

	/**
	 * Queries if CRIU support is enabled.
	 *
	 * @return true if CRIU support is enabled, false otherwise
	 */
	public synchronized static boolean isCRIUSupportEnabled() {
		return criuSupportEnabled;
	}

	/**
	 * Queries if CRIU Checkpoint is allowed.
	 * isCRIUSupportEnabled() is invoked first to check if CRIU support is enabled,
	 * and j9criu29 is to be lazily loaded at CRIUSupport.checkpointJVM().
	 *
	 * @return true if Checkpoint is allowed, otherwise false
	 */
	public static boolean isCheckpointAllowed() {
		boolean checkpointAllowed = false;
		if (criuSupportEnabled) {
			checkpointAllowed = isCheckpointAllowedImpl();
		}
		return checkpointAllowed;
	}

	/**
	 *
	 * Executes a runnable in NotCheckpointSafe mode. This means that while the
	 * runnable frame is on the stack, a checkpoint cannot be taken.
	 *
	 * @param run Runnable
	 */
	@NotCheckpointSafe
	public static void runRunnableInNotCheckpointSafeMode(Runnable run) {
		run.run();
	}
}
