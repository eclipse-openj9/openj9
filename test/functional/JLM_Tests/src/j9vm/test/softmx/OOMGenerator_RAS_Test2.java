/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.softmx;

import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.List;

import com.ibm.lang.management.MemoryMXBean;

@SuppressWarnings({ "nls", "restriction" })
public class OOMGenerator_RAS_Test2 {

	/**
	 * Start program with -Xmx of max heap size.
	 * Force memory used by the heap more than half of max heap size,
	 * then reset softmx to half of max heap size through JMX.
	 * Trigger gc shrink heap.
	 * Then start allocating again until OOM is reached.
	 */
	public static void main(String[] args) {
		try {
			final int MegaByte = 1024 * 1024;
			MemoryMXBean ibmMemoryMBean = (MemoryMXBean) ManagementFactory.getMemoryMXBean();
			long maxHeapSize = ibmMemoryMBean.getMaxHeapSize() / MegaByte;

			SoftmxRASTest2.logger.debug("In OOMGenerator_RAS_Test2...");
			SoftmxRASTest2.logger.debug("Max Heap Size in secondary JVM = " + maxHeapSize + "M");

			long heapSize_half_Of_original = maxHeapSize / 2;

			SoftmxRASTest2.logger.debug("Forcing memory used by heap to more than half the max heap size, which is : " + heapSize_half_Of_original + "M" );

			List<Object> tempList = new ArrayList<Object>();
			int count = 0;

			while (true) {
				/*
				 * To avoid wasting nearly half of each region with the balanced gc policy, allocate
				 * arrays that, including overhead, are slightly less than a power of 2 in size.
				 */
				tempList.add(new byte[MegaByte - 32]);
				count++;
				if (count > (heapSize_half_Of_original + 5)) {
					break;
				}
			}

			// Set softmx value to half the original max Heap size through JMX
			SoftmxRASTest2.logger.debug("Re-setting softmx value via JMX to " + heapSize_half_Of_original + "M");
			ibmMemoryMBean.setMaxHeapSize(heapSize_half_Of_original * MegaByte);

			// Force GC shrink heap
			SoftmxRASTest2.logger.debug("Force Aggressive GC. Waiting for heap shrink...");
			TestNatives.setAggressiveGCPolicy();

			long newSoftmxValue = ibmMemoryMBean.getMaxHeapSize() / MegaByte;

			SoftmxRASTest2.logger.debug("New softmx value = " + newSoftmxValue + "M");

			// Start object allocation again..
			SoftmxRASTest2.logger.debug("Starting object allocation again. We will intentionally reach an OOM...");

			new MemoryExhauster().exhaustHeap();
		} catch (OutOfMemoryError OME) {
			SoftmxRASTest2.logger.error("Received OutOfMemoryError");
		}
	}

}
