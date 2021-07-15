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

	public CRIUSupport() {}

	/**
	 * Queries if CRIU support is enabled.
	 * 
	 * @return TRUE is support is enabled, FALSE otherwise
	 */
	public static boolean isCRIUSupportEnabled() {
		return criuSupportEnabled;
	}

	private String imagesDir = null;
	private boolean leaveRunning = false;
	private boolean shellJob = true;
	private boolean extUnixSupport = true;
	private int logLevel = 0;
	private String logFile = null;
	private boolean fileLocks = false;
	private String workDir = null;

	public CRIUSupport setImagesDir(Path imagesDir) throws SecurityException {
		if (imagesDir != null) {
			String dir = imagesDir.toAbsolutePath().toString();

			SecurityManager manager = System.getSecurityManager();
			if (manager != null) {
				manager.checkPermission(CRIU_DUMP_PERMISSION);
				manager.checkWrite(dir);
			}
			this.imagesDir = dir;
		}
		return this;
	}

	public CRIUSupport setLeaveRunning(boolean leaveRunning) {
		this.leaveRunning = leaveRunning;
		return this;
	}

	public CRIUSupport setShellJob(boolean shellJob) {
		this.shellJob = shellJob;
		return this;
	}

	public CRIUSupport setExtUnixSupport(boolean extUnixSupport) {
		this.extUnixSupport = extUnixSupport;
		return this;
	}

	public CRIUSupport setLogLevel(int logLevel) {
		if (logLevel > 0) {
			this.logLevel = logLevel;
		}
		return this;
	}

	public CRIUSupport setLogFile(String logFile) {
		if (logFile != null && !logFile.contains("/")) {
			this.logFile = logFile;
		}
		return this;
	}

	public CRIUSupport setFileLocks(boolean fileLocks) {
		this.fileLocks = fileLocks;
		return this;
	}

	public CRIUSupport setWorkDir(Path workDir) throws SecurityException {
		if (workDir != null) {
			String dir = workDir.toAbsolutePath().toString();

			SecurityManager manager = System.getSecurityManager();
			if (manager != null) {
				manager.checkPermission(CRIU_DUMP_PERMISSION);
				manager.checkWrite(dir);
			}
			this.workDir = dir;
		}
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
	 */
	public CRIUResult checkPointJVM() throws NullPointerException {
		CRIUResult ret = new CRIUResult(CRIUResultType.UNSUPPORTED_OPERATION, null);

		if (null == imagesDir) {
			throw new NullPointerException("Images directory needs to be set with setImagesDir(Path imagesDir)");
		} else {
			if (isCRIUSupportEnabled()) {
				ret = checkpointJVMImpl();
			}
		}

		return ret;
	}
}
