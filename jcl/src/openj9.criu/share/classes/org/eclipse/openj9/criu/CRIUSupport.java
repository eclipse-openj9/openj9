/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package org.eclipse.openj9.criu;

import java.nio.file.Path;
import java.util.Objects;
import java.io.File;

/**
 * CRIU Support API
 */
public final class CRIUSupport {
	
	/**
	 * CRIU operation return type
	 */
	public static enum CRIUResultType {
		/**
		 * The operation was successful.
		 */
		SUCCESS,
		/**
		 * The requested operation is unsupported.
		 */
		UNSUPPORTED_OPERATION,
		/**
		 * The arguments provided for the operation are invalid.
		 */
		INVALID_ARGUMENTS,
		/**
		 * A system failure occurred when attempting to checkpoint the JVM.
		 */
		SYSTEM_CHECKPOINT_FAILURE,
		/**
		 * A JVM failure occurred when attempting to checkpoint the JVM.
		 */
		JVM_CHECKPOINT_FAILURE,
		/**
		 * A non-fatal JVM failure occurred when attempting to restore the JVM.
		 */
		JVM_RESTORE_FAILURE;
	}
	
	/**
	 * CRIU operation result. If there is a system failure the system error code will be set.
	 * If there is a JVM failure the throwable will be set.
	 *
	 */
	public final static class CRIUResult {
		private final CRIUResultType type;

		private final Throwable throwable;

		CRIUResult(CRIUResultType type, Throwable throwable) {
			this.type = type;
			this.throwable = throwable;
		}

		/**
		 * Returns CRIUResultType value
		 * 
		 * @return CRIUResultType
		 */
		public CRIUResultType getType() {
			return this.type;
		}

		/**
		 * Returns Java error. This will never be set if the CRIUResultType is SUCCESS
		 * 
		 * @return Java error
		 */
		public Throwable getThrowable() {
			return this.throwable;
		}
	}

	private static final CRIUDumpPermission CRIU_DUMP_PERMISSION = new CRIUDumpPermission();

	private static final boolean criuSupportEnabled = isCRIUSupportEnabledImpl();

	private static native boolean isCRIUSupportEnabledImpl();
	
	private static native boolean isCheckpointAllowed();

	private native CRIUResult checkpointJVMImpl();

	public CRIUSupport() {
		shellJob = true;
		extUnixSupport = true;
	}

	/**
	 * Queries if CRIU support is enabled.
	 * 
	 * @return TRUE is support is enabled, FALSE otherwise
	 */
	public static boolean isCRIUSupportEnabled() {
		return criuSupportEnabled;
	}

	private String imagesDir;
	private boolean leaveRunning;
	private boolean shellJob;
	private boolean extUnixSupport;
	private int logLevel;
	private String logFile;
	private boolean fileLocks;
	private String workDir;

	/**
	 * Sets the directory that will hold the images upon dump. Corresponds to criu_set_images_dir_fd.
	 * This must be set before calling {@link #checkPointJVM()}.
	 * 
	 * @param imagesDir the directory as a java.nio.file.Path
	 * @return this
	 * @throws NullPointerException if imagesDir is null.
	 * @throws SecurityException if no permission to access imagesDir.
	 */
	public CRIUSupport setImagesDir(Path imagesDir) {
		Objects.requireNonNull(imagesDir, "Path cannot be null");
		String dir = imagesDir.toAbsolutePath().toString();

		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(CRIU_DUMP_PERMISSION);
			manager.checkWrite(dir);
		}
		this.imagesDir = dir;
		return this;
	}

	/**
	 * Controls whether processes are left running after dump. Corresponds to criu_set_leave_running.
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
	 * Controls ability to dump shell jobs. Corresponds to criu_set_shell_job.
	 * Default: true
	 * 
	 * @param shellJob
	 * @return this
	 */
	public CRIUSupport setShellJob(boolean shellJob) {
		this.shellJob = shellJob;
		return this;
	}

	/**
	 * Controls whether to dump only one end of a unix socket pair. Corresponds to criu_set_ext_unix_sk.
	 * Default: true
	 * 
	 * @param extUnixSupport
	 * @return this
	 */
	public CRIUSupport setExtUnixSupport(boolean extUnixSupport) {
		this.extUnixSupport = extUnixSupport;
		return this;
	}

	/**
	 * Sets the verbosity of log output. Corresponds to criu_set_log_level.
	 * Default: 2 (errors + warnings)
	 * 
	 * @param logLevel verbosity from 1 to 4 inclusive
	 * @return this
	 * @throws IllegalArgumentException if logLevel is not valid.
	 */
	public CRIUSupport setLogLevel(int logLevel) {
		if (logLevel > 0 && logLevel <= 4) {
			this.logLevel = logLevel;
		} else {
			throw new IllegalArgumentException("Log level must be 1 to 4 inclusive");
		}
		return this;
	}

	/**
	 * Write log output to logFile. Corresponds to criu_set_log_file.
	 * Default: criu.log
	 * 
	 * @param logFile name of the file to write log output to. The path to the file can be set with {@link #setWorkDir(Path)}.
	 * @return this
	 * @throws IllegalArgumentException if logFile is null or a path.
	 */
	public CRIUSupport setLogFile(String logFile) {
		if (logFile != null && !logFile.contains(File.separator)) {
			this.logFile = logFile;
		} else {
			throw new IllegalArgumentException("Log file must not be null and not be a path");
		}
		return this;
	}

	/**
	 * Controls whether to dump file locks. Corresponds to criu_set_file_locks.
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
	 * Sets the directory where non-image files are stored (e.g. logs). Corresponds to criu_set_work_dir_fd.
	 * Default: same as path set by {@link #setImagesDir(Path)}.
	 * 
	 * @param workDir the directory as a java.nio.file.Path
	 * @return this
	 * @throws NullPointerException if workDir is null.
	 * @throws SecurityException if no permission to access workDir.
	 */
	public CRIUSupport setWorkDir(Path workDir) {
		Objects.requireNonNull(workDir, "Path cannot be null");
		String dir = workDir.toAbsolutePath().toString();

		SecurityManager manager = System.getSecurityManager();
		if (manager != null) {
			manager.checkPermission(CRIU_DUMP_PERMISSION);
			manager.checkWrite(dir);
		}
		this.workDir = dir;
		return this;
	}

	/**
	 * Checkpoint the JVM. A security manager check is done for org.eclipse.openj9.criu.CRIUDumpPermission
	 * as well as a java.io.FilePermission check to see if checkPointDir is writable.
	 * 
	 * This operation will use the CRIU options set by the options setters.
	 * 
	 * All errors will be stored in the throwable field of CRIUResult.
	 * 
	 * @return return CRIUResult
	 * @throws NullPointerException if imagesDir has not been set with {@link #setImagesDir(Path)}.
	 */
	public CRIUResult checkPointJVM() {
		CRIUResult ret;

		Objects.requireNonNull(imagesDir, "Images directory needs to be set with setImagesDir(Path imagesDir)");
		if (isCRIUSupportEnabled()) {
			ret = checkpointJVMImpl();
		} else {
			ret = new CRIUResult(CRIUResultType.UNSUPPORTED_OPERATION, null);
		}

		return ret;
	}
}
