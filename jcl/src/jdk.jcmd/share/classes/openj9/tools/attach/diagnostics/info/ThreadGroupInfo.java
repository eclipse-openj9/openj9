/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package openj9.tools.attach.diagnostics.info;

import static com.ibm.oti.util.Msg.getString;
import java.io.IOException;
import java.lang.management.LockInfo;
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.Properties;

import com.ibm.tools.attach.target.IPC;
import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticsInfo;

/**
 * Class for encoding and decoding stack traces for transmission via attach API.
 */
public class ThreadGroupInfo implements DiagnosticsInfo {

	private static final String CURRENT_GROUPINFO_VERSION = "0"; //$NON-NLS-1$
	private static final String TIME_FORMAT_STRING = "\t%s time: %d ns%n"; //$NON-NLS-1$
	static final String THREAD_GROUP_PREFIX = "threadgroup."; //$NON-NLS-1$
	static final String CPU_TIME = "cpu_time"; //$NON-NLS-1$
	static final String USER_TIME = "user_time"; //$NON-NLS-1$
	private static final String THREAD_GROUP_VERSION = THREAD_GROUP_PREFIX + "version"; //$NON-NLS-1$
	private static final String NUM_THREADS = THREAD_GROUP_PREFIX + "num_threads"; //$NON-NLS-1$

	private final int numThreads;
	private final JvmThreadInfo[] myThreads;
	private final String groupInfoVersion;
	private final DiagnosticProperties myProps;
	private final String javaInfo;
	private boolean addSynchronizers;

	/**
	 * Create an object from received properties.
	 * 
	 * @param props Properties receive from the target
	 * @param printSynchronizers add ownable synchronizers to output
	 * @throws IOException if properties file is corrupt
	 */
	public ThreadGroupInfo(Properties props, boolean printSynchronizers) throws IOException {
		addSynchronizers = printSynchronizers;
		try {
			myProps = new DiagnosticProperties(props);
			if (myProps.containsField(IPC.PROPERTY_DIAGNOSTICS_ERROR)
					&& myProps.getBoolean(IPC.PROPERTY_DIAGNOSTICS_ERROR)) {
				String targetMsg = myProps.getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORMSG);
				String msg = myProps.getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORTYPE);
				if ((null != targetMsg) && !targetMsg.isEmpty()) {
					msg = msg + ": " + targetMsg; //$NON-NLS-1$
				}
				throw new IOException(msg);
			}
			if (!myProps.containsField(NUM_THREADS)) {
				throw new IOException(IPC.INCOMPATIBLE_JAVA_VERSION);
			}
			numThreads = myProps.getInt(NUM_THREADS);
			groupInfoVersion = props.getProperty(THREAD_GROUP_VERSION);
			if (!checkVersion(groupInfoVersion)) {
				/*[MSG "K080A", "Incompatible target VM uses protocol version {0}"] */
				throw new IOException(getString("K080A", groupInfoVersion)); //$NON-NLS-1$
			}
			javaInfo = props.getProperty(DiagnosticsInfo.JAVA_INFO);

			if (numThreads <= 0) {
				throw new UnsupportedOperationException("Cannot determine number of threads"); //$NON-NLS-1$
			}
			myThreads = new JvmThreadInfo[numThreads];
			for (int i = 0; i < numThreads; ++i) {
				myThreads[i] = new JvmThreadInfo(myProps, i);
			}
		} catch (NumberFormatException e) {
			throw new IOException(e);
		}
	}

	/**
	 * Acquire stack traces for all running threads.
	 * 
	 * @return Properties objects thread information
	 */
	public static DiagnosticProperties getThreadInfoProperties() {
		ThreadMXBean threadBean = ManagementFactory.getThreadMXBean();
		ThreadInfo infos[] = threadBean.dumpAllThreads(true, true);
		DiagnosticProperties props = getThreadInfoPropertiesImpl(threadBean, infos);
		return props;
	}

	/**
	 * Acquire stack traces for selected threads.
	 * 
	 * @param threadIds list of threads to query
	 * @return Properties objects thread information
	 * @note some entries my be null if a specified thread no longer exists
	 */
	public static DiagnosticProperties getThreadInfoProperties(long[] threadIds) {
		ThreadMXBean threadBean = ManagementFactory.getThreadMXBean();
		int numThreads = threadIds.length;
		ThreadInfo infos[] = new ThreadInfo[numThreads];
		for (int i = 0; i < numThreads; ++i) {
			infos[i] = threadBean.getThreadInfo(threadIds[i]);
		}
		DiagnosticProperties myProps = getThreadInfoPropertiesImpl(threadBean, infos);
		return myProps;
	}

	private static DiagnosticProperties getThreadInfoPropertiesImpl(ThreadMXBean threadBean, ThreadInfo[] infos) {
		DiagnosticProperties props = new DiagnosticProperties();
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERROR, false);
		int n = infos.length;
		props.put(NUM_THREADS, n);
		props.put(THREAD_GROUP_VERSION, CURRENT_GROUPINFO_VERSION);
		props.put(DiagnosticsInfo.JAVA_INFO, System.getProperty("java.vm.info")); //$NON-NLS-1$
		boolean addCpuTime = threadBean.isThreadCpuTimeSupported() && threadBean.isThreadCpuTimeEnabled();
		for (int i = 0; i < n; ++i) {
			if (null == infos[i]) {
				continue;
			}
			JvmThreadInfo.put(infos[i], props, i);
			if (addCpuTime) {
				String prefix = JvmThreadInfo.thrdNumPrefix(i);
				long id = infos[i].getThreadId();
				long cpuTime = threadBean.getThreadCpuTime(id);
				props.put(prefix + CPU_TIME, cpuTime);
				long userTime = threadBean.getThreadUserTime(id);
				props.put(prefix + USER_TIME, userTime);
			}
		}
		DiagnosticProperties.dumpPropertiesIfDebug("Threads:", props.toProperties()); //$NON-NLS-1$
		return props;
	}

	int activeCount() {
		return numThreads;
	}

	@Override
	public String toString() {
		String newline = System.lineSeparator();
		StringBuffer buff = new StringBuffer(2000);
		for (int i = 0; i < numThreads; ++i) {
			ThreadInfo ti = myThreads[i].getThreadInfo();
			if (null == ti) {
				/* incompatible data. Dump the raw data. */
				buff.setLength(0);
				break;
			}
			buff.append(ti.toString());
			LockInfo[] syncs = ti.getLockedSynchronizers();
			if (addSynchronizers) {
				buff.append(String.format("%n\tLocked ownable synchronizers: %d%n", //$NON-NLS-1$
						Integer.valueOf(syncs.length)));
				for (LockInfo s : syncs) {
					buff.append(String.format("\t- %s%n", s.toString())); //$NON-NLS-1$
				}
				buff.append(newline);
			}
			String prefix = JvmThreadInfo.thrdNumPrefix(i);
			try {
				String cpuTimeKey = prefix + CPU_TIME;
				String userTimeKey = prefix + USER_TIME;
				if (myProps.containsField(cpuTimeKey)) {
					long cpuTime = myProps.getLong(cpuTimeKey);
					buff.append(String.format(TIME_FORMAT_STRING, "CPU", Long.valueOf(cpuTime))); //$NON-NLS-1$
				}
				if (myProps.containsField(userTimeKey)) {
					long userTime = myProps.getLong(userTimeKey);
					buff.append(String.format(TIME_FORMAT_STRING, "User", Long.valueOf(userTime))); //$NON-NLS-1$
				}
			} catch (NumberFormatException | IOException e) {
				/* EMPTY: ignore missing optional information */
			}
			buff.append(newline);
		}
		String result = buff.toString();
		if (result.isEmpty()) {
			result = "Incompatible data from target VM:" //$NON-NLS-1$
					+ newline + DiagnosticProperties.printProperties(myProps.toProperties());
		}
		return result;
	}

	public String getGroupInfoVersion() {
		return groupInfoVersion;
	}

	@Override
	public String getJavaInfo() {
		return javaInfo;
	}

	private static boolean checkVersion(String version) {
		/*
		 * only one protocol version for now. Update the algorithm as versions are added
		 */
		return CURRENT_GROUPINFO_VERSION.equals(version);
	}

}
