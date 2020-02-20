/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import com.sun.management.OperatingSystemMXBean;
import java.lang.management.ManagementFactory;

/**
 * @file TestStringBufferAndBuilderGrowth.java
 * @brief Grows a StringBuffer and StringBuilder to Integer.MAX_VALUE
 */
public class TestStringBufferAndBuilderGrowth {

public static void main(String[] args) {
	OperatingSystemMXBean opBean = (OperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean();
	long physicalMemory = opBean.getTotalPhysicalMemorySize();
	System.out.println("Machine has physical memory " + physicalMemory + " bytes or " + (physicalMemory >> 20) + " MB or " + (physicalMemory >> 30) + " GB");
	// An AIX machine with 7616 MB doesn't work
	long limit = 8193L << 20; 
	if (physicalMemory < limit) {
		// Machines with less memory may swap and timeout trying to run the test
		System.out.println("Not enough resource to run test.");
		return;
	}

	char[] cbuf = new char[Integer.MAX_VALUE / 32];

	// Create a StringBuffer big enough that the default algorithm to
	// grow it will overflow Integer.MAX_VALUE. Some smaller sizes could
	// be used without affecting the test, such as the default size, but
	// starting it big avoids unnecessary copying so the test runs faster.
	StringBuffer sbuf = new StringBuffer((Integer.MAX_VALUE / 2) + 1);
	// 17 iterations adds approximately 1G + 64M (minus a few bytes)
	// to the Buffer, causing it to grow.
	for (int i = 0; i < 17; i++) {
		try {
 	 		sbuf.append(cbuf);
		} catch (OutOfMemoryError e) {
			System.out.println("OOM StringBuffer occurred iteration " + i);
			return;
		}
	}
	int sbufCapacity = sbuf.capacity();
	// sbuf is no longer used after this point and will be GCed. Nulling it
	// isn't technically required for OpenJ9, but make it explicit for clarity.
	sbuf = null;

	// Duplicate the above test for StringBuilder.
	StringBuilder sbld = new StringBuilder((Integer.MAX_VALUE / 2) + 1);
	for (int i = 0; i < 17; i++) {
		try {
 	 		sbld.append(cbuf);
		} catch (OutOfMemoryError e) {
			System.out.println("OOM StringBuilder occurred iteration " + i);
			return;
		}
	}
	int sbldCapacity = sbld.capacity();

	System.out.println("StringBuffer capacity=" + sbufCapacity + " StringBuilder capacity=" + sbldCapacity);
}
}
