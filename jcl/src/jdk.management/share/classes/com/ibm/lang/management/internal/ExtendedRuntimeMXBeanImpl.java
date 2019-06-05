/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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

import com.ibm.java.lang.management.internal.RuntimeMXBeanImpl;
import com.ibm.lang.management.RuntimeMXBean;
import com.ibm.tools.attach.target.AttachHandler;


/**
 * Runtime type for {@link com.ibm.lang.management.RuntimeMXBean}.
 *
 * @since 1.5
 */
public final class ExtendedRuntimeMXBeanImpl extends RuntimeMXBeanImpl implements RuntimeMXBean {

	private static final RuntimeMXBean instance = new ExtendedRuntimeMXBeanImpl();

	private static final ExtendedOperatingSystemMXBeanImpl os = ExtendedOperatingSystemMXBeanImpl.getInstance();

	/**
	 * Singleton accessor method.
	 * 
	 * @return the <code>RuntimeMXBeanImpl</code> singleton.
	 */
	public static RuntimeMXBean getInstance() {
		return instance;
	}

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean.
	 */
	private ExtendedRuntimeMXBeanImpl() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public double getCPULoad() {
		return os.getSystemLoadAverage();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getProcessID() {
		return getProcessIDImpl();
	}

	private native long getProcessIDImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public double getVMGeneratedCPULoad() {
		return os.getSystemLoadAverage() / os.getAvailableProcessors();
	}

	private native int getVMIdleStateImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public VMIdleStates getVMIdleState() {
		int vmIdleState = getVMIdleStateImpl();

		for (VMIdleStates idleState : VMIdleStates.values()) {
			if (vmIdleState == idleState.idleStateValue()) {
				return idleState;
			}
		}
		/* control never reaches here */
		return VMIdleStates.INVALID;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isVMIdle() {
		if (VMIdleStates.IDLE == getVMIdleState()) {
			return true;
		}
		return false;
	}

	@Override
	public boolean isAttachApiInitialized() {
		checkMonitorPermission();
		return AttachHandler.isAttachApiInitialized();
	}

	@Override
	public boolean isAttachApiTerminated() {
		checkMonitorPermission();
		return AttachHandler.isAttachApiTerminated();
	}

	@Override
	public String getVmId() {
		checkMonitorPermission();
		return AttachHandler.getVmId();
	}
}
