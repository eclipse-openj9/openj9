/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
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
 */
package openj9.internal.criu;

import com.ibm.oti.vm.VM;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Properties;
/*[IF JAVA_SPEC_VERSION == 8]*/
import sun.misc.Unsafe;
/*[ELSE]
import jdk.internal.misc.Unsafe;
/*[ENDIF] JAVA_SPEC_VERSION == 8 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
import jdk.internal.access.JavaNetInetAddressAccess;
import jdk.internal.access.SharedSecrets;
/*[ELSE] JAVA_SPEC_VERSION >= 17
import jdk.internal.misc.JavaNetInetAddressAccess;
import jdk.internal.misc.SharedSecrets;
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

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
public final class InternalCRIUSupport {
	private static final boolean criuSupportEnabled = isCRIUSupportEnabledImpl();
	private static long checkpointRestoreNanoTimeDelta;

	private static native boolean enableCRIUSecProviderImpl();
	private static native long getCheckpointRestoreNanoTimeDeltaImpl();
	private static native long getLastRestoreTimeImpl();
	private static native long getProcessRestoreStartTimeImpl();
	private static native boolean isCRIUSupportEnabledImpl();
	private static native boolean isCheckpointAllowedImpl();
/*[IF CRAC_SUPPORT]*/
	private static final boolean cracSupportEnabled = isCRaCSupportEnabledImpl();
	private static native boolean isCRaCSupportEnabledImpl();
	private static String cracCheckpointToDir = "";
	private static native String getCRaCCheckpointToDirImpl();
/*[ENDIF] CRAC_SUPPORT */

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
	 * @return the time in nanoseconds since the start of the epoch, -1 if restore
	 *         has not occurred.
	 */
	public static long getLastRestoreTime() {
		return getLastRestoreTimeImpl();
	}

	/**
	 * Get the start time of the CRIU process that restores the java process.
	 * The time that is set by the restored java process when it resumes from
	 * checkpoint can be retrieved with {@link #getLastRestoreTime()}.
	 *
	 * @return the time in nanoseconds since the epoch, -1 if restore has not occurred.
	 */
	public static long getProcessRestoreStartTime() {
		return getProcessRestoreStartTimeImpl();
	}

	/**
	 * Queries if CRaC or CRIU support is enabled.
	 *
	 * @return true if CRaC or CRIU support is enabled, false otherwise
	 */
	public synchronized static boolean isCRaCorCRIUSupportEnabled() {
		boolean result = criuSupportEnabled;
/*[IF CRAC_SUPPORT]*/
		result |= cracSupportEnabled;
/*[ENDIF] CRAC_SUPPORT */
		return result;
	}

/*[IF CRAC_SUPPORT]*/
	/**
	 * Queries if CRaC support is enabled.
	 *
	 * @return true if CRaC support is enabled, false otherwise
	 */
	public synchronized static boolean isCRaCSupportEnabled() {
		return cracSupportEnabled;
	}
/*[ENDIF] CRAC_SUPPORT */

	/**
	 * Queries if CRIU support is enabled.
	 *
	 * @return true if CRIU support is enabled, false otherwise
	 */
	public synchronized static boolean isCRIUSupportEnabled() {
		return criuSupportEnabled;
	}

	/**
	 * Checks if CRIU Security provider is enabled
	 * when CRIU support is enabled.
	 *
	 * @return true if enabled, otherwise false
	 */
	public static boolean enableCRIUSecProvider() {
		boolean isCRIUSecProviderEnabled = false;
		if (criuSupportEnabled) {
			isCRIUSecProviderEnabled = enableCRIUSecProviderImpl();
		}
		return isCRIUSecProviderEnabled;
	}

	/**
	 * Queries if CRIU Checkpoint is allowed.
	 * isCRaCorCRIUSupportEnabled() is invoked first to check if CRaC or CRIU support is enabled,
	 * and criu library has been loaded.
	 *
	 * @return true if Checkpoint is allowed, otherwise false
	 */
	public static boolean isCheckpointAllowed() {
		boolean checkpointAllowed = false;
		if (criuSupportEnabled
/*[IF CRAC_SUPPORT]*/
			|| cracSupportEnabled
/*[ENDIF] CRAC_SUPPORT */
		) {
			checkpointAllowed = isCheckpointAllowedImpl();
		}
		return checkpointAllowed;
	}

/*[IF CRAC_SUPPORT]*/
	/**
	 * Retrieve the CRaC checkpoint DIR specified via a command line option -XX:CRaCCheckpointTo=[dir].
	 *
	 * @return the CRaC checkpoint DIR, null if no such DIR was specified.
	 */
	public static String getCRaCCheckpointToDir() {
		if ((cracCheckpointToDir != null) && cracCheckpointToDir.isEmpty()) {
			cracCheckpointToDir = getCRaCCheckpointToDirImpl();
		}
		return cracCheckpointToDir;
	}
/*[ENDIF] CRAC_SUPPORT */

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

	@SuppressWarnings("restriction")
	private static Unsafe unsafe;
	private static final CRIUDumpPermission CRIU_DUMP_PERMISSION = new CRIUDumpPermission();
	private static boolean nativeLoaded;
	private static boolean initComplete;
	private static String errorMsg;

	private static native void checkpointJVMImpl(String imageDir,
			boolean leaveRunning,
			boolean shellJob,
			boolean extUnixSupport,
			int logLevel,
			String logFile,
			boolean fileLocks,
			String workDir,
			boolean tcpEstablished,
			boolean autoDedup,
			boolean trackMemory,
			boolean unprivileged,
			String optionsFile,
			String envFile,
			long ghostFileLimit);
	private static native boolean setupJNIFieldIDsAndCRIUAPI();

	private static native String[] getRestoreSystemProperites();

	private static void initializeUnsafe() {
		AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
			try {
				Field f = Unsafe.class.getDeclaredField("theUnsafe"); //$NON-NLS-1$
				f.setAccessible(true);
				unsafe = (Unsafe) f.get(null);
			} catch (NoSuchFieldException | SecurityException | IllegalArgumentException
					| IllegalAccessException e) {
				throw new InternalError(e);
			}
			return null;
		});
	}

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

	private static boolean loadNativeLibrary() {
		if (!nativeLoaded) {
			if (setupJNIFieldIDsAndCRIUAPI()) {
				nativeLoaded = true;
			}
		}

		return nativeLoaded;
	}

	private static void init() {
		if (!initComplete) {
			loadNativeLibrary();
			initComplete = true;
		}
	}

	/**
	 * Constructs a new {@code InternalCRIUSupport}.
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
	 * {@code workDir} = imageDir, the directory where the images are to be created.
	 *
	 * @param imageDir the directory that will hold the dump files as a
	 *                 java.nio.file.Path
	 * @throws NullPointerException     if imageDir is null
	 * @throws SecurityException        if no permission to access imageDir or no
	 *                                  CRIU_DUMP_PERMISSION
	 * @throws IllegalArgumentException if imageDir is not a valid directory
	 */
	public InternalCRIUSupport(Path imageDir) {
		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(CRIU_DUMP_PERMISSION);
		}

		setImageDir(imageDir);
	}

	/**
	 * Queries if the criu library has been loaded.
	 *
	 * @return TRUE if the library is loaded, FALSE otherwise.
	 */
	public static boolean isNativeLoaded() {
		init();
		return nativeLoaded;
	}

	/**
	 * Queries if CRaC or CRIU support is enabled and criu library has been loaded.
	 *
	 * @return TRUE if CRaC or CRIU support is enabled and the library is loaded, FALSE otherwise.
	 */
	public static boolean isCRaCorCRIUSupportEnabledAndNativeLoaded() {
		return (isNativeLoaded() && isCRaCorCRIUSupportEnabled());
	}

	/**
	 * Queries if CRIU support is enabled and criu library has been loaded.
	 *
	 * @return TRUE if CRIU support is enabled and the library is loaded, FALSE otherwise.
	 */
	public static boolean isCRIUSupportEnabledAndNativeLoaded() {
		return (isNativeLoaded() && isCRIUSupportEnabled());
	}

	/**
	 * Returns an error message describing why isCRaCorCRIUSupportEnabled()
	 * returns false, and what can be done to remediate the issue.
	 *
	 * @return NULL if isCRaCorCRIUSupportEnabled() returns true and nativeLoaded is true as well, otherwise the error message.
	 */
	public static String getErrorMessage() {
		if (errorMsg == null) {
			if (isCRaCorCRIUSupportEnabled()) {
				if (!nativeLoaded) {
					errorMsg = "There was a problem loaded the criu native library.\n" //$NON-NLS-1$
							+ "Please check that criu is installed on the machine by running `criu check`.\n" //$NON-NLS-1$
							+ "Also, please ensure that the JDK is criu enabled by contacting your JDK provider."; //$NON-NLS-1$
				}
				// no error message if nativeLoaded
			} else {
				errorMsg = "To enable criu support, please run java with the `-XX:+EnableCRIUSupport` option."; //$NON-NLS-1$
			}
		}

		return errorMsg;
	}

	/* Higher priority hooks are run last in pre-checkoint hooks, and are run
	 * first in post restore hooks.
	 */
	// the lowest priority of a user hook
	public static int LOWEST_USER_HOOK_PRIORITY = 0;
	// the highest priority of a user hook
	public static int HIGHEST_USER_HOOK_PRIORITY = 99;
	// the default SINGLE_THREAD_MODE hook priority
	private static final int DEFAULT_SINGLE_THREAD_MODE_HOOK_PRIORITY = LOWEST_USER_HOOK_PRIORITY;
	// other SINGLE_THREAD_MODE hook priority
	private static final int RESTORE_CLEAR_INETADDRESS_CACHE_PRIORITY = HIGHEST_USER_HOOK_PRIORITY + 1;
	private static final int RESTORE_ENVIRONMENT_VARIABLES_PRIORITY = HIGHEST_USER_HOOK_PRIORITY + 1;
	private static final int RESTORE_SYSTEM_PROPERTIES_PRIORITY = HIGHEST_USER_HOOK_PRIORITY + 1;
	/* RESET_CRIUSEC_PRIORITY and RESTORE_SECURITY_PROVIDERS_PRIORITY need to
	 * be higher than any other JVM hook that may require security providers.
	 */
	static final int RESET_CRIUSEC_PRIORITY = HIGHEST_USER_HOOK_PRIORITY + 1;
	static final int RESTORE_SECURITY_PROVIDERS_PRIORITY = HIGHEST_USER_HOOK_PRIORITY + 1;

	private String imageDir;
	private boolean leaveRunning;
	private boolean shellJob;
	private boolean extUnixSupport;
	private int logLevel;
	private String logFile;
	private boolean fileLocks;
	private String workDir;
	private boolean tcpEstablished;
	private boolean autoDedup;
	private boolean trackMemory;
	private boolean unprivileged;
	private Path envFile;
	private String optionsFile;
	private long ghostFileLimit = -1;

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
	public InternalCRIUSupport setGhostFileLimit(long limit) {
		if ((limit > 0xFFFFFFFFL) || (limit < 0)) {
			throw new UnsupportedOperationException("Ghost file must be non-negative and less than 4GB");
		}
		this.ghostFileLimit = limit;
		return this;
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
	public InternalCRIUSupport setImageDir(Path imageDir) {
		Objects.requireNonNull(imageDir, "Image directory cannot be null"); //$NON-NLS-1$
		if (!Files.isDirectory(imageDir)) {
			throw new IllegalArgumentException(imageDir.toAbsolutePath() + " is not a valid directory"); //$NON-NLS-1$
		}
		String dir = imageDir.toAbsolutePath().toString();

		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkWrite(dir);
		}

		this.imageDir = dir;
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
	public InternalCRIUSupport setLeaveRunning(boolean leaveRunning) {
		this.leaveRunning = leaveRunning;
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
	public InternalCRIUSupport setShellJob(boolean shellJob) {
		this.shellJob = shellJob;
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
	public InternalCRIUSupport setExtUnixSupport(boolean extUnixSupport) {
		this.extUnixSupport = extUnixSupport;
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
	public InternalCRIUSupport setLogLevel(int logLevel) {
		if (logLevel > 0 && logLevel <= 4) {
			this.logLevel = logLevel;
		} else {
			throw new IllegalArgumentException("Log level must be 1 to 4 inclusive"); //$NON-NLS-1$
		}
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
	public InternalCRIUSupport setLogFile(String logFile) {
		if (logFile != null && !logFile.contains(File.separator)) {
			this.logFile = logFile;
		} else {
			throw new IllegalArgumentException("Log file must not be null and not be a path"); //$NON-NLS-1$
		}
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
	public InternalCRIUSupport setFileLocks(boolean fileLocks) {
		this.fileLocks = fileLocks;
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
	public InternalCRIUSupport setTCPEstablished(boolean tcpEstablished) {
		this.tcpEstablished = tcpEstablished;
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
	public InternalCRIUSupport setAutoDedup(boolean autoDedup) {
		this.autoDedup = autoDedup;
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
	public InternalCRIUSupport setTrackMemory(boolean trackMemory) {
		this.trackMemory = trackMemory;
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
	public InternalCRIUSupport setWorkDir(Path workDir) {
		Objects.requireNonNull(workDir, "Work directory cannot be null"); //$NON-NLS-1$
		if (!Files.isDirectory(workDir)) {
			throw new IllegalArgumentException("workDir is not a valid directory"); //$NON-NLS-1$
		}
		String dir = workDir.toAbsolutePath().toString();

		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkWrite(dir);
		}

		this.workDir = dir;
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
	public InternalCRIUSupport setUnprivileged(boolean unprivileged) {
		this.unprivileged = unprivileged;
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
	public InternalCRIUSupport registerRestoreEnvFile(Path envFile) {
		this.envFile = envFile;
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
	public InternalCRIUSupport registerRestoreOptionsFile(Path optionsFile) {
		String optionsFilePath = optionsFile.toAbsolutePath().toString();
		this.optionsFile = "-Xoptionsfile=" + optionsFilePath; //$NON-NLS-1$

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
	public InternalCRIUSupport registerPostRestoreHook(Runnable hook) {
		return registerPostRestoreHook(hook, HookMode.SINGLE_THREAD_MODE, DEFAULT_SINGLE_THREAD_MODE_HOOK_PRIORITY);
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
	public InternalCRIUSupport registerPostRestoreHook(Runnable hook, HookMode mode, int priority)
			throws UnsupportedOperationException {
		return registerCheckpointHookHelper(hook, mode, priority, false);
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
	public InternalCRIUSupport registerPreCheckpointHook(Runnable hook) {
		return registerPreCheckpointHook(hook, HookMode.SINGLE_THREAD_MODE, DEFAULT_SINGLE_THREAD_MODE_HOOK_PRIORITY);
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
	public InternalCRIUSupport registerPreCheckpointHook(Runnable hook, HookMode mode, int priority)
			throws UnsupportedOperationException {
		return registerCheckpointHookHelper(hook, mode, priority, true);
	}

	private InternalCRIUSupport registerCheckpointHookHelper(Runnable hook, HookMode mode, int priority,
			boolean isPreCheckpoint) throws UnsupportedOperationException {
		if (hook != null) {
			if ((priority < LOWEST_USER_HOOK_PRIORITY) || (priority > HIGHEST_USER_HOOK_PRIORITY)) {
				throw new UnsupportedOperationException("The user hook priority must be between " //$NON-NLS-1$
						+ LOWEST_USER_HOOK_PRIORITY + " and " + HIGHEST_USER_HOOK_PRIORITY + "."); //$NON-NLS-1$ //$NON-NLS-2$
			}
			String threadModeMsg;
			if (HookMode.SINGLE_THREAD_MODE == mode) {
				threadModeMsg = "single-threaded mode"; //$NON-NLS-1$
			} else if (HookMode.CONCURRENT_MODE == mode) {
				threadModeMsg = "concurrent mode"; //$NON-NLS-1$
			} else {
				throw new UnsupportedOperationException("The hook mode must be SINGLE_THREAD_MODE or CONCURRENT_MODE."); //$NON-NLS-1$
			}

			String checkpointMsg = isPreCheckpoint ? "pre-checkpoint " : "post-restore hook "; //$NON-NLS-1$ //$NON-NLS-2$
			String commMsg = isPreCheckpoint ? "pre-checkpoint " : "post-restore hook " + threadModeMsg + " hook"; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
			String hookName = "User " + commMsg; //$NON-NLS-1$
			String exceptionMsg = "Exception thrown when running user " + commMsg; //$NON-NLS-1$
			if (isPreCheckpoint) {
				J9InternalCheckpointHookAPI.registerPreCheckpointHook(mode, priority, hookName, () -> {
					try {
						hook.run();
					} catch (Throwable t) {
						throw new JVMCheckpointException(exceptionMsg, 0, t);
					}
				});
			} else {
				J9InternalCheckpointHookAPI.registerPostRestoreHook(mode, priority, hookName, () -> {
					try {
						hook.run();
					} catch (Throwable t) {
						throw new JVMRestoreException(exceptionMsg, 0, t);
					}
				});
			}
		}
		return this;
	}

	@SuppressWarnings("restriction")
	private void registerRestoreEnvVariables() {
		if (this.envFile == null) {
			return;
		}

		if (unsafe == null) {
			initializeUnsafe();
		}

		J9InternalCheckpointHookAPI.registerPostRestoreHook(HookMode.SINGLE_THREAD_MODE, RESTORE_ENVIRONMENT_VARIABLES_PRIORITY,
				"Restore environment variables via env file: " + envFile, () -> { //$NON-NLS-1$
					if (!Files.exists(this.envFile)) {
						throw throwSetEnvException(new IllegalArgumentException(
								"Restore environment variable file " + envFile + " does not exist.")); //$NON-NLS-1$ //$NON-NLS-2$
					}

					String file = envFile.toAbsolutePath().toString();

					try (BufferedReader envFileReader = new BufferedReader(new FileReader(file))) {

						Class<?> processEnvironmentClass = Class.forName("java.lang.ProcessEnvironment"); //$NON-NLS-1$
						Class<?> stringEnvironmentClass = Class.forName("java.lang.ProcessEnvironment$StringEnvironment"); //$NON-NLS-1$
						Class<?> variableClass = Class.forName("java.lang.ProcessEnvironment$Variable"); //$NON-NLS-1$
						Class<?> valueClass = Class.forName("java.lang.ProcessEnvironment$Value"); //$NON-NLS-1$
						Field theUnmodifiableEnvironmentHandle = processEnvironmentClass.getDeclaredField("theUnmodifiableEnvironment"); //$NON-NLS-1$
						Field theEnvironmentHandle = processEnvironmentClass.getDeclaredField("theEnvironment"); //$NON-NLS-1$
						Constructor<?> newStringEnvironment = stringEnvironmentClass.getConstructor(new Class<?>[] { Map.class });
						Method variableValueOf = variableClass.getDeclaredMethod("valueOf", new Class<?>[] { String.class }); //$NON-NLS-1$
						Method valueValueOf = valueClass.getDeclaredMethod("valueOf", new Class<?>[] { String.class }); //$NON-NLS-1$
						theUnmodifiableEnvironmentHandle.setAccessible(true);
						theEnvironmentHandle.setAccessible(true);
						newStringEnvironment.setAccessible(true);
						variableValueOf.setAccessible(true);
						valueValueOf.setAccessible(true);

						@SuppressWarnings("unchecked")
						Map<String, String> oldTheUnmodifiableEnvironment = (Map<String, String>) theUnmodifiableEnvironmentHandle
								.get(processEnvironmentClass);
						@SuppressWarnings("unchecked")
						Map<Object, Object> theEnvironment = (Map<Object, Object>) theEnvironmentHandle
								.get(processEnvironmentClass);

						String entry = null;

						List<String> illegalKeys = new ArrayList<>(0);
						while ((entry = envFileReader.readLine()) != null) {
							// Keep the leading or trailing spaces of entry
							if (!entry.trim().isEmpty()) {
								// Only split into 2 (max) allow "=" to be contained in the value.
								String entrySplit[] = entry.split("=", 2); //$NON-NLS-1$
								if (entrySplit.length != 2) {
									throw new IllegalArgumentException(
											"Env File entry is not in the correct format: [envVarName]=[envVarVal]: " //$NON-NLS-1$
													+ entry);
								}

								String name = entrySplit[0];
								String newValue = entrySplit[1];
								String oldValue = oldTheUnmodifiableEnvironment.get(name);
								if (oldValue != null) {
									if (!Objects.equals(oldValue, newValue)) {
										illegalKeys.add(name);
									}
								} else {
									theEnvironment.put(variableValueOf.invoke(null, name), valueValueOf.invoke(null, newValue));
								}
							}
						}

						if (!illegalKeys.isEmpty()) {
							throw new IllegalArgumentException(String.format("Env file entry cannot modifiy pre-existing environment keys: %s", String.valueOf(illegalKeys))); //$NON-NLS-1$
						}

						@SuppressWarnings("unchecked")
						Map<String, String> newTheUnmodifiableEnvironment = (Map<String, String>) newStringEnvironment.newInstance(theEnvironment);

						unsafe.putObject(processEnvironmentClass, unsafe.staticFieldOffset(theUnmodifiableEnvironmentHandle),
								Collections.unmodifiableMap(newTheUnmodifiableEnvironment));

					} catch (Throwable t) {
						throw throwSetEnvException(t);
					}
				});
	}

	private static JVMRestoreException throwSetEnvException(Throwable cause) {
		throw new JVMRestoreException("Failed to setup new environment variables", 0, cause); //$NON-NLS-1$
	}

	private static void setRestoreJavaProperties() {
		String properties[] = getRestoreSystemProperites();

		if (properties != null) {
			for (int i = 0; i < properties.length; i += 2) {
				System.setProperty(properties[i], properties[i + 1]);
			}
		}
	}

	private static void clearInetAddressCache() {
		Field jniaa = AccessController.doPrivileged((PrivilegedAction<Field>) () -> {
			Field jniaaTmp = null;
			try {
				jniaaTmp = SharedSecrets.class.getDeclaredField("javaNetInetAddressAccess"); //$NON-NLS-1$
				jniaaTmp.setAccessible(true);
			} catch (NoSuchFieldException | SecurityException e) {
				// ignore exceptions
			}
			return jniaaTmp;
		});
		try {
			if ((jniaa != null) && (jniaa.get(null) != null)) {
				// InetAddress static initializer invokes SharedSecrets.setJavaNetInetAddressAccess().
				// There is no need to clear the cache if InetAddress hasn't been initialized yet.
				SharedSecrets.getJavaNetInetAddressAccess().clearInetAddressCache();
			}
		} catch (IllegalArgumentException | IllegalAccessException e) {
			// ignore exceptions
		}
	}

	/**
	 * Checkpoint the JVM. This operation will use the CRIU options set by the
	 * options setters.
	 *
	 * @throws UnsupportedOperationException if CRIU is not supported
	 *  or running in non-portable mode (only one checkpoint is allowed),
	 *  and we have already checkpointed once.
	 * @throws JVMCheckpointException        if a JVM error occurred before
	 *                                       checkpoint
	 * @throws SystemCheckpointException     if a System operation failed before checkpoint
	 * @throws SystemRestoreException        if a System operation failed during or after restore
	 * @throws JVMRestoreException           if an error occurred during or after
	 *                                       restore
	 */
	public synchronized void checkpointJVM() {
		if (isCRaCorCRIUSupportEnabledAndNativeLoaded()) {
			if (isCheckpointAllowed()) {
				/* Add env variables restore hook. */
				String envFilePath = null;
				if (envFile != null) {
					envFilePath = envFile.toAbsolutePath().toString();
					registerRestoreEnvVariables();
				}

				J9InternalCheckpointHookAPI.registerPostRestoreHook(HookMode.SINGLE_THREAD_MODE, RESTORE_CLEAR_INETADDRESS_CACHE_PRIORITY, "Clear InetAddress cache on restore", InternalCRIUSupport::clearInetAddressCache); //$NON-NLS-1$
				J9InternalCheckpointHookAPI.registerPostRestoreHook(HookMode.SINGLE_THREAD_MODE, RESTORE_ENVIRONMENT_VARIABLES_PRIORITY, "Restore system properties", InternalCRIUSupport::setRestoreJavaProperties); //$NON-NLS-1$

				/* Add option overrides. */
				Properties props = VM.internalGetProperties();

				String unprivilegedOpt = props.getProperty("openj9.internal.criu.unprivilegedMode"); //$NON-NLS-1$
				if (unprivilegedOpt != null) {
						setUnprivileged(Boolean.parseBoolean(unprivilegedOpt));
				}

				String tcpEstablishedOpt = props.getProperty("openj9.internal.criu.tcpEstablished"); //$NON-NLS-1$
				if (tcpEstablishedOpt != null) {
					setTCPEstablished(Boolean.parseBoolean(tcpEstablishedOpt));
				}

				String ghostFileLimitOpt = props.getProperty("openj9.internal.criu.ghostFileLimit"); //$NON-NLS-1$
				if (ghostFileLimitOpt != null) {
					try {
						setGhostFileLimit(Long.parseLong(ghostFileLimitOpt));
					} catch (NumberFormatException e) {
						System.err.println("Invalid value specified: `-Dopenj9.internal.criu.ghostFileLimit=" + ghostFileLimitOpt + "`."); //$NON-NLS-1$ //$NON-NLS-2$
					}
				}

				String logLevelOpt =  props.getProperty("openj9.internal.criu.logLevel"); //$NON-NLS-1$
				if (logLevelOpt != null) {
					try {
						setLogLevel(Integer.parseInt(logLevelOpt));
					} catch (NumberFormatException e) {
						System.err.println("Invalid value specified: `-Dopenj9.internal.criu.logLevel=" + logLevelOpt + "` ."); //$NON-NLS-1$ //$NON-NLS-2$
					}
				}

				String logFileOpt =  props.getProperty("openj9.internal.criu.logFile"); //$NON-NLS-1$
				if (logFileOpt != null) {
					setLogFile(logFileOpt);
				}

				/* Add security provider hooks. */
				SecurityProviders.registerResetCRIUState();
				SecurityProviders.registerRestoreSecurityProviders();

				J9InternalCheckpointHookAPI.runPreCheckpointHooksConcurrentThread();
				System.gc();
				checkpointJVMImpl(imageDir, leaveRunning, shellJob, extUnixSupport, logLevel, logFile, fileLocks,
						workDir, tcpEstablished, autoDedup, trackMemory, unprivileged, optionsFile, envFilePath, ghostFileLimit);
				J9InternalCheckpointHookAPI.runPostRestoreHooksConcurrentThread();
			} else {
				throw new UnsupportedOperationException(
						"Running in non-portable mode (only one checkpoint is allowed), and we have already checkpointed once"); //$NON-NLS-1$
			}
		} else {
			throw new UnsupportedOperationException("CRaC or CRIU support is not enabled"); //$NON-NLS-1$
		}
	}
}
