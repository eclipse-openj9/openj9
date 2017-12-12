/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.lang.management.internal;

import javax.management.MBeanNotificationInfo;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.java.lang.management.internal.OperatingSystemMXBeanImpl;
import com.ibm.lang.management.AvailableProcessorsNotificationInfo;
import com.ibm.lang.management.CpuLoadCalculationConstants;
import com.ibm.lang.management.MemoryUsage;
import com.ibm.lang.management.MemoryUsageRetrievalException;
import com.ibm.lang.management.OperatingSystemMXBean;
import com.ibm.lang.management.ProcessingCapacityNotificationInfo;
import com.ibm.lang.management.ProcessorUsage;
import com.ibm.lang.management.ProcessorUsageRetrievalException;
import com.ibm.lang.management.TotalPhysicalMemoryNotificationInfo;

/**
 * Runtime type for {@link com.ibm.lang.management.OperatingSystemMXBean}.
 * 
 * @author sonchakr, sridevi
 * @since 1.7.1
 */
public class ExtendedOperatingSystemMXBeanImpl extends OperatingSystemMXBeanImpl implements OperatingSystemMXBean {

	private enum HwEmulResult { NO, UNKNOWN, YES }

	private static final ExtendedOperatingSystemMXBeanImpl instance = new ExtendedOperatingSystemMXBeanImpl();

	/*
	 * Maintain 3 distinct sampling points of timestamps and CPU times (in static fields). 
	 * They are used in the getProcessCpuLoad calculations.
	 */
	private static long oldTime = -1;
	private static long oldCpuTime = -1;
	private static long interimTime = -1;
	private static long interimCpuTime = -1;
	private static long latestTime = -1;
	private static long latestCpuTime = -1;

	private static MemoryUsage memObj = new MemoryUsage();

	/**
	 * Singleton accessor method.
	 * 
	 * @return the {@link com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl} singleton.
	 */
	public static ExtendedOperatingSystemMXBeanImpl getInstance() {
		if (ManagementUtils.isRunningOnUnix()) {
			return UnixExtendedOperatingSystem.getInstance();
		} else {
			return instance;
		}
	}

	/**
	 * Used to identify emulated hardware (z/OS only)
	 *
	 * @param hwModel The hardware model number 
	 * @return True if hwModel is in the list of hardware model numbers indicating 
	 * emulated hardware. False otherwise
	 */
	private static boolean isZosHardwareEmulated(String hwModel) {
		/* hardcoded model numbers for zpdt */
		if (hwModel.equalsIgnoreCase("1090") || hwModel.equalsIgnoreCase("1091")) { //$NON-NLS-1$ //$NON-NLS-2$
			return true;
		}

		/* add configurable model numbers if any */
		String emuHwProperty = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("com.ibm.lang.management.OperatingSystemMXBean.zos.emulatedHardwareModels"); //$NON-NLS-1$

		if (null != emuHwProperty) {
			for (String emuHw : emuHwProperty.split("[;,]")) { //$NON-NLS-1$
				if (hwModel.equalsIgnoreCase(emuHw)) {
					return true;
				}
			}
		}

		return false;
	}

	private final CpuUtilizationHelper cpuUtilizationHelper = new CpuUtilizationHelper();

	private HwEmulResult isHwEmulated = HwEmulResult.UNKNOWN;

	/**
	 * Protected  Constructor to prevent instantiation by others, but let subclasses use it.
	 */
	ExtendedOperatingSystemMXBeanImpl() {
		super();
		// only launch the notification thread if the environment could change
		if (isDLPAREnabled()) {
			Thread thread = new OperatingSystemNotificationThread(this);

			thread.setDaemon(true);
			thread.setName("OperatingSystemMXBean notification dispatcher"); //$NON-NLS-1$
			thread.setPriority(Thread.NORM_PRIORITY + 1);
			thread.start();
		}
	}

	/**
	 * @return true if we are executing in a DLPAR-enabled environment where
	 *         #cpus / capacity / phys mem size change notifications might be
	 *         emitted
	 */
	private native boolean isDLPAREnabled();

	/**
	 * Helper Function to validate the timing information in the two 
	 * records and calculate the CPU utilization
	 * per CPU over that interval.
	 * @param	endTs Timestamp at the end of the interval.
	 * @param	endCpuTime Cpu time consumed at the end of the interval.
	 * @param	startTs Timestamp at the beginning of the interval.
	 * @param	startCpuTime Cpu time sampled at the onset of the interval.
	 * @return number in [0.0, 1.0], or ERROR_VALUE in case of error
	 */
	private double calculateProcessCpuLoad(long endTs, long endCpuTime, long startTs, long startCpuTime) {
		double timestampDelta = endTs - startTs;
		double processTimeDelta = endCpuTime - startCpuTime;
		if ((timestampDelta <= 0) || (processTimeDelta < 0)) {
			/* The stats obtained are invalid */
			return CpuLoadCalculationConstants.ERROR_VALUE;
		}
		/* Ensure that the load doesn't go over 1.0. */
		return Math.min(processTimeDelta / (getOnlineProcessorsImpl() * timestampDelta), 1.0);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getFreePhysicalMemorySize() {
		return this.getFreePhysicalMemorySizeImpl();
	}

	/**
	 * Returns the amount of free physical memory at current instance on the
	 * system in bytes. Returns -1 if the value is unavailable on this
	 * platform or in the case of an error.
	 *
	 * @return amount of physical memory available in bytes
	 */
	private native long getFreePhysicalMemorySizeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getFreeSwapSpaceSize() {
		try {
			memObj = retrieveMemoryUsage(memObj);
		} catch (MemoryUsageRetrievalException e) {
			// In case of exception return -1
			return -1;
		}
		return memObj.getSwapFree();
	}

	/**
	 * Retrieve hardware model
	 *
	 * @return String containing the hardware model. NULL in case of an error.
	 * @throws UnsupportedOperationException if the operation is not implemented on this platform. 
	 * UnsupportedOperationException will also be thrown if the operation is implemented but it 
	 * cannot be performed because the system does not satisfy all the requirements, for example,
	 * an OS service is not installed.
	 */
	@Override
	public final String getHardwareModel() throws UnsupportedOperationException {
		return getHardwareModelImpl();
	}

	/**
	 * Retrieve hardware model
	 *
	 * @return String containing the hardware model. NULL in case of an error.
	 * @throws UnsupportedOperationException if the operation is not implemented on this platform. 
	 * UnsupportedOperationException will also be thrown if the operation is implemented but it 
	 * cannot be performed because the system does not satisfy all the requirements, for example,
	 * an OS service is not installed.
	 */
	private native String getHardwareModelImpl() throws UnsupportedOperationException;

	/* Native functions that this class defines and consumes. */
	private native MemoryUsage getMemoryUsageImpl(MemoryUsage memUsage);

	/* Returns the number of CPU's online at this very moment. */
	private native int getOnlineProcessorsImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final synchronized double getProcessCpuLoad() {
		double processCpuLoad = -1;

		/* Get the process CPU time and also, the sampling timestamp. */
		latestTime = System.nanoTime();
		latestCpuTime = getProcessCpuTimeByNS();

		/* First call to this method should -1, since we don't have any previous
		 * CPU times (or timestamp) to compute CPU load against.
		 */
		if (-1 == oldTime) {
			/* Save current counters; next invocation onwards, we use these to
			 * compute CPU loads.
			 */
			oldTime = interimTime = latestTime;
			oldCpuTime = interimCpuTime = latestCpuTime;
			return CpuLoadCalculationConstants.ERROR_VALUE;
		}

		/* If a sufficiently long interval has elapsed since last sampling, calculate using 
		 * the most recent value in the history.
		 */
		if ((latestTime - interimTime) >= CpuLoadCalculationConstants.MINIMUM_INTERVAL) {
			/* Calculate the ProcessCpuLoad. */
			processCpuLoad = calculateProcessCpuLoad(latestTime,
					latestCpuTime,
					interimTime,
					interimCpuTime);
			if (processCpuLoad >= 0.0) { /* no errors detected in the statistics */
				/* Save the interim counters as old and update the interim counters with the
				 * latest ones (that we obtained on this invocation).
				 */
				oldTime = interimTime;
				oldCpuTime = interimCpuTime;
				interimTime = latestTime;
				interimCpuTime = latestCpuTime;
				return processCpuLoad;
			} else {
				interimTime = latestTime;
				interimCpuTime = latestCpuTime;
				/*
				 * either the latest time or the interim time are bogus. 
				 * Discard the interim value and try with the oldest value. 
				 */
			}
		}
		if ((latestTime - oldTime) >= CpuLoadCalculationConstants.MINIMUM_INTERVAL) {
			processCpuLoad = calculateProcessCpuLoad(latestTime, 
					latestCpuTime, 
					oldTime,
					oldCpuTime);
			if (processCpuLoad < 0) {
				/* the stats look bogus.  Discard them */
				oldTime = latestTime;
				oldCpuTime = latestCpuTime;
			}
		}

		return processCpuLoad;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getProcessCpuTime() {
		return this.getProcessCpuTimeImpl() * CpuTimePrecisionHolder.precision;
	}

	/**
	 * Deprecated. Use getProcessCpuTime()
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public final long getProcessCpuTimeByNS() {
		long cpuTimeNS = this.getProcessCpuTime();
		if (CpuTimePrecisionHolder.precision == CpuTimePrecisionHolder.NO_SCALE_FACTOR) {
			cpuTimeNS *= CpuTimePrecisionHolder.NS_SCALE_FACTOR;
		}
		return cpuTimeNS;
	}

	/**
	 * Returns total amount of time the process has been scheduled or
	 * executed so far in both kernel and user modes. Returns -1 if the
	 * value is unavailable on this platform or in the case of an error.
	 * 
	 * @return process cpu ime in 100 ns units
	 * @see #getProcessCpuTime()
	 */
	private native long getProcessCpuTimeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final int getProcessingCapacity() {
		return this.getProcessingCapacityImpl();
	}

	/**
	 * @return the collective capacity of the virtual processors available to
	 *         the VM
	 * @see #getProcessingCapacity()
	 */
	private native int getProcessingCapacityImpl();

	private native ProcessorUsage[] getProcessorUsageImpl(ProcessorUsage[] procUsage);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getProcessPhysicalMemorySize() {
		return this.getProcessPhysicalMemorySizeImpl();
	}

	/**
	 * Returns the amount of physical memory being used by the process
	 * in bytes. Returns -1 if the value is unavailable on this platform
	 * or in the case of an error.
	 * 
	 * @return amount of physical memory being used by the process in bytes
	 * @see #getProcessPrivateMemorySize()
	 */
	private native long getProcessPhysicalMemorySizeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getProcessPrivateMemorySize() {
		return this.getProcessPrivateMemorySizeImpl();
	}

	/**
	 * Returns the amount of private memory used by the process in bytes.
	 * Returns -1 if the value is unavailable on this platform or in the
	 * case of an error.
	 * 
	 * @return amount of private memory used by the process in bytes
	 * @see #getProcessPrivateMemorySize()
	 */
	private native long getProcessPrivateMemorySizeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getCommittedVirtualMemorySize() {
		return this.getProcessVirtualMemorySizeImpl();
	}

	/**
	 * Deprecated: use getCommittedVirtualMemorySize()
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public final long getProcessVirtualMemorySize() {
		return this.getProcessVirtualMemorySizeImpl();
	}

	/**
	 * Returns the amount of virtual memory used by the process in bytes,
	 * including physical memory and swap space. Returns -1 if the value
	 * is unavailable on this platform or in the case of an error.
	 * 
	 * @return amount of virtual memory used by the process in bytes
	 * @see #getCommittedVirtualMemorySize()
	 */
	private native long getProcessVirtualMemorySizeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final double getSystemCpuLoad() {
		return cpuUtilizationHelper.getSystemCpuLoad();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getTotalPhysicalMemorySize() {
		return this.getTotalPhysicalMemoryImpl();
	}

	/**
	 * Deprecated: use getTotalPhysicalMemorySize()
	 */
	/*[IF Sidecar19-SE]
	@Deprecated(forRemoval = true, since = "1.8")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public final long getTotalPhysicalMemory() {
		return this.getTotalPhysicalMemoryImpl();
	}

	/**
	 * @return the number of bytes used for physical memory
	 * @see #getTotalPhysicalMemorySize()
	 */
	private native long getTotalPhysicalMemoryImpl();

	private native ProcessorUsage getTotalProcessorUsageImpl(ProcessorUsage procTotalUsage);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getTotalSwapSpaceSize() {
		try {
			memObj = retrieveMemoryUsage(memObj);
		} catch (MemoryUsageRetrievalException e) {
			// In case of exception return -1
			return -1;
		}
		return memObj.getSwapTotal();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final boolean isHardwareEmulated() throws UnsupportedOperationException {
		if (HwEmulResult.UNKNOWN == isHwEmulated) {
			String osName = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("os.name"); //$NON-NLS-1$
			String hwModel = getHardwareModel();

			if ((null != osName) && (null != hwModel)) {
				boolean isEmuTmp = false;

				if (osName.equalsIgnoreCase("z/OS")) { //$NON-NLS-1$
					isEmuTmp = isZosHardwareEmulated(hwModel);
				}

				if (isEmuTmp) {
					isHwEmulated = HwEmulResult.YES;
				} else {
					isHwEmulated = HwEmulResult.NO; 
				}
			} else {
				if (null == hwModel) {
					/*[MSG "K05db", "Could not get hardware model"]*/
					String strErr = com.ibm.oti.util.Msg.getString("K05db"); //$NON-NLS-1$

					throw new UnsupportedOperationException(strErr);
				} else {
					throw new UnsupportedOperationException();
				}
			}
		}

		return (HwEmulResult.YES == isHwEmulated);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final MemoryUsage retrieveMemoryUsage() throws MemoryUsageRetrievalException {
		/* Allocate and construct a MemoryUsage instance. */
		MemoryUsage memoryUsageObj = new MemoryUsage();

		/* Obtain the current memory usage stats. */
		memoryUsageObj = getMemoryUsageImpl(memoryUsageObj);

		return memoryUsageObj;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final MemoryUsage retrieveMemoryUsage(MemoryUsage memoryUsageObj)
			throws NullPointerException, MemoryUsageRetrievalException {
		/* For this overload, it is expected that we receive a pre-allocated object. */
		if (null == memoryUsageObj) {
			throw new NullPointerException();
		}

		/* Obtain the current memory usage stats. */
		memoryUsageObj = getMemoryUsageImpl(memoryUsageObj);

		return memoryUsageObj;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ProcessorUsage[] retrieveProcessorUsage() throws ProcessorUsageRetrievalException {
		/* Obtain the processor usage statistics at this moment and return the same. */
		return getProcessorUsageImpl(null);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ProcessorUsage[] retrieveProcessorUsage(ProcessorUsage[] procUsageArr)
			throws NullPointerException, ProcessorUsageRetrievalException, IllegalArgumentException {
		/* check whether the processorUsage array has been allocated for and
		 * also whether procUsage objects have been allocated.
		 */
		if (null == procUsageArr) {
			/*[MSG "K056B", "Null ProcessorUsage array received."]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K056B")); //$NON-NLS-1$
		}

		/* Check the array received for a NULL slot. If an unallocated slot is hit, throw 
		 * a NullPointerException indicating this.
		 */
		for (ProcessorUsage p : procUsageArr) {
			if (null == p) {
				/*[MSG "K056C", "Null ProcessorUsage array element received."]*/
				throw new NullPointerException(com.ibm.oti.util.Msg.getString("K056C")); //$NON-NLS-1$
			}
		}

		/* obtain the processor usage statistics at this moment. */
		procUsageArr = getProcessorUsageImpl(procUsageArr);

		return procUsageArr;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ProcessorUsage retrieveTotalProcessorUsage() throws ProcessorUsageRetrievalException {
		/* The user expects the DeprecatedExtendedOperatingSystem class to instantiate and
		 * return an object on its own.
		 */
		ProcessorUsage procUsageObj = new ProcessorUsage();

		/* Obtain the processor usage statistics sample at this time. */
		procUsageObj = getTotalProcessorUsageImpl(procUsageObj);

		return procUsageObj;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ProcessorUsage retrieveTotalProcessorUsage(ProcessorUsage procUsageObj)
			throws NullPointerException, ProcessorUsageRetrievalException {
		/* Did the user pass on a Null object. Throw an exception, if yes. */
		if (null == procUsageObj) {
			throw new NullPointerException();
		}

		/* Obtain the processor usage statistics sample at this time. */
		procUsageObj = getTotalProcessorUsageImpl(procUsageObj);

		return procUsageObj;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final MBeanNotificationInfo[] getNotificationInfo() {
		// We know what kinds of notifications we can emit whereas the
		// notifier delegate does not. So, for this method, no delegating.
		// Instead respond using our own metadata.
		return new MBeanNotificationInfo[] {
				// -- Processing capacity notification type
				makeNotificationInfo(
						ProcessingCapacityNotificationInfo.PROCESSING_CAPACITY_CHANGE,
						"Processing Capacity Notification"), //$NON-NLS-1$
				// -- Total physical memory notification type
				makeNotificationInfo(
						TotalPhysicalMemoryNotificationInfo.TOTAL_PHYSICAL_MEMORY_CHANGE,
						"Total Physical Memory Notification"), //$NON-NLS-1$
				// -- Available processors notification type
				makeNotificationInfo(
						AvailableProcessorsNotificationInfo.AVAILABLE_PROCESSORS_CHANGE,
						"Available Processors Notification") //$NON-NLS-1$
		};
	}

	private static MBeanNotificationInfo makeNotificationInfo(String type, String description) {
		return new MBeanNotificationInfo(
				new String[] { type },
				javax.management.Notification.class.getName(),
				description);
	}

	/**
	 * Do lazy initialization of the precision value.
	 * By default, precision is 1 ns.  The user can override this by -Dcom.ibm.lang.management.OperatingSystemMXBean.isCpuTime100ns=true
	 */
	private final static class CpuTimePrecisionHolder {
		static final int precision = getPrecision();
		static final int NS_SCALE_FACTOR = 100;
		static final int NO_SCALE_FACTOR = 1;
		
		private static int getPrecision() {
			boolean is100ns = Boolean.getBoolean("com.ibm.lang.management.OperatingSystemMXBean.isCpuTime100ns"); //$NON-NLS-1$
			int precisionVaue = is100ns ? NO_SCALE_FACTOR : NS_SCALE_FACTOR; /* if 1 ns resolution, scale the result up by 100 */
			return precisionVaue;
		}
	}

}
