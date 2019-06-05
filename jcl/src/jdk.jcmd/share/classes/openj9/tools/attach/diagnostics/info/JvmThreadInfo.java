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

import static openj9.tools.attach.diagnostics.info.ThreadGroupInfo.THREAD_GROUP_PREFIX;

import java.io.IOException;
import java.lang.management.ThreadInfo;
import java.util.Properties;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeType;

import com.ibm.java.lang.management.internal.ThreadInfoUtil;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;

/**
 * Container for information on a foreign JVM's thread
 *
 */
public class JvmThreadInfo extends JvmInfo {

	private final ThreadInfo myInfo;

	/**
	 * Construct a JvmThreadInfo using a generic Properties file.
	 * 
	 * @param props     properties object produced by
	 *                  ThreadGroupInfo.getThreadProperties()
	 * @param threadNum ordinal position of the thread in the list or threads
	 * @throws IOException if data cannot be converted
	 */
	public JvmThreadInfo(Properties props, int threadNum) throws IOException {
		this(new DiagnosticProperties(props), threadNum);
	}

	/**
	 * Get the ThreadInfo object for a thread on a target VM.
	 * 
	 * @return ThreadInfo object or null if the data was not parsable.
	 */
	public ThreadInfo getThreadInfo() {
		return myInfo;
	}
	
	static void put(ThreadInfo info, DiagnosticProperties props, int threadNum) {
		CompositeData threadData = ThreadInfoUtil.toCompositeData(info);
		String keyPrefix = thrdNumPrefix(threadNum);
		CompositeType compType = ThreadInfoUtil.getCompositeType();
		addFields(props, keyPrefix, compType, threadData);
	}

	static String thrdNumPrefix(int threadNum) {
		return THREAD_GROUP_PREFIX + "thread_num." //$NON-NLS-1$
				+ threadNum + '.';
	}

	/**
	 * Parse a properties file from a foreign JVM. This may contain information for
	 * multiple threads.
	 * 
	 * @param props     properties object produced by
	 *                  ThreadGroupInfo.getThreadProperties()
	 * @param threadNum ordinal position of the thread in the list or threads
	 * @throws IOException if the properties are invalid
	 */
	JvmThreadInfo(DiagnosticProperties props, int threadNum) throws IOException {
		String threadNumPrefix = thrdNumPrefix(threadNum);
		CompositeType threadType = ThreadInfoUtil.getCompositeType();
		CompositeData compData = extractFields(props, threadNumPrefix, threadType);
		ThreadInfo extractedInfo = null;
		try {
			extractedInfo = (null != compData) ? ThreadInfo.from(compData) : null;
		} catch (IllegalArgumentException e) {
			extractedInfo = null;
			if (DiagnosticProperties.isDebug) {
				e.printStackTrace();
			}
		}
		myInfo = extractedInfo;
	}

}
