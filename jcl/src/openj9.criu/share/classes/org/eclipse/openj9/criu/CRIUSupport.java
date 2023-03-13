/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.eclipse.openj9.criu;

import java.nio.file.Path;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
/*[IF JAVA_SPEC_VERSION == 8] */
import sun.misc.Unsafe;
/*[ELSE]
import jdk.internal.misc.Unsafe;
/*[ENDIF] JAVA_SPEC_VERSION == 8 */
import java.util.Objects;
import java.util.Properties;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import openj9.internal.criu.InternalCRIUSupport;

/**
 * CRIU Support API
 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
@SuppressWarnings({ "deprecation", "removal" })
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
public final class CRIUSupport {

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
			String envFile);

	private static native String[] getRestoreSystemProperites();

	static {
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

	private static boolean loadNativeLibrary() {
		if (!nativeLoaded) {
			try {
				AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
					System.loadLibrary("j9criu29"); //$NON-NLS-1$
					nativeLoaded = true;
					return null;
				});
			} catch (UnsatisfiedLinkError e) {
				errorMsg = e.getMessage();
				Properties internalProperties = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties();
				if (internalProperties.getProperty("enable.j9internal.checkpoint.hook.api.debug") != null) { //$NON-NLS-1$
					e.printStackTrace();
				}
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
		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(CRIU_DUMP_PERMISSION);
		}

		setImageDir(imageDir);
	}

	/**
	 * Queries if CRIU support is enabled and j9criu29 library has been loaded.
	 *
	 * @return TRUE if support is enabled and the library is loaded, FALSE otherwise
	 */
	public static boolean isCRIUSupportEnabled() {
		init();
		return (nativeLoaded && InternalCRIUSupport.isCRIUSupportEnabled());
	}

	/**
	 * Queries if CRIU Checkpoint is allowed.
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
		if (errorMsg == null) {
			if (InternalCRIUSupport.isCRIUSupportEnabled()) {
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
	private static final int RESTORE_ENVIRONMENT_VARIABLES_PRIORITY = 100;
	private static final int RESTORE_SYSTEM_PROPERTIES_PRIORITY = 100;
	private static final int USER_HOOKS_PRIORITY = 1;
	/* RESET_CRIUSEC_PRIORITY and RESTORE_SECURITY_PROVIDERS_PRIORITY need to
	 * be higher than any other JVM hook that may require security providers.
	 */
	static final int RESET_CRIUSEC_PRIORITY = 100;
	static final int RESTORE_SECURITY_PROVIDERS_PRIORITY = 100;

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
		Objects.requireNonNull(imageDir, "Image directory cannot be null"); //$NON-NLS-1$
		if (!Files.isDirectory(imageDir)) {
			throw new IllegalArgumentException("imageDir is not a valid directory"); //$NON-NLS-1$
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
	public CRIUSupport setLeaveRunning(boolean leaveRunning) {
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
	public CRIUSupport setShellJob(boolean shellJob) {
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
	public CRIUSupport setExtUnixSupport(boolean extUnixSupport) {
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
	public CRIUSupport setLogLevel(int logLevel) {
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
	public CRIUSupport setLogFile(String logFile) {
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
	public CRIUSupport setFileLocks(boolean fileLocks) {
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
	public CRIUSupport setTCPEstablished(boolean tcpEstablished) {
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
	public CRIUSupport setAutoDedup(boolean autoDedup) {
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
	public CRIUSupport setTrackMemory(boolean trackMemory) {
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
	public CRIUSupport setWorkDir(Path workDir) {
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
	public CRIUSupport setUnprivileged(boolean unprivileged) {
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
	 * @param envFile The file that contains the new environment variables to be
	 *                added
	 * @return this
	 */
	public CRIUSupport registerRestoreEnvFile(Path envFile) {
		this.envFile = envFile;
		return this;
	}

	/**
	 * Add new JVM options upon restore. The options will be specified in an options
	 * file with the form specified in https://www.eclipse.org/openj9/docs/xoptionsfile/
	 *
	 * TODO: The first iteration will only support -D options. Subsequent iterations will add more functionality.
	 *
	 * @param optionsFile The file that contains the new JVM options to be added on restore
	 *
	 * @return this
	 */
	public CRIUSupport registerRestoreOptionsFile(Path optionsFile) {
		String optionsFilePath = optionsFile.toAbsolutePath().toString();
		this.optionsFile = "-Xoptionsfile=" + optionsFilePath; //$NON-NLS-1$

		return this;
	}

	/**
	 * User hook that is run after restoring a checkpoint image.
	 *
	 * Hooks will be run in single threaded mode, no other application threads
	 * will be active. Users should avoid synchronization of objects that are not owned
	 * by the thread, terminally blocking operations and launching new threads in the hook.
	 *
	 * @param hook user hook
	 *
	 * @return this
	 *
	 * TODO: Additional JVM capabilities will be added to prevent certain deadlock scenarios
	 */
	public CRIUSupport registerPostRestoreHook(Runnable hook) {
		if (hook != null) {
			J9InternalCheckpointHookAPI.registerPostRestoreHook(USER_HOOKS_PRIORITY, "User post-restore hook", ()->{ //$NON-NLS-1$
				try {
					hook.run();
				} catch (Throwable t) {
					throw new JVMRestoreException("Exception thrown when running user post-restore hook", 0, t); //$NON-NLS-1$
				}
			});
		}
		return this;
	}

	/**
	 * User hook that is run before checkpointing the JVM.
	 *
	 * Hooks will be run in single threaded mode, no other application threads
	 * will be active. Users should avoid synchronization of objects that are not owned
	 * by the thread, terminally blocking operations and launching new threads in the hook.
	 *
	 * @param hook user hook
	 *
	 * @return this
	 *
	 * TODO: Additional JVM capabilities will be added to prevent certain deadlock scenarios
	 */
	public CRIUSupport registerPreCheckpointHook(Runnable hook) {
		if (hook != null) {
			J9InternalCheckpointHookAPI.registerPreCheckpointHook(USER_HOOKS_PRIORITY, "User pre-checkpoint hook", ()->{ //$NON-NLS-1$
				try {
					hook.run();
				} catch (Throwable t) {
					throw new JVMCheckpointException("Exception thrown when running user pre-checkpoint hook", 0, t); //$NON-NLS-1$
				}
			});
		}
		return this;
	}

	@SuppressWarnings("restriction")
	private void registerRestoreEnvVariables() {
		if (this.envFile == null) {
			return;
		}

		J9InternalCheckpointHookAPI.registerPostRestoreHook(RESTORE_ENVIRONMENT_VARIABLES_PRIORITY,
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

	/**
	 * Checkpoint the JVM. This operation will use the CRIU options set by the
	 * options setters.
	 *
	 * @throws UnsupportedOperationException if CRIU is not supported
	 *  or running in non-portable mode (only one checkpoint is allowed),
	 *  and we have already checkpointed once.
	 * @throws JVMCheckpointException        if a JVM error occurred before
	 *                                       checkpoint
	 * @throws SystemCheckpointException     if a CRIU operation failed
	 * @throws JVMRestoreException           if an error occurred during or after
	 *                                       restore
	 */
	public synchronized void checkpointJVM() {
		if (isCRIUSupportEnabled()) {
			if (InternalCRIUSupport.isCheckpointAllowed()) {
				/* Add env variables restore hook. */
				String envFilePath = null;
				if (envFile != null) {
					envFilePath = envFile.toAbsolutePath().toString();
					registerRestoreEnvVariables();
				}

				J9InternalCheckpointHookAPI.registerPostRestoreHook(RESTORE_ENVIRONMENT_VARIABLES_PRIORITY, "Restore system properties", CRIUSupport::setRestoreJavaProperties);

				/* Add security provider hooks. */
				SecurityProviders.registerResetCRIUState();
				SecurityProviders.registerRestoreSecurityProviders();

				try {
					checkpointJVMImpl(imageDir, leaveRunning, shellJob, extUnixSupport, logLevel, logFile, fileLocks,
							workDir, tcpEstablished, autoDedup, trackMemory, unprivileged, optionsFile, envFilePath);
				} catch (UnsatisfiedLinkError ule) {
					errorMsg = ule.getMessage();
					throw new InternalError("There is a problem with libj9criu in the JDK"); //$NON-NLS-1$
				}
			} else {
				throw new UnsupportedOperationException(
						"Running in non-portable mode (only one checkpoint is allowed), and we have already checkpointed once"); //$NON-NLS-1$
			}
		} else {
			throw new UnsupportedOperationException("CRIU support is not enabled"); //$NON-NLS-1$
		}
	}
}
