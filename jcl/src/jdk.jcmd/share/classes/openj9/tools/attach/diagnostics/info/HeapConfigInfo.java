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

import java.io.IOException;
import java.lang.management.MemoryUsage;
import java.util.Properties;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeType;

import com.ibm.java.lang.management.internal.MemoryUsageUtil;
import com.ibm.lang.management.MemoryMXBean;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticUtils;

/**
 * Class for getting heap configuration information from a target VM.
 *
 */
public class HeapConfigInfo extends JvmInfo {

	static final String HEAP_CONFIG_PREFIX = "heap_config."; //$NON-NLS-1$

	/* information about the heap */
	static final String HEAP_CONFIG_HEAPMEMORYUSAGE = "Heap Memory Usage"; //$NON-NLS-1$
	static final String HEAP_CONFIG_MAXHEAPSIZELIMIT = "Max Heap Size Limit"; //$NON-NLS-1$
	static final String HEAP_CONFIG_MAXHEAPSIZE = "Max Heap Size"; //$NON-NLS-1$
	static final String HEAP_CONFIG_MINHEAPSIZE = "Min Heap Size"; //$NON-NLS-1$
	static final String HEAP_CONFIG_NONHEAPMEMORYUSAGE = "Non Heap Memory Usage"; //$NON-NLS-1$
	static final String HEAP_CONFIG_PENDINGFINALIZATIONCOUNT = "Object Pending Finalization Count"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHESIZE = "Shared Class Cache Size"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHESOFTMXBYTES = "Shared Class Cache Softmx Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMINAOTBYTES = "Shared Class Cache Min Aot Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTBYTES = "Shared Class Cache Max Aot Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMINJITDATABYTES = "Shared Class Cache Min Jit Data Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATABYTES = "Shared Class Cache Max Jit Data Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHESOFTMXUNSTOREDBYTES = "Shared Class Cache Softmx Unstored Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTUNSTOREDBYTES = "Shared Class Cache Max Aot Unstored Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATAUNSTOREDBYTES = "Shared Class Cache Max Jit Data Unstored Bytes"; //$NON-NLS-1$
	static final String HEAP_CONFIG_SHAREDCLASSCACHEFREESPACE = "Shared Class Cache Free Space"; //$NON-NLS-1$
	static final String HEAP_CONFIG_GCMODE = "GCMode"; //$NON-NLS-1$
	static final String HEAP_CONFIG_GCMASTERTHREADCPUUSED = "GCMaster Thread Cpu Used"; //$NON-NLS-1$
	static final String HEAP_CONFIG_GCSLAVETHREADSCPUUSED = "GCSlave Threads Cpu Used"; //$NON-NLS-1$
	static final String HEAP_CONFIG_MAXIMUMGCTHREADS = "Maximum GCThreads"; //$NON-NLS-1$
	static final String HEAP_CONFIG_CURRENTGCTHREADS = "Current GCThreads"; //$NON-NLS-1$
	static final String[] numericValueKeys = new String[] { HEAP_CONFIG_MINHEAPSIZE, HEAP_CONFIG_MAXHEAPSIZE,
			HEAP_CONFIG_MAXHEAPSIZELIMIT, HEAP_CONFIG_CURRENTGCTHREADS, HEAP_CONFIG_MAXIMUMGCTHREADS,
			HEAP_CONFIG_GCMASTERTHREADCPUUSED, HEAP_CONFIG_GCSLAVETHREADSCPUUSED, HEAP_CONFIG_PENDINGFINALIZATIONCOUNT,
			HEAP_CONFIG_SHAREDCLASSCACHESIZE, HEAP_CONFIG_SHAREDCLASSCACHESOFTMXBYTES,
			HEAP_CONFIG_SHAREDCLASSCACHEMINAOTBYTES, HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTBYTES,
			HEAP_CONFIG_SHAREDCLASSCACHEMINJITDATABYTES, HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATABYTES,
			HEAP_CONFIG_SHAREDCLASSCACHESOFTMXUNSTOREDBYTES, HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTUNSTOREDBYTES,
			HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATAUNSTOREDBYTES, HEAP_CONFIG_SHAREDCLASSCACHEFREESPACE };

	final DiagnosticProperties myProps;

	/**
	 * Create a HeapInfo object from a properties object.
	 * 
	 * @param props Properties with the target's data
	 * @throws IOException if
	 */
	public HeapConfigInfo(Properties props) throws IOException {
		myProps = new DiagnosticProperties(props);
		DiagnosticProperties.dumpPropertiesIfDebug("Heap configuration:", props); //$NON-NLS-1$
	}

	/**
	 * Get a description of the heap.
	 * 
	 * @return properties object containing the configuration
	 */
	public static DiagnosticProperties getHeapConfigInfo() {
		DiagnosticProperties props;
		MemoryMXBean memBean = getMemoryMXBean();
		if (null != memBean) {
			props = new DiagnosticProperties();
			props.put(addPrefix(HEAP_CONFIG_CURRENTGCTHREADS), memBean.getCurrentGCThreads());
			props.put(addPrefix(HEAP_CONFIG_GCMASTERTHREADCPUUSED), memBean.getGCMasterThreadCpuUsed());
			props.put(addPrefix(HEAP_CONFIG_GCMODE), memBean.getGCMode());
			props.put(addPrefix(HEAP_CONFIG_GCSLAVETHREADSCPUUSED), memBean.getGCSlaveThreadsCpuUsed());
			props.put(addPrefix(HEAP_CONFIG_MAXHEAPSIZE), memBean.getMaxHeapSize());
			props.put(addPrefix(HEAP_CONFIG_MAXHEAPSIZELIMIT), memBean.getMaxHeapSizeLimit());
			props.put(addPrefix(HEAP_CONFIG_MAXIMUMGCTHREADS), memBean.getMaximumGCThreads());
			props.put(addPrefix(HEAP_CONFIG_MINHEAPSIZE), memBean.getMinHeapSize());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEFREESPACE), memBean.getSharedClassCacheFreeSpace());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTBYTES), memBean.getSharedClassCacheMaxAotBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMAXAOTUNSTOREDBYTES),
					memBean.getSharedClassCacheMaxAotUnstoredBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATABYTES),
					memBean.getSharedClassCacheMaxJitDataBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMAXJITDATAUNSTOREDBYTES),
					memBean.getSharedClassCacheMaxJitDataUnstoredBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMINAOTBYTES), memBean.getSharedClassCacheMinAotBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHEMINJITDATABYTES),
					memBean.getSharedClassCacheMinJitDataBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHESIZE), memBean.getSharedClassCacheSize());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHESOFTMXBYTES), memBean.getSharedClassCacheSoftmxBytes());
			props.put(addPrefix(HEAP_CONFIG_SHAREDCLASSCACHESOFTMXUNSTOREDBYTES),
					memBean.getSharedClassCacheSoftmxUnstoredBytes());
			props.put(addPrefix(HEAP_CONFIG_PENDINGFINALIZATIONCOUNT), memBean.getObjectPendingFinalizationCount());

			CompositeType compType = MemoryUsageUtil.getCompositeType();

			CompositeData heapMemoryInfo = MemoryUsageUtil.toCompositeData(memBean.getHeapMemoryUsage());
			addFields(props, addPrefix(HEAP_CONFIG_HEAPMEMORYUSAGE), compType, heapMemoryInfo);

			heapMemoryInfo = MemoryUsageUtil.toCompositeData(memBean.getNonHeapMemoryUsage());
			addFields(props, addPrefix(HEAP_CONFIG_NONHEAPMEMORYUSAGE), compType, heapMemoryInfo);
		} else {
			props = DiagnosticProperties.makeStatusProperties(true, "Incompatible MemoryMXBean"); //$NON-NLS-1$
		}
		return props;

	}

	private static String addPrefix(String propertyName) {
		return HEAP_CONFIG_PREFIX + propertyName.replaceAll(" ", DiagnosticUtils.EMPTY_STRING); //$NON-NLS-1$
	}

	@Override
	public String toString() {
		StringBuffer buff = new StringBuffer(2000);
		try {
			buff.append(String.format("Garbage collector mode: %s%n", //$NON-NLS-1$
					myProps.getString(addPrefix(HEAP_CONFIG_GCMODE))));
			for (String k : numericValueKeys) {
				buff.append(String.format("    %-47s: %d%n", //$NON-NLS-1$
						k, Long.valueOf(myProps.getLong(addPrefix(k)))));
			}
			buff.append(formatMemInfo(HEAP_CONFIG_HEAPMEMORYUSAGE));
			buff.append(formatMemInfo(HEAP_CONFIG_NONHEAPMEMORYUSAGE));

		} catch (IOException e) {
			buff.setLength(0);
			String msg = e.getMessage();
			if ((null == msg) || msg.isEmpty()) {
				msg = "unknown"; //$NON-NLS-1$
			}
			buff.append(String.format("Invalid data from target, error:%s%n", msg)); //$NON-NLS-1$
		}
		return buff.toString();
	}

	private String formatMemInfo(String memType) throws IOException {
		String keyPrefix = addPrefix(memType);
		CompositeType compType = MemoryUsageUtil.getCompositeType();
		CompositeData compData = extractFields(myProps, keyPrefix, compType);
		MemoryUsage extractedInfo = null;
		try {
			extractedInfo = (null != compData) ? MemoryUsage.from(compData) : null;
		} catch (IllegalArgumentException e) {
			extractedInfo = null;
			if (DiagnosticProperties.isDebug) {
				e.printStackTrace();
			}
		}
		String memInfo = DiagnosticUtils.EMPTY_STRING;
		if (null != extractedInfo) {
			memInfo = String.format("%s: %s%n", //$NON-NLS-1$
					memType, extractedInfo.toString());
		}
		return memInfo;
	}

}
