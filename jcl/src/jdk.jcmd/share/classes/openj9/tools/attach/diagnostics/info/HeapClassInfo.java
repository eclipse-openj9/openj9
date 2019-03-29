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
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Properties;
import java.util.stream.Collectors;

import com.ibm.lang.management.MemoryMXBean;
import com.ibm.tools.attach.target.IPC;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.HeapClassInformation;

/**
 * Class for getting heap information from a target VM.
 *
 */
public class HeapClassInfo extends JvmInfo {
	
	static final String HEAP_CLASS_PREFIX = "heap_class."; //$NON-NLS-1$
	static final String NUM_CLASSES = HEAP_CLASS_PREFIX + "num_classess"; //$NON-NLS-1$
	static final String HEAP_CLASS_NAME = "name."; //$NON-NLS-1$
	static final String HEAP_CLASS_OBJECT_SIZE = "object_size."; //$NON-NLS-1$
	static final String HEAP_CLASS_OBJECT_COUNT = "object_count."; //$NON-NLS-1$
	
	final DiagnosticProperties myProps;
	final HeapClassInformation[] heapStats;

	/**
	 * Create a HeapInfo object from a properties object.
	 * @param props Properties with the target's data
	 * @throws IOException if 
	 */
	public HeapClassInfo(Properties props) throws IOException {
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
		int numClasses = myProps.getInt(NUM_CLASSES);
		heapStats = new HeapClassInformation[numClasses];
		for (int classNum = 0; classNum < numClasses; ++classNum) {
			String prefix = classNumPrefix(classNum);
			heapStats[classNum] = getFields(myProps, prefix);
		}
		
	}

	/**
	 * Scan the heap and get statistics on object counts and sizes.
	 * @return Statistics encoded as properties
	 */
	public static DiagnosticProperties getHeapClassStatistics() {
		DiagnosticProperties props;
		MemoryMXBean memBean = JvmInfo.getMemoryMXBean();
		if (null != memBean) {
			props = new DiagnosticProperties();

			HeapClassInformation[] heapStats = memBean.getHeapClassStatistics();
			props.put(NUM_CLASSES, heapStats.length);
			int classNum = 0;
			for (HeapClassInformation hci: heapStats) {
				String prefix = classNumPrefix(classNum);
				addFields(props, prefix, hci);
				++classNum;
			}
		} else {
			props = DiagnosticProperties.makeStatusProperties(true, "Incompatible MemoryMXBean"); //$NON-NLS-1$			
		}
		DiagnosticProperties.dumpPropertiesIfDebug("Heap statistics", props); //$NON-NLS-1$
		return props;
	}
	
	private static void addFields(DiagnosticProperties props, String prefix, HeapClassInformation hci) {
		props.put(prefix+HEAP_CLASS_NAME, hci.getHeapClassName());
		props.put(prefix+HEAP_CLASS_OBJECT_SIZE, hci.getObjectSize());
		props.put(prefix+HEAP_CLASS_OBJECT_COUNT, hci.getObjectCount());
	}
	
	private static HeapClassInformation getFields(DiagnosticProperties props, String prefix) throws IOException {
		String heapClassName = props.getString(prefix+HEAP_CLASS_NAME);
		int objectSize = props.getInt(prefix+HEAP_CLASS_OBJECT_SIZE);
		int objectCount = props.getInt(prefix+HEAP_CLASS_OBJECT_COUNT);
		HeapClassInformation hci = new HeapClassInformation(heapClassName, objectCount, objectSize);
		return hci;
	}
	
	static String classNumPrefix(int classNum) {
		return HEAP_CLASS_PREFIX + "class_num." //$NON-NLS-1$
				+ classNum + '.';
	}

	@Override
	public String toString() {
		StringBuffer buff = new StringBuffer(2000);
		String headerLine = String.format("%5s%15s%15s    %s\n"  //$NON-NLS-1$
				+ "-------------------------------------------------\n", //$NON-NLS-1$
				"num", "object count", "total size", "class name"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
		buff.append(headerLine);
		List<HeapClassInformation> sortedList = Arrays.stream(heapStats)
				/* negate the size to force reverse order */
				.sorted(Comparator.comparingLong(i -> -(i.getAggregateObjectSize())))
				.collect(Collectors.toList());
		int counter = 1;
		long totalObjectCount = 0;
		long totalObjectSize = 0;
		for (HeapClassInformation hci: sortedList) {
			long aggregateObjectSize = hci.getAggregateObjectSize();
			String statsLine = String.format("%5d%15d%15d    %s\n", //$NON-NLS-1$
					Integer.valueOf(counter), Long.valueOf(hci.getObjectCount()),
					Long.valueOf(aggregateObjectSize), 
					hci.getHeapClassName()
					);
			buff.append(statsLine);
			totalObjectCount += hci.getObjectCount();
			totalObjectSize += aggregateObjectSize;
			counter += 1;
		}
		String statsLine = String.format("%5s%15d%15d\n", //$NON-NLS-1$
				"Total", Long.valueOf(totalObjectCount), //$NON-NLS-1$
				Long.valueOf(totalObjectSize)
				);
		buff.append(statsLine);

		return buff.toString();

	}

}
