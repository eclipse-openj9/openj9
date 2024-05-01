/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2021
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
package org.eclipse.openj9.criu;

import java.nio.file.Path;
import openj9.internal.criu.InternalCRIUSupport;

/**
 * CRIU Support API
 *
 * A checkpoint is the act of halting the JVM, saving its state and writing it out to a file(s). Restore is reading the saved checkpoint state
 * and resuming the JVM application from when it was checkpointed. This technology is available via CRIU (checkpoint/restore in user space
 * see, https://criu.org/Main_Page).
 *
 * This API enables the use of CRIU capabilities provided by the OS as well as JVM support for facilitating a successful checkpoint
 * and restore in varying environments.
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@SuppressWarnings({ "deprecation", "removal" })
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public final class CRIUSupport {

	/**
	 * A hook can be one of the following states:
	 *
	 * SINGLE_THREAD_MODE - a mode in which only the Java thread that requested a
	 * checkpoint is permitted to run. This means that global state changes are
	 * permitted without the risk of race conditions. It also implies that if one
	 * attempts to acquire a resource held by another Java thread a deadlock might
	 * occur, or a JVMCheckpointException or JVMRestoreException to be thrown if
	 * such an attempt is detected by JVM.
	 *
	 * CONCURRENT_MODE - a hook running when the SINGLE_THREAD_MODE is NOT enabled
	 */
	public static enum HookMode {
		SINGLE_THREAD_MODE,
		CONCURRENT_MODE
	}

	private InternalCRIUSupport internalCRIUSupport;

	/**
	 * Constructs a new {@code CRIUSupport}.
	 *
	 * The default CRIU dump options are:
	 * <p>
	 * {@code imageDir} = imageDir, the directory where the images are to be
	 * created.
	 * <p>
	 * {@code leaveRunning} = false
	 * <p>
	 * {@code shellJob} = false
	 * <p>
	 * {@code extUnixSupport} = false
	 * <p>
	 * {@code logLevel} = 2
	 * <p>
	 * {@code logFile} = criu.log
	 * <p>
	 * {@code fileLocks} = false
	 * <p>
	 * {@code ghostFileLimit} = 1 MB
	 * <p>
	 * {@code workDir} = imageDir, the directory where the images are to be created.
	 *
	 * @param imageDir the directory that will hold the dump files as a
	 *                 java.nio.file.Path
	 * @throws NullPointerException     if imageDir is null
	 * @throws SecurityException        if no permission to access imageDir or no
	 *                                  CRIU_DUMP_PERMISSION
	 * @throws IllegalArgumentException if imageDir is not a valid directory
	 */
	public CRIUSupport(Path imageDir) {
		internalCRIUSupport = new InternalCRIUSupport(imageDir);
	}

	/**
	 * Queries if CRIU support is enabled and criu library has been loaded.
	 *
	 * @return TRUE if CRIU support is enabled and the library is loaded, FALSE otherwise
	 */
	public static boolean isCRIUSupportEnabled() {
		return InternalCRIUSupport.isCRIUSupportEnabledAndNativeLoaded();
	}

	/**
	 * Checks if the CRIUSecProvider is enabled when CRIU
	 * checkpoints are allowed (checks whether -XX:-CRIUSecProvider
	 * has been specified).
	 *
	 * @return true if CRIUSecProvider is enabled, otherwise false
	 */
	public static boolean enableCRIUSecProvider() {
		return InternalCRIUSupport.enableCRIUSecProvider();
	}

	/**
	 * Queries if CRIU Checkpoint is allowed. With -XX:+CRIURestoreNonPortableMode enabled
	 * (default policy) only a single checkpoint is allowed.
	 *
	 * @return true if Checkpoint is allowed, otherwise false
	 */
	public static boolean isCheckpointAllowed() {
		return InternalCRIUSupport.isCheckpointAllowed();
	}

	/**
	 * Returns an error message describing why isCRIUSupportEnabled()
	 * returns false, and what can be done to remediate the issue.
	 *
	 * @return NULL if isCRIUSupportEnabled() returns true and nativeLoaded is true as well, otherwise the error message.
	 */
	public static String getErrorMessage() {
		return InternalCRIUSupport.getErrorMessage();
	}

	/**
	 * Sets the directory that will hold the images upon checkpoint. This must be
	 * set before calling {@link #checkpointJVM()}.
	 *
	 * @param imageDir the directory as a java.nio.file.Path
	 * @return this
	 * @throws NullPointerException     if imageDir is null
	 * @throws SecurityException        if no permission to access imageDir
	 * @throws IllegalArgumentException if imageDir is not a valid directory
	 */
	public CRIUSupport setImageDir(Path imageDir) {
		internalCRIUSupport = internalCRIUSupport.setImageDir(imageDir);
		return this;
	}

	/**
	 * Controls whether process trees are left running after checkpoint.
	 * <p>
	 * Default: false
	 *
	 * @param leaveRunning
	 * @return this
	 */
	public CRIUSupport setLeaveRunning(boolean leaveRunning) {
		internalCRIUSupport = internalCRIUSupport.setLeaveRunning(leaveRunning);
		return this;
	}

	/**
	 * Controls ability to dump shell jobs.
	 * <p>
	 * Default: false
	 *
	 * @param shellJob
	 * @return this
	 */
	public CRIUSupport setShellJob(boolean shellJob) {
		internalCRIUSupport = internalCRIUSupport.setShellJob(shellJob);
		return this;
	}

	/**
	 * Controls whether to dump only one end of a unix socket pair.
	 * <p>
	 * Default: false
	 *
	 * @param extUnixSupport
	 * @return this
	 */
	public CRIUSupport setExtUnixSupport(boolean extUnixSupport) {
		internalCRIUSupport = internalCRIUSupport.setExtUnixSupport(extUnixSupport);
		return this;
	}

	/**
	 * Sets the verbosity of log output. Available levels:
	 * <ol>
	 * <li>Only errors
	 * <li>Errors and warnings
	 * <li>Above + information messages and timestamps
	 * <li>Above + debug
	 * </ol>
	 * <p>
	 * Default: 2
	 *
	 * @param logLevel verbosity from 1 to 4 inclusive
	 * @return this
	 * @throws IllegalArgumentException if logLevel is not valid
	 */
	public CRIUSupport setLogLevel(int logLevel) {
		internalCRIUSupport = internalCRIUSupport.setLogLevel(logLevel);
		return this;
	}

	/**
	 * Write log output to logFile.
	 * <p>
	 * Default: criu.log
	 *
	 * @param logFile name of the file to write log output to. The path to the file
	 *                can be set with {@link #setWorkDir(Path)}.
	 * @return this
	 * @throws IllegalArgumentException if logFile is null or a path
	 */
	public CRIUSupport setLogFile(String logFile) {
		internalCRIUSupport = internalCRIUSupport.setLogFile(logFile);
		return this;
	}

	/**
	 * Controls whether to dump file locks.
	 * <p>
	 * Default: false
	 *
	 * @param fileLocks
	 * @return this
	 */
	public CRIUSupport setFileLocks(boolean fileLocks) {
		internalCRIUSupport = internalCRIUSupport.setFileLocks(fileLocks);
		return this;
	}

	/**
	 * Controls whether to re-establish TCP connects.
	 * <p>
	 * Default: false
	 *
	 * @param tcpEstablished
	 * @return this
	 */
	public CRIUSupport setTCPEstablished(boolean tcpEstablished) {
		internalCRIUSupport = internalCRIUSupport.setTCPEstablished(tcpEstablished);
		return this;
	}

	/**
	 * Controls whether auto dedup of memory pages is enabled.
	 * <p>
	 * Default: false
	 *
	 * @param autoDedup
	 * @return this
	 */
	public CRIUSupport setAutoDedup(boolean autoDedup) {
		internalCRIUSupport = internalCRIUSupport.setAutoDedup(autoDedup);
		return this;
	}

	/**
	 * Controls whether memory tracking is enabled.
	 * <p>
	 * Default: false
	 *
	 * @param trackMemory
	 * @return this
	 */
	public CRIUSupport setTrackMemory(boolean trackMemory) {
		internalCRIUSupport = internalCRIUSupport.setTrackMemory(trackMemory);
		return this;
	}

	/**
	 * Sets the directory where non-image files are stored (e.g. logs).
	 * <p>
	 * Default: same as path set by {@link #setImageDir(Path)}.
	 *
	 * @param workDir the directory as a java.nio.file.Path
	 * @return this
	 * @throws NullPointerException     if workDir is null
	 * @throws SecurityException        if no permission to access workDir
	 * @throws IllegalArgumentException if workDir is not a valid directory
	 */
	public CRIUSupport setWorkDir(Path workDir) {
		internalCRIUSupport = internalCRIUSupport.setWorkDir(workDir);
		return this;
	}

	/**
	 * Controls whether CRIU will be invoked in privileged or unprivileged mode.
	 * <p>
	 * Default: false
	 *
	 * @param unprivileged
	 * @return this
	 */
	public CRIUSupport setUnprivileged(boolean unprivileged) {
		internalCRIUSupport = internalCRIUSupport.setUnprivileged(unprivileged);
		return this;
	}

	/**
	 * Set the size limit for ghost files when taking a checkpoint. File limit
	 * can not be greater than 2^32 - 1 or negative.
	 *
	 * Default: 1MB set by CRIU
	 *
	 * @param limit the file limit size in bytes.
	 * @return this
	 * @throws UnsupportedOperationException if file limit is greater than 2^32 - 1 or negative.
	 */
	public CRIUSupport setGhostFileLimit(long limit) {
		internalCRIUSupport = internalCRIUSupport.setGhostFileLimit(limit);
		return this;
	}

	/**
	 * Append new environment variables to the set returned by ProcessEnvironment.getenv(...) upon
	 * restore. All pre-existing (environment variables from checkpoint run) env
	 * vars are retained. All environment variables specified in the envFile are
	 * added as long as they do not modifiy pre-existeing environment variables.
	 *
	 * Format for envFile is the following: ENV_VAR_NAME1=ENV_VAR_VALUE1 ...
	 * ENV_VAR_NAMEN=ENV_VAR_VALUEN
	 *
	 * OPENJ9_RESTORE_JAVA_OPTIONS is a special environment variable that can be
	 * used to add JVM options on restore.
	 *
	 * @param envFile The file that contains the new environment variables to be
	 *                added
	 * @return this
	 */
	public CRIUSupport registerRestoreEnvFile(Path envFile) {
		internalCRIUSupport = internalCRIUSupport.registerRestoreEnvFile(envFile);
		return this;
	}

	/**
	 * Add new JVM options upon restore. The options will be specified in an options
	 * file with the form specified in https://www.eclipse.org/openj9/docs/xoptionsfile/
	 *
	 * Only a subset of JVM options are available on restore. Consult the CRIU Support section of Eclipse OpenJ9 documentation to determine
	 * which options are supported.
	 *
	 * @param optionsFile The file that contains the new JVM options to be added on restore
	 *
	 * @return this
	 */
	public CRIUSupport registerRestoreOptionsFile(Path optionsFile) {
		internalCRIUSupport = internalCRIUSupport.registerRestoreOptionsFile(optionsFile);
		return this;
	}

	/**
	 * User hook that is run after restoring a checkpoint image. This is equivalent
	 * to registerPostRestoreHook(hook, HookMode.SINGLE_THREAD_MODE,
	 * DEFAULT_SINGLE_THREAD_MODE_HOOK_PRIORITY);
	 *
	 * Hooks will be run in single threaded mode, no other application threads
	 * will be active. Users should avoid synchronization of objects that are not owned
	 * by the thread, terminally blocking operations and launching new threads in the hook.
	 * If the thread attempts to acquire a lock that it doesn't own, an exception will
	 * be thrown.
	 *
	 * @param hook user hook
	 *
	 * @return this
	 */
	public CRIUSupport registerPostRestoreHook(Runnable hook) {
		try {
			internalCRIUSupport = internalCRIUSupport.registerPostRestoreHook(hook);
		} catch (openj9.internal.criu.JVMCheckpointException jce) {
			throw new JVMCheckpointException(jce.getMessage(), 0, jce);
		} catch (openj9.internal.criu.JVMRestoreException jre) {
			throw new JVMRestoreException(jre.getMessage(), 0, jre);
		}
		return this;
	}

	/**
	 * User hook that is run after restoring a checkpoint image.
	 *
	 * If SINGLE_THREAD_MODE is requested, no other application threads will be
	 * active. Users should avoid synchronization of objects that are not owned by
	 * the thread, terminally blocking operations and launching new threads in the
	 * hook. If the thread attempts to acquire a lock that it doesn't own, an
	 * exception will be thrown.
	 *
	 * If CONCURRENT_MODE is requested, the hook will be run alongside all other
	 * active Java threads.
	 *
	 * High-priority hooks are run first after restore, and vice-versa for
	 * low-priority hooks. The priority of the hook is with respect to the other
	 * hooks run within that mode. CONCURRENT_MODE hooks are implicitly lower
	 * priority than SINGLE_THREAD_MODE hooks. Ie. the lowest priority
	 * SINGLE_THREAD_MODE hook is a higher priority than the highest priority
	 * CONCURRENT_MODE hook. The hooks of the same mode with the same priority are
	 * run in random order.
	 *
	 * @param hook     user hook
	 * @param mode     the mode in which the hook is run, either CONCURRENT_MODE or
	 *                 SINGLE_THREAD_MODE
	 * @param priority the priority of the hook, between LOWEST_USER_HOOK_PRIORITY -
	 *                 HIGHEST_USER_HOOK_PRIORITY. Throws
	 *                 UnsupportedOperationException otherwise.
	 *
	 * @return this
	 *
	 * @throws UnsupportedOperationException if the hook mode is not
	 *                                       SINGLE_THREAD_MODE or CONCURRENT_MODE
	 *                                       or the priority is not between
	 *                                       LOWEST_USER_HOOK_PRIORITY and
	 *                                       HIGHEST_USER_HOOK_PRIORITY.
	 */
	public CRIUSupport registerPostRestoreHook(Runnable hook, HookMode mode, int priority)
			throws UnsupportedOperationException {
		InternalCRIUSupport.HookMode internalMode;
		if (mode == HookMode.SINGLE_THREAD_MODE) {
			internalMode = InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE;
		} else {
			internalMode = InternalCRIUSupport.HookMode.CONCURRENT_MODE;
		}
		try {
			internalCRIUSupport = internalCRIUSupport.registerPostRestoreHook(hook, internalMode, priority);
		} catch (openj9.internal.criu.JVMCheckpointException jce) {
			throw new JVMCheckpointException(jce.getMessage(), 0, jce);
		} catch (openj9.internal.criu.JVMRestoreException jre) {
			throw new JVMRestoreException(jre.getMessage(), 0, jre);
		}
		return this;
	}

	/**
	 * User hook that is run before checkpointing the JVM. This is equivalent to
	 * registerPreCheckpointHook(hook, HookMode.SINGLE_THREAD_MODE,
	 * DEFAULT_SINGLE_THREAD_MODE_HOOK_PRIORITY).
	 *
	 * Hooks will be run in single threaded mode, no other application threads
	 * will be active. Users should avoid synchronization of objects that are not owned
	 * by the thread, terminally blocking operations and launching new threads in the hook.
	 * If the thread attempts to acquire a lock that it doesn't own, an exception will
	 * be thrown.
	 *
	 * @param hook user hook
	 *
	 * @return this
	 *
	 */
	public CRIUSupport registerPreCheckpointHook(Runnable hook) {
		try {
			internalCRIUSupport = internalCRIUSupport.registerPreCheckpointHook(hook);
		} catch (openj9.internal.criu.JVMCheckpointException jce) {
			throw new JVMCheckpointException(jce.getMessage(), 0, jce);
		} catch (openj9.internal.criu.JVMRestoreException jre) {
			throw new JVMRestoreException(jre.getMessage(), 0, jre);
		}
		return this;
	}

	/**
	 * User hook that is run before checkpointing the JVM.
	 *
	 * If SINGLE_THREAD_MODE is requested, no other application threads will be
	 * active. Users should avoid synchronization of objects that are not owned by
	 * the thread, terminally blocking operations and launching new threads in the
	 * hook. If the thread attempts to acquire a lock that it doesn't own, an
	 * exception will be thrown.
	 *
	 * If CONCURRENT_MODE is requested, the hook will be run alongside all other
	 * active Java threads.
	 *
	 * High-priority hooks are run last before checkpoint, and vice-versa for
	 * low-priority hooks. The priority of the hook is with respect to the other
	 * hooks run within that mode. CONCURRENT_MODE hooks are implicitly lower
	 * priority than SINGLE_THREAD_MODEd hooks. Ie. the lowest priority
	 * SINGLE_THREAD_MODE hook is a higher priority than the highest priority
	 * CONCURRENT_MODE hook. The hooks of the same mode with the same priority are
	 * run in random order.
	 *
	 * @param hook     user hook
	 * @param mode     the mode in which the hook is run, either CONCURRENT_MODE or
	 *                 SINGLE_THREAD_MODE
	 * @param priority the priority of the hook, between LOWEST_USER_HOOK_PRIORITY -
	 *                 HIGHEST_USER_HOOK_PRIORITY. Throws
	 *                 UnsupportedOperationException otherwise.
	 *
	 * @return this
	 *
	 * @throws UnsupportedOperationException if the hook mode is not
	 *                                       SINGLE_THREAD_MODE or CONCURRENT_MODE
	 *                                       or the priority is not between
	 *                                       LOWEST_USER_HOOK_PRIORITY and
	 *                                       HIGHEST_USER_HOOK_PRIORITY.
	 */
	public CRIUSupport registerPreCheckpointHook(Runnable hook, HookMode mode, int priority)
			throws UnsupportedOperationException {
		InternalCRIUSupport.HookMode internalMode;
		if (mode == HookMode.SINGLE_THREAD_MODE) {
			internalMode = InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE;
		} else {
			internalMode = InternalCRIUSupport.HookMode.CONCURRENT_MODE;
		}
		try {
			internalCRIUSupport = internalCRIUSupport.registerPreCheckpointHook(hook, internalMode, priority);
		} catch (openj9.internal.criu.JVMCheckpointException jce) {
			throw new JVMCheckpointException(jce.getMessage(), 0, jce);
		} catch (openj9.internal.criu.JVMRestoreException jre) {
			throw new JVMRestoreException(jre.getMessage(), 0, jre);
		}
		return this;
	}

	/**
	 * Checkpoint the JVM. This operation will use the CRIU options set by the
	 * options setters.
	 *
	 * @throws UnsupportedOperationException if CRIU is not supported
	 *  or running in non-portable mode (only one checkpoint is allowed),
	 *  and we have already checkpointed once.
	 * @throws JVMCheckpointException        if a JVM error occurred before checkpoint
	 * @throws SystemCheckpointException     if a System operation failed before checkpoint
	 * @throws SystemRestoreException        if a System operation failed during or after restore
	 * @throws JVMRestoreException           if an error occurred during or after restore
	 */
	public synchronized void checkpointJVM() {
		if (isCRIUSupportEnabled()) {
			try {
				internalCRIUSupport.checkpointJVM();
			} catch (openj9.internal.criu.JVMCheckpointException jce) {
				throw new JVMCheckpointException(jce.getMessage(), 0, jce);
			} catch (openj9.internal.criu.JVMRestoreException jre) {
				throw new JVMRestoreException(jre.getMessage(), 0, jre);
			} catch (openj9.internal.criu.SystemCheckpointException sce) {
				throw new SystemCheckpointException(sce.getMessage(), 0, sce);
			} catch (openj9.internal.criu.SystemRestoreException sre) {
				throw new SystemRestoreException(sre.getMessage(), 0, sre);
			}
		} else {
			throw new UnsupportedOperationException("CRIU support is not enabled"); //$NON-NLS-1$
		}
	}
}
