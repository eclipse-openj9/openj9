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

import java.io.PrintWriter;
import java.io.StringWriter;

import com.ibm.lang.management.MemoryMXBean;

/**
 * Class for getting heap configuration information from a target VM.
 *
 */
public class HeapConfigInfo extends JvmInfo {

	private static final String NUMBER_FORMAT = "%s: %d\n"; //$NON-NLS-1$
	private static final String STRING_FORMAT = "%s: %s\n"; //$NON-NLS-1$

	/* information about the heap */
	private static final String HEAP_CONFIG_HEAPMEMORYUSAGE = "Heap Memory Usage"; //$NON-NLS-1$
	private static final String HEAP_CONFIG_NONHEAPMEMORYUSAGE = "Non Heap Memory Usage"; //$NON-NLS-1$


	/**
	 * @return information about the heap as a formatted string
	 */
	public static String getHeapConfigString() {
		StringWriter buff = new StringWriter(1000);
		MemoryMXBean memBean = getMemoryMXBean();
		try (PrintWriter buffWriter = new PrintWriter(buff)) {
			buffWriter.printf("Garbage collector mode: %s%n", //$NON-NLS-1$
					memBean.getGCMode());
			buffWriter.printf(NUMBER_FORMAT, "Current GCThreads",//$NON-NLS-1$
					Integer.valueOf(memBean.getCurrentGCThreads()));
			buffWriter.printf(NUMBER_FORMAT, "GCMaster Thread Cpu Used", //$NON-NLS-1$
					Long.valueOf(memBean.getGCMasterThreadCpuUsed()));
			buffWriter.printf(NUMBER_FORMAT, "GCSlave Threads Cpu Used", //$NON-NLS-1$
					Long.valueOf(memBean.getGCSlaveThreadsCpuUsed()));
			buffWriter.printf(NUMBER_FORMAT, "Max Heap Size", //$NON-NLS-1$
					Long.valueOf(memBean.getMaxHeapSize()));
			buffWriter.printf(NUMBER_FORMAT, "Max Heap Size Limit", //$NON-NLS-1$
					Long.valueOf(memBean.getMaxHeapSizeLimit()));
			buffWriter.printf(NUMBER_FORMAT, "Maximum GCThreads", //$NON-NLS-1$
					Integer.valueOf(memBean.getMaximumGCThreads()));
			buffWriter.printf(NUMBER_FORMAT, "Min Heap Size", //$NON-NLS-1$
					Long.valueOf(memBean.getMinHeapSize()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Free Space", //$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheFreeSpace()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Max Aot Bytes", //$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMaxAotBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Max Aot Unstored Bytes",//$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMaxAotUnstoredBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Max Jit Data Bytes",//$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMaxJitDataBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Max Jit Data Unstored Bytes",//$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMaxJitDataUnstoredBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Min Aot Bytes", //$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMinAotBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Min Jit Data Bytes",//$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheMinJitDataBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Size", //$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheSize()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Softmx Bytes", //$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheSoftmxBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Shared Class Cache Softmx Unstored Bytes",//$NON-NLS-1$
					Long.valueOf(memBean.getSharedClassCacheSoftmxUnstoredBytes()));
			buffWriter.printf(NUMBER_FORMAT, "Object Pending Finalization Count", //$NON-NLS-1$
					Integer.valueOf(memBean.getObjectPendingFinalizationCount()));

			buffWriter.printf(STRING_FORMAT,
					HEAP_CONFIG_HEAPMEMORYUSAGE, memBean.getHeapMemoryUsage().toString());
			buffWriter.printf(STRING_FORMAT,
					HEAP_CONFIG_NONHEAPMEMORYUSAGE, memBean.getHeapMemoryUsage().toString());
		}
		return buff.toString();
	}
}
