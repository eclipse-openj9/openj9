/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;

/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.security.SecureRandom;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Paths;
import java.nio.file.attribute.PosixFilePermission;
import java.util.EnumSet;
import java.util.Set;
import java.util.Objects;
import java.util.Properties;
import java.util.Random;

import static java.nio.file.attribute.PosixFilePermission.GROUP_READ;
import static java.nio.file.attribute.PosixFilePermission.GROUP_WRITE;
import static java.nio.file.attribute.PosixFilePermission.OTHERS_READ;
import static java.nio.file.attribute.PosixFilePermission.OTHERS_WRITE;
import static openj9.tools.attach.diagnostics.base.DiagnosticsInfo.OPENJ9_DIAGNOSTICS_PREFIX;

/**
 * Utility class for operating system calls
 */
public class IPC {

	private static final String JAVA_IO_TMPDIR = "java.io.tmpdir"; //$NON-NLS-1$
	/**
	 * Successful return code from natives.
	 */
	public static final int JNI_OK = 0;
	static final int TRACEPOINT_STATUS_NORMAL = 0; /* use for debugging or to record normal events. Use message to distinguish errors  */
	static final int TRACEPOINT_STATUS_LOGGING = 1; /* used for the IPC.logMessage methods */
	static final int TRACEPOINT_STATUS_ERROR = -1; /* use only for debugging. Use message to distinguish errors  */
	static final int TRACEPOINT_STATUS_OOM_DURING_WAIT = -2;
	static final int TRACEPOINT_STATUS_OOM_DURING_TERMINATE = -3;
	static final String LOCAL_CONNECTOR_ADDRESS = "com.sun.management.jmxremote.localConnectorAddress"; //$NON-NLS-1$
	private static final EnumSet<PosixFilePermission> NON_OWNER_READ_WRITE =
				EnumSet.of(GROUP_READ, GROUP_WRITE, OTHERS_READ, OTHERS_WRITE);
	static final int LOGGING_UNKNOWN = 0;
	static final int LOGGING_DISABLED = 1;
	static final int LOGGING_ENABLED = 2;
	/**
	 * Prefixes and names for Diagnostics properties
	 */
	public static final String INCOMPATIBLE_JAVA_VERSION = "OPENJ9_INCOMPATIBLE_JAVA_VERSION"; //$NON-NLS-1$
	public final static String PROPERTY_DIAGNOSTICS_ERROR = OPENJ9_DIAGNOSTICS_PREFIX + "error"; //$NON-NLS-1$
	public final static String PROPERTY_DIAGNOSTICS_ERRORTYPE = OPENJ9_DIAGNOSTICS_PREFIX + "errortype"; //$NON-NLS-1$
	public final static String PROPERTY_DIAGNOSTICS_ERRORMSG = OPENJ9_DIAGNOSTICS_PREFIX + "errormsg"; //$NON-NLS-1$
	/**
	 * True if operating system is Windows.
	 */
	public static boolean isWindows = false;

	private static Random randomGen; /* Cleanup. this is used by multiple threads */
	
	/* loggingStatus may be seen to be LOGGING_ENABLED before logStream is initialized,
	 * so use logStream inside a synchronized (IPC.accessorMutex) block.
	 */
	static  PrintStream logStream;
	
	/* loggingStatus is set at initialization time to LOGGING_DISABLED or LOGGING_ENABLED 
	 * and not changed thereafter.
	 * This can be safely tested by any thread against LOGGING_DISABLED.  If it is not equal, then 
	 * isLoggingEnabled() will check the actual status in a thread-safe manner.
	 */
	static int loggingStatus = LOGGING_UNKNOWN; /* set at initialization time and not changed */
	static String defaultVmId;

	/**
	 * Set the permissions on a file or directory
	 * @param path path to a file or directory
	 * @param perms POSIX-style permissions, including owner, group, world read/write/execute bits, sticky, suid, sgid bits
	 * @return Actual permissions, which may differ from those requested, particularly on Win32
	 */
	static native int chmod(String path, int perms);

	/**
	 * change the ownership of a file.  Can be called only if this process is owned by root.
	 * @param path path to the file
	 * @param uid effective userid
	 * @return result of chown() operation.
	 */
	static native int chownFileToTargetUid(String path, long uid);
	
	/**
	 * Create a directory and set the permissions on it.
	 * @param absolutePath directory
	 * @param perms request Posix-style permissions
	 * @return Actual permissions
	 * @throws IOException if mkdir fails
	 */
	static int mkdirWithPermissions(String absolutePath, int perms)
			throws IOException {
		int status = mkdirWithPermissionsImpl(absolutePath, perms);
		if (JNI_OK != status) {
			throw new IOException(absolutePath);
		}
		return status;
	}

	static native int mkdirWithPermissionsImpl(String absolutePath, int perms);

	/**
	 * Ensure that a file or directory is not readable or writable by others
	 * and is owned by the current user.
	 * @param filePath File or directory path
	 * @throws IOException if the access or ownership is wrong
	 */
	public static void checkOwnerAccessOnly(String filePath) throws IOException {
		final long myUid = getUid();
		/* Ensure file is owned by current user, or current user is root */
		final long fileOwner = CommonDirectory.getFileOwner(filePath);
		if ((0 != myUid) && (fileOwner != myUid)) {
			logMessage("Wrong permissions or ownership for ", filePath); //$NON-NLS-1$
			/*[MSG "K0803", "File {0} is owned by {1}, should be owned by current user"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0803", filePath, Long.valueOf(fileOwner)));//$NON-NLS-1$
		}
		if (!isWindows) {
			try {
				/* ensure that the directory is not readable or writable by others */
				Set<PosixFilePermission> actualPermissions =
						Files.getPosixFilePermissions(Paths.get(filePath), LinkOption.NOFOLLOW_LINKS);
				actualPermissions.retainAll(NON_OWNER_READ_WRITE);
				if (!actualPermissions.isEmpty()) {
					final String permissionString = Files.getPosixFilePermissions(Paths.get(filePath), LinkOption.NOFOLLOW_LINKS).toString();
					logMessage("Wrong permissions: " +permissionString + " for ", filePath); //$NON-NLS-1$ //$NON-NLS-2$
					/*[MSG "K0805", "{0} has permissions {1}, should have owner access only"]*/
					throw new IOException(com.ibm.oti.util.Msg.getString("K0805", filePath, permissionString));//$NON-NLS-1$
				}
			} catch (UnsupportedOperationException e) {
				String osName = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("os.name"); //$NON-NLS-1$
				if ((null != osName) && !osName.startsWith("Windows")) { //$NON-NLS-1$
					/*[MSG "K0806", "Cannot verify permissions {0}"]*/
					throw new IOException(com.ibm.oti.util.Msg.getString("K0806", filePath), e); //$NON-NLS-1$
				}
			}
		}
	}

	/**
	 * Open a semaphore.  On Windows, use the global namespace if requested.
	 * @param ctrlDir Location of the control file
	 * @param SemaphoreName key used to identify the semaphore
	 * @return non-zero on failure
	 */
	native static int openSemaphore(String ctrlDir, String SemaphoreName, boolean global);

	/**
	 * wait for a post on the semaphore for this VM. Use notify() to do the post
	 * @return 0 if success.
	 */
	static native int waitSemaphore();

	/**
	 * Open the semaphore, post to it, and close it
	 * @param ctrlDir directory containing the semaphore
	 * @param semaphoreName name of the semaphore
	 * @param numberOfTargets number of times to post to the semaphore
	 * @param global open the semaphore in the global namespace (Windows only)
	 * @return 0 on success
	 */
	native static int notifyVm(String ctrlDir, String semaphoreName,
			int numberOfTargets, boolean global);

	/**
	 * Open the semaphore, decrement it without blocking to it, and close it
	 * @param ctrlDir directory containing the semaphore
	 * @param semaphoreName name of the semaphore
	 * @param numberOfTargets number of times to post to the semaphore
	 * @param global open the semaphore in the global namespace (Windows only)
	 * @return 0 on success
	 */
	static native int cancelNotify(String ctrlDir, String semaphoreName,
			int numberOfTargets, boolean global);

	/**
	 * close but do not destroy this VM's notification semaphore.
	 * This has no effect on non-Windows OSes
	 */
	static native void closeSemaphore();

	/**
	 * destroy the semaphore if possible.  This may or may not interrupt a waitSemaphore()
	 */
	static native int destroySemaphore();

	/**
	 * @return OS UID (numeric user ID)
	 * @note Returns 0 on Windows.
	 */
	 /* CMVC 161414 - PIDs and UIDs are long */
	public static native long getUid();

	/**
	 * This crawls the z/OS control blocks to determine if the current process is using the default UID.
	 * This handles both job- and task-level security.
	 * For information on the control blocks, see the z/OS V1R9.0 MVS Data Areas documentation.
	 * @return JNI_TRUE if the process is running on z/OS and is using the default UID
	 * @note A batch process runs as the default UID if it has no USS segment.
	 */
	static native boolean isUsingDefaultUid();
	
	/**
	 * @return OS process ID
	 */
	 /* CMVC 161414 - PIDs and UIDs are long */
	static native long getProcessId();

	/**
	 * @param pid process ID
	 * @return true if process exists 
	 * Error conditions are ignored.
	 */
	public static boolean processExists(long pid) {
		int rc = processExistsImpl(pid);
		return (rc > 0);
	}
	
	/**
	 * Check if attach API initialization has enabled logging.
	 * Logging is enabled or disabled once, and remains enabled or disabled for the duration of the process.
	 * @return true if logging is definitely enabled, false if is not initialized or is disabled.
	 */
	private static boolean isLoggingEnabled() {
		boolean result = false;
		if (LOGGING_DISABLED == loggingStatus) {
			result = false;
		} else if (LOGGING_ENABLED == loggingStatus) {
			result = true;
		} else synchronized (accessorMutex) {
			/* 
			 * We may be initializing.  If so, wait until initialization is complete.  
			 * Otherwise, assume logging is disabled.
			 */
			result = (LOGGING_ENABLED == loggingStatus);			
		}
		return result;
	}

	private static native int processExistsImpl(long pid);

	/**
	 * Create a new file with specified the permissions (to override umask) and close it.
	 * If the file exists, delete it.
	 * @param path file system path
	 * @param mode file access permissions (posix format) for the new file
	 * @throws IOException if the file exists and cannot be removed, or 
	 * a new file cannot be created with the specified permission
	 */
	static void createNewFileWithPermissions(File theFile, int perms) throws IOException {
		final String filePathString = theFile.getAbsolutePath();
		if (theFile.exists()) {
			IPC.logMessage("Found existing file ", filePathString); //$NON-NLS-1$
			if (!theFile.delete()) {
				IPC.logMessage("Cannot delete existing file ", filePathString); //$NON-NLS-1$
				/*[MSG "K0807", "Cannot delete file {0}"]*/
				throw (new IOException(com.ibm.oti.util.Msg.getString("K0807", filePathString))); //$NON-NLS-1$
			}
		}
		int rc = createFileWithPermissionsImpl(theFile.getAbsolutePath(), perms);
		if (JNI_OK != rc) {
			IPC.logMessage("Cannot create new file ", filePathString); //$NON-NLS-1$
			/*[MSG "K0808", "Cannot create new file {0}"]*/
			throw (new IOException(com.ibm.oti.util.Msg.getString("K0808", filePathString))); //$NON-NLS-1$
		}
	}

	static private native int createFileWithPermissionsImpl(String path,
			int perms);

	/**
	 * Get the system temporary directory: /tmp on all but Windows, C:\Documents
	 * and Settings\<userid>\Local Settings\Temp
	 * 
	 * @return directory path name. If the result is null, use the
	 *         java.io.tmpdir property.
	 */
	static String getTmpDir() {
		String tmpDir = getTempDirImpl();
		if (null == tmpDir) {
			IPC.logMessage("Could not get system temporary directory. Trying "+JAVA_IO_TMPDIR); //$NON-NLS-1$
			tmpDir = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty(JAVA_IO_TMPDIR); //$NON-NLS-1$
		}
		return tmpDir;
	}

	/**
	 * Get the path to the system temporary directory in modified UTF-8
	 * @return
	 */
	static private native String getTempDirImpl();

	/**
	 * Create a Random object if it does not exist. 
	 * generate a random number based on the current time.
	 * @return random int
	 */
	public static int getRandomNumber() {
		synchronized (accessorMutex) {
			if (null == randomGen) {
				randomGen = new Random(System.nanoTime());
			}
			return randomGen.nextInt();
		}
	}
	
	/**
	 * Create a truly random string of hexadecimal characters with 64 bits of entropy.
	 * This may be slow.
	 * @return hexadecimal string
	 */
	public static String getRandomString() {
		SecureRandom randomGenerator = new SecureRandom();
		return Long.toHexString(randomGenerator.nextLong());
	}

	/**
	 * generate a level 1 tracepoint with a status code and message.
	 *
	 * @param statusCode numeric status value.  Use the values defined above.
	 * @param message descriptive message. May be null.
	 */
	static native void tracepoint(int statusCode, String message);
	/**
	 * Print a message prefixed with the current time to the log file.
	 * Calls to this method on the success path for normal VM operation are already protected 
	 * by a test on loggingEnabled for performance reasons.
	 * @param msg message to print
	 */
	public static void logMessage(final String msg) {
		if (isLoggingEnabled()) {
			printLogMessage(msg);
		}
	}

	/**
	 * convenience method for messages with two strings.  Avoids creating extra strings for the normal (logging disabled) case.
	 * Calls to this method on the success path for normal VM operation are already protected 
	 * by a test on loggingEnabled for performance reasons.
	 * @param string1 first argument
	 * @param string2 concatenated to second argument
	 */
	public static void logMessage(String string1, String string2) {
		if (isLoggingEnabled()) {
			printLogMessage(string1+string2);
		}
	}

	/**
	 * convenience method for messages with an integer.  Avoids creating extra strings for the normal (logging disabled) case.
	 * Calls to this method on the success path for normal VM operation are already protected 
	 * by a test on loggingEnabled for performance reasons.
	 * @param string1 first argument
	 * @param int1 concatenated to second argument
	 */
	public static void logMessage(String string1, int int1) {
		if (isLoggingEnabled()) {
			printLogMessage(string1+Integer.toString(int1));
		}
	}

	/**
	 * convenience method for messages with a string, then an integer then a string.  Avoids creating extra strings for the normal (logging disabled) case.
	 * Calls to this method on the success path for normal VM operation are already protected 
	 * by a test on loggingEnabled for performance reasons.
	 * @param string1 first string in the message
	 * @param int1 concatenated to second argument
	 * @param string2 second string
	 */
	public static void logMessage(String string1, int int1, String string2) {
		if (isLoggingEnabled()) {
			printLogMessage(string1+Integer.toString(int1)+string2);
		}
	}

	/**
	 * convenience method for messages with an integer then two strings.  Avoids creating extra strings for the normal (logging disabled) case.
	 * Calls to this method on the success path for normal VM operation are already protected 
	 * by a test on loggingEnabled for performance reasons.
	 * @param string1 first string in the message
	 * @param int1 concatenated to second argument
	 * @param string2 second string
	 * @param string3 third string
	 */
	public static void logMessage(String string1, int int1, String string2, String string3) {
		if (isLoggingEnabled()) {
			printLogMessage(string1+Integer.toString(int1)+string2+string3);
		}
	}

	/**
	 * Print the information about a throwable, including the exact class,
	 * message, and stack trace.
	 * @param msg User supplied message
	 * @param thrown Throwable object or null
	 * @note nothing is printed if logging is disabled
	 */
	public static void logMessage(String msg, Throwable thrown) {
		synchronized (accessorMutex) {
			if (isLoggingEnabled()) {
				printMessageWithHeader(msg, logStream);
				if (null != thrown) {
					thrown.printStackTrace(logStream);
				}
				logStream.flush();
			}
		}
	}

	/**
	 * Print a message to the log file with time and thread information.
	 * Also send the raw message to a tracepoint.
	 * Call this only if isLoggingEnabled() has returned true.
	 * @param msg message to print
	 * @note no message is printed if logging is disabled
	 */
	private static void printLogMessage(final String msg) {
		synchronized (accessorMutex) {
			if (!Objects.isNull(logStream)) {
				printMessageWithHeader(msg, logStream);
				logStream.flush();
			}
		}
	}

	/**
	 * Print a message to the logging stream with metadata.
	 * This must be called in a synchronized block.
	 * @param msg Message to print
	 * @param log output stream.
	 */
	static void printMessageWithHeader(String msg, PrintStream log) {
		tracepoint(TRACEPOINT_STATUS_LOGGING, msg);
		printLogMessageHeader(log);
		log.println(msg);
	}

	/**
	 * Print the time, virtual machine ID, and thread ID to the log stream.
	 * @param log output stream
	 * @note log must be non-null
	 */
	private static void printLogMessageHeader(PrintStream log) {
		long currentTime = System.currentTimeMillis();
		log.print(currentTime);
		log.print(" "); //$NON-NLS-1$
		String id = AttachHandler.getVmId();
		if (0 == id.length()) {
			id = defaultVmId;
		}
		log.print(id);
		log.print(": "); //$NON-NLS-1$
		log.print(Thread.currentThread().getId());
		log.print(" ["); //$NON-NLS-1$
		log.print(Thread.currentThread().getName());
		log.print("]: "); //$NON-NLS-1$
	}
	
	static final class syncObject {
		/* empty */
	}
	/**
	 * For mutual exclusion to IPC objects.
	 */
	public static final syncObject accessorMutex = new syncObject();

	static void setDefaultVmId(String id) { /* use this while until the real vmId is set, since they will usually be the same */
		defaultVmId = id;
	}

	/**
	 * Send a properties file as a stream of bytes
	 * 
	 * @param props     Properties file
	 * @param outStream destination of the bytes
	 * @throws IOException on communication error
	 */
	public static void sendProperties(Properties props, OutputStream outStream)
			throws IOException {
		ByteArrayOutputStream propsBuffer = new ByteArrayOutputStream();
		props.store(propsBuffer, ""); //$NON-NLS-1$
		outStream.write(propsBuffer.toByteArray());
		outStream.write(0);
	}

	/**
	 * @param inStream    Source for the properties
	 * @param requireNull Indicates if the input message must be null-terminated
	 *                    (typically for socket input) or end of file (typically for
	 *                    re-parsing an already-received message)
	 * @return properties object
	 * @throws IOException on communication error
	 */
	public static Properties receiveProperties(InputStream inStream, boolean requireNull)
			throws IOException {
		byte msgBuff[] = AttachmentConnection.streamReceiveBytes(inStream, 0, requireNull);
		if (IPC.isLoggingEnabled()) {
			String propsString = new String(msgBuff, StandardCharsets.UTF_8);
			IPC.logMessage("Received properties file:", propsString); //$NON-NLS-1$
		}
		Properties props = new Properties();
		props.load(new ByteArrayInputStream(msgBuff));
		return props;
	}

}
