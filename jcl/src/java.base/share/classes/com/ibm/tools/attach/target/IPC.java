/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;

/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
import java.util.Objects;
import java.util.Properties;
import java.util.Random;

/**
 * Utility class for operating system calls
 */
public class IPC {

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


	private static Random randomGen; /* Cleanup. this is used by multiple threads */
	static  PrintStream logStream; /* cleanup.  Used by multiple threads */
	static volatile boolean loggingEnabled; /* set at initialization time and not changed */
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

	/*[PR Jazz 30075] setupSemaphore was re-doing what createDirectoryAndSemaphore (now called prepareCommonDirectory) did already */

	/**
	 * 
	 * @param ctrlDir Location of the control file
	 * @param SemaphoreName key used to identify the semaphore
	 * @return non-zero on failure
	 */
	native static int openSemaphore(String ctrlDir, String SemaphoreName);

	/**
	 * wait for a post on the semaphore for this VM. Use notify() to do the post
	 * @return 0 if success.
	 */
	static native int waitSemaphore();

	/**
	 * Open the semaphore, post to it, and close it
	 * @param numberOfTargets number of times to post to the semaphore
	 * @return 0 on success
	 */
	native static int notifyVm(String ctrlDir, String SemaphoreName,
			int numberOfTargets);

	/**
	 * Open the semaphore, decrement it without blocking to it, and close it
	 * @param numberOfTargets number of times to decrement to the semaphore
	 * @return 0 on success
	 */
	static native int cancelNotify(String ctrlDir, String SemaphoreName,
			int numberOfTargets);

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

	private static native int processExistsImpl(long pid);

	static void createFileWithPermissions(String path, int perms)
			throws IOException {
		int rc = createFileWithPermissionsImpl(path, perms);
		if (JNI_OK != rc) {
			throw new IOException(path);
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
			tmpDir = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("java.io.tmpdir"); //$NON-NLS-1$
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
		if (loggingEnabled ) {
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
		if (loggingEnabled ) {
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
		if (loggingEnabled ) {
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
		if (loggingEnabled ) {
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
		if (loggingEnabled ) {
			printLogMessage(string1+Integer.toString(int1)+string2+string3);
		}
	}

	private static void printMessageWithHeader(String msg, PrintStream log) {
		tracepoint(TRACEPOINT_STATUS_LOGGING, msg);
		printLogMessageHeader(log);
		log.println(msg);
	}

	/**
	 * Print the information about a throwable, including the exact class,
	 * message, and stack trace.
	 * @param msg User supplied message
	 * @param thrown throwable
	 * @note nothing is printed if logging is disabled
	 */
	public synchronized static void logMessage(String msg, Throwable thrown) {
		@SuppressWarnings("resource") /* this will be closed when the VM exits */
		PrintStream log = getLogStream();
		if (!Objects.isNull(log)) {
			printMessageWithHeader(msg, log);
			thrown.printStackTrace(log);
			log.flush();
		}
	}

	/**
	 * Print a message to the log file with time and thread information.
	 * Also send the raw message to a tracepoint.
	 * @param msg message to print
	 * @note no message is printed if logging is disabled
	 */
	private synchronized static void printLogMessage(final String msg) {
		@SuppressWarnings("resource") /* this will be closed when the VM exits */
		PrintStream log = getLogStream();
		if (!Objects.isNull(log)) {
			printMessageWithHeader(msg, log);
			log.flush();
		}
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
	}
	private static final syncObject accessorMutex = new syncObject();

	private static PrintStream getLogStream() {
		synchronized(accessorMutex) {
			return logStream;
		}
	}

	static void setLogStream(PrintStream log) {
		synchronized(accessorMutex) {
			IPC.logStream = log;
		}
	}

	static void setDefaultVmId(String id) { /* use this while until the real vmId is set, since they will usually be the same */
		defaultVmId = id;
	}

	public static void sendProperties(Properties props, OutputStream outStream)
			throws IOException {
		ByteArrayOutputStream propsBuffer = new ByteArrayOutputStream();
		props.store(propsBuffer, ""); //$NON-NLS-1$
		outStream.write(propsBuffer.toByteArray());
		outStream.write(0);
	}

	/**
	 * @param inStream  Source for the properties
	 * @param requireNull Indicates if the input message must be null-terminated (typically for socket input) or end of file (typically for re-parsing an already-received message) 
	 * @return properties object
	 * @throws IOException
	 */
	public static Properties receiveProperties(InputStream inStream, boolean requireNull)
			throws IOException {
		byte msgBuff[] = AttachmentConnection.streamReceiveBytes(inStream, 0, requireNull);
		Properties props = new Properties();
		props.load(new ByteArrayInputStream(msgBuff));
		return props;
	}

}
