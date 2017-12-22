/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
package com.ibm.lang.management.internal;

final class SysinfoCpuTime {

	private long timestamp; /* in ns.  Used for comparison, this has no absolute significance. */
	private long cpuTime; /* in ns. Does not necessarily start at the same time as the timestamp. */
	private int numberOfCpus; /* should be at least 1 */
	private int status;
	private long processCpuTime; /* in ns. The process Cpu Time, used for getProcessCpuLoad() */

	/**
	 * @param timestamp time in nanoseconds from a fixed but arbitrary point in time 
	 * @param cpuTime time spent in user, nice, and system/privileged state by all CPUs.
	 * @param numberOfCpus number of CPUs as reported by the operating system.
	 * @param status status as reported by the JNI native
	 */
	SysinfoCpuTime(long timestamp, long cpuTime, int numberOfCpus, int status) {
		super();
		this.timestamp = timestamp;
		this.cpuTime = cpuTime;
		this.numberOfCpus = numberOfCpus;
		this.status = status;
	}
	
	/**
	 * calls the constructor for this class and initializes the fields
	 * @return null if the JNI code could not look up the class or constructor.
	 */
	static native SysinfoCpuTime getCpuUtilizationImpl();

	/**
	 * @see com.ibm.lang.management.internal.CpuUtilizationHelper for error code values
	 * @return status of the JNI native.
	 */
	int getStatus() {
		return status;
	}

	/**
	 * @return number of CPUs
	 */
	long getNumberOfCpus() {
		return numberOfCpus;
	}

	/**
	 * @return timestamp in ns
	 */
	long getTimestamp() {
		return timestamp;
	}

	/**
	 * @return cumulative CPU time in ns
	 */
	long getCpuTime() {
		return cpuTime;
	}

	/**
	 * Set the Process Cpu Time for this SysinfoTime Object
	 * 
	 * @param processCpuTime the ProcessCpuTime in ns.
	 */
	void setProcessCpuTime(long processCpuTime) {
		this.processCpuTime = processCpuTime;
	}

	/**
	 * @return the process Cpu Time in ns.
	 */
	long getProcessCpuTime() {
		return processCpuTime;
	}

}
