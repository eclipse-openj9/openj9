/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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
package com.ibm.lang.management;

/**
 * The IBM-specific interface for the runtime system of the virtual machine.
 */
public interface RuntimeMXBean extends java.lang.management.RuntimeMXBean {

	/**
	 * enum type defines the different states of the VM Idle 
	 */
	public enum VMIdleStates {
		/* Below constant values reflect J9VMRuntimeStateListener.vmRuntimeState
		 * from the VM and the VM constants J9VM_RUNTIME_STATE_ACTIVE and
		 * J9VM_RUNTIME_STATE_IDLE. These values need to match the VM values.
		 */
		INVALID(-1, "Invalid"),
		ACTIVE(1, "Active"),
		IDLE(2, "Idle");

		private int stateVal;
		private String stateName;

		private VMIdleStates(int val, String name) {
			this.stateVal = val;
			this.stateName = name;
		}

		public int idleStateValue() {
			return this.stateVal;
		}

		public String idleStateName() {
			return this.stateName;
		}
	}

	/**
	 * Returns a double value which holds the system load average calculated for
	 * the minute preceding the call, where <i>system load average</i> is taken
	 * to mean the following:
	 * <p>
	 * The time-averaged value of the sum of the number of runnable entities
	 * running on the available processors and the number of runnable entities
	 * ready and queued to run on the available processors. The averaging
	 * technique adopted can vary depending on the underlying operating system.
	 * 
	 * @return normally, the system load average as a double. If the system load
	 *         average is not obtainable (e.g. because the calculation may
	 *         involve an unacceptable performance impact) then a negative value
	 *         is returned.
	 * @since 1.6
	 * @see java.lang.management.OperatingSystemMXBean#getSystemLoadAverage()        
	 */
	public double getCPULoad();

	/**
	 * Returns the native process identifier that identifies the current
	 * Java process to the operating system. The value is prone to being 
	 * recycled over a period of time, as considered suitable by the 
	 * operating system.
	 * 
	 * @return A long representing the process ID (pid) on the underlying 
	 * operating system.
	/*[IF Java10]
	 * 
	 * @deprecated As of Java 10. Use 
	 * {@link java.lang.management.RuntimeMXBean#getPid() getPid()} instead.
	 */
	@Deprecated(forRemoval=true, since="10")
	/*[ELSE]
	 */
	/*[ENDIF]*/
	public long getProcessID();

	/**
	 * Returns a system load average calculated over the minute preceding 
	 * the call averaged over the number of CPU available to Java virtual
	 * machine process.
	 * 
	 * @return A double indicating the average system load per processor.
	 * If the system load average is not available, it returns a negative
	 * value to indicate this. 
	 */
	public double getVMGeneratedCPULoad();

	/**
	 * Returns current JVM Idle state.
	 *
	 * @return JVM idle state value - i.e active / idle
	 */
	public VMIdleStates getVMIdleState();

	/**
	 * @return  true if JVM state is idle. Otherwise returns false
	 */
	public boolean isVMIdle();
	
	/**
	 * Query the state of the Attach API. Return false if the Attach API is:
	 * - still initializing
	 * - disabled
	 * - terminated by VM shutdown
	 * @return true if Attach API is initialized
	 * @since   1.8
	 */
	public boolean isAttachApiInitialized();

	/**
	 * Query the state of the Attach API. Return true if the Attach API is:
	 * - disabled
	 * - terminated by VM shutdown
	 * and false if it is
	 * - still initializing
	 * - initialized and running
	 * @return true if Attach API is terminated
	 * @since   1.8
	 */
	public boolean isAttachApiTerminated();

	/**
	 * This is provided for the benefit of applications which use attach API to load JVMTI agents 
	 * into their own JVMs.
	 * 
	 * @return Attach API Virtual Machine ID of this VM
	 * @since   1.8

	 */
	public String getVmId();

}
