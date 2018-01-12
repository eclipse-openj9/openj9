/*[INCLUDE-IF Sidecar17]*/

/*******************************************************************************
 * Copyright (c) 2006, 2014 IBM Corp. and others
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

package com.ibm.jvm;

/**
 * This class is used provide the following java heap and system statistics:
 * <ul> committed heap memory
 * <ul> used heap memory
 * <ul> max heap memory
 * <ul> softmx heap memory
 * <ul> free physical memory
 * <ul> total physical memory
 * All this info is retrieved without any allocation of new objects
 */
public class Stats {
	private long committedHeap = 0L;
	private long usedHeap = 0L;
	private long maxHeap = 0L;
	private long softmxHeap = 0L;
	private long freePhysicalMem = 0L;
	private long totPhysicalMem = 0L;
	private double sysLoadAvg = 0.0;
	private long cpuTime = 0L;

	/** To avoid allocating new objects, this function merely sets
	 *  the member variables of this class as it is called from a
	 *  native.
	 * 
	 * @param committed is the committed heap size (bytes)
	 * @param used is the amount of used memory on the java heap (bytes)
	 * @param max is the maximum bound of the java heap (bytes)
	 * @param free is the amount of free physical memory (bytes)
	 * @param tot is the amount of total physical memory (bytes)
	 * @param sysLoadAvg is the system load average in decimal (capacity of cpu)
	 * @param cpuTime is the time elapsed since the start of the jvm process
	 * @param softmxHeap is the softmx heap value (-Xsoftmx or changed from MemoryMXBean) value in bytes
	 */
	public void setFields(long committed, long used, long max, long free, long tot, double sysLoadAvg, long cpuTime, long softmxHeap)
	{
		this.committedHeap = committed;
		this.usedHeap = used;
		this.maxHeap = max;
		this.freePhysicalMem = free;
		this.totPhysicalMem = tot;
		this.sysLoadAvg = sysLoadAvg;
		this.cpuTime = cpuTime;
		this.softmxHeap = softmxHeap;
	}
	
	/**
	 * @return the committed heap (reserved memory) in bytes
	 */
	public long getCommittedHeap() {
		return committedHeap;
	}

	/**
	 * @return how much of the java heap is used by the application in bytes
	 */
	public long getUsedHeap() {
		return usedHeap;
	}

	/**
	 * @return the maximum java heap memory that can be used by the jvm application (-Xmx) in bytes
	 */
	public long getMaxHeap() {
		return maxHeap;
	}
	
	/**
	 * @return the softmx java heap memory (set by -Xsoftmx or from the MemoryMXBean.setMaxHeapSize)
	 */
	public long getSoftmxHeap() {
		return softmxHeap;
	}

	/**
	 * @return the amount of free physical memory in bytes
	 */
	public long getFreePhysicalMem() {
		return freePhysicalMem;
	}

	/**
	 * @return the total amount of physical memory in bytes
	 */
	public long getTotPhysicalMem() {
		return totPhysicalMem;
	}
	
	/**
	 * @return the system load average in last minute (fraction)
	 */
	public double getSysLoadAvg() {
		return sysLoadAvg;
	}
	
	/**
	 * Returns total amount of time the process has been scheduled or executed so far
	 * in both kernel and user modes.
	 * 
	 * @return time in 100 ns units, or -1 in the case of an error or if this metric
	 *         is not supported for this operating system.
	 */
	public long getCpuTime() {
		return cpuTime;
	}
	
	/**
	 *  Native used to retrieve the heap/OS stats 
	 */
	public native void getStats();
}	
