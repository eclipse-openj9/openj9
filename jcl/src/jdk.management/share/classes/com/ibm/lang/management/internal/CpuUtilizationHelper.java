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

import com.ibm.lang.management.CpuLoadCalculationConstants;

final class CpuUtilizationHelper implements CpuLoadCalculationConstants {

	SysinfoCpuTime oldestTime = null;
	SysinfoCpuTime interimTime = null;
	SysinfoCpuTime latestTime = null;

	/**
	 * Returns the CPU utilization, i.e. fraction of the time spent in user or system/privileged mode, since the last call to getSystemCpuLoad() on this object.  
	 * To compute load over different concurrent intervals, create separate objects.
	 * If insufficient time (less than 10 ms) has elapsed since a previous call on this object, or
	 * if an operating system error has occurred, e.g. clock rollover, ERROR_VALUE is returned.
	 * If this operation is not supported due to insufficient user privilege, return INSUFFICIENT_PRIVILEGE. 
	 * If this operation is not supported on this platform, return UNSUPPORTED_VALUE. 
	 * INTERNAL_ERROR indicates an internal problem.
	 * This is not guaranteed to return the same value as reported by operating system
	 * utilities such as Unix "top" or Windows task manager.
	 * @return value between 0 and 1.0, or a negative value in the case of an error.
	 */
	synchronized double getSystemCpuLoad() {
		double cpuLoad = ERROR_VALUE;

		latestTime = SysinfoCpuTime.getCpuUtilizationImpl();
		/* the native had problems creating the object */
		if (null == latestTime) {
			return INTERNAL_ERROR;
		}

		/* not supported on this platform or user does not have sufficient rights */
		int status = latestTime.getStatus();
		if (status < 0) {
			return status;
		}

		if (null == oldestTime) { /* first call to this method */
			oldestTime = interimTime = latestTime;
			return ERROR_VALUE;
		}

		/* calculate using the most recent value in the history */
		if ((latestTime.getTimestamp() - interimTime.getTimestamp()) >= MINIMUM_INTERVAL) {
			cpuLoad = calculateCpuLoad(latestTime, interimTime);
			if (cpuLoad >= 0.0) { /* no errors detected in the statistics */
				oldestTime = interimTime;
				interimTime = latestTime; /* next time we'll use the oldestTime */
				return cpuLoad;
			} else {
				interimTime = latestTime;
				/*
				 * either the latest time or the interim time are bogus. 
				 * Discard the interim value and try with the oldest value. 
				 */
			}
		}

		if ((latestTime.getTimestamp() - oldestTime.getTimestamp()) >= MINIMUM_INTERVAL) {
			cpuLoad = calculateCpuLoad(latestTime, oldestTime);
			if (cpuLoad < 0) { /* the stats look bogus.  Discard them */
				oldestTime = latestTime;
			}
		}

		return cpuLoad;
	}

	/**
	 * Validate the timing information in the two records and calculate the CPU utilization
	 * per CPU over that interval.
	 * @param endRecord timing record for the end of the interval
	 * @param startRecord timing record for the start of the interval
	 * @return number in [0.0, 1.0], or ERROR_VALUE in case of error
	 */
	private static double calculateCpuLoad(SysinfoCpuTime endRecord, SysinfoCpuTime startRecord) {
		double timestampDelta = endRecord.getTimestamp() - startRecord.getTimestamp();
		double cpuDelta = endRecord.getCpuTime() - startRecord.getCpuTime();
		if ((timestampDelta <= 0) || (cpuDelta < 0)) {
			return ERROR_VALUE; /* the stats are invalid */
		}
		/* ensure the load can't go over 1.0 */
		return Math.min(cpuDelta / (endRecord.getNumberOfCpus() * timestampDelta), 1.0);
	}

}
