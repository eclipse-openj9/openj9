/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2016 IBM Corp. and others
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
package com.ibm.virtualization.management.internal;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

import com.ibm.virtualization.management.HypervisorInfoRetrievalException;
import com.ibm.virtualization.management.HypervisorMXBean;

/**
 * Runtime type for {@link HypervisorMXBean}.
 * @since 1.7.1
 */
public final class HypervisorMXBeanImpl implements HypervisorMXBean {

	/*
	 * Defines for the Hypervisor, these should be synchronized with the native
	 * implementation.
	 */
	private static final int HYPERVISOR_NOT_SUPPORTED = -100001;
	private static final int RUNNING_ON_HYPERVISOR = 1;
	private static final int NOT_RUNNING_ON_HYPERVISOR = 0;

	private static final HypervisorMXBeanImpl instance = new HypervisorMXBeanImpl();

	private int isVirtual = isEnvironmentVirtualImpl();
	private String hypervisorName = getVendorImpl();

	/**
	 * Singleton accessor method.
	 * 
	 * @return the {@link HypervisorMXBeanImpl} singleton.
	 */
	public static HypervisorMXBeanImpl getInstance() {
		return instance;
	}

	private HypervisorMXBeanImpl() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getVendor() {
		return hypervisorName;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isEnvironmentVirtual() throws UnsupportedOperationException, HypervisorInfoRetrievalException {
		/* Hypervisor not supported */
		if (isVirtual == HYPERVISOR_NOT_SUPPORTED) {
			/*[MSG "K0565", "Unsupported Hypervisor error"]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0565")); //$NON-NLS-1$
		} else if (isVirtual < 0) {
			/* Any other detection errors */
			/*[MSG "K0566", "Error during Hypervisor detection error={0}"]*/
			throw new HypervisorInfoRetrievalException(com.ibm.oti.util.Msg.getString("K0566", isVirtual)); //$NON-NLS-1$
		} else if (isVirtual == RUNNING_ON_HYPERVISOR) {
			return true; /* Running on a Hypervisor */
		} else if (isVirtual == NOT_RUNNING_ON_HYPERVISOR) {
			return false;
		}
		return false;
	}

	/**
	 * Returns the object name of the MXBean.
	 * 
	 * @return objectName representing the MXBean.
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			ObjectName name = new ObjectName("com.ibm.virtualization.management:type=Hypervisor"); //$NON-NLS-1$
			return name;
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}

	/**
	 * Query whether the Environment is Virtual or not.
	 * 
	 * @return true if Operating System is running on a Hypervisor Host.
	 */
	private native int isEnvironmentVirtualImpl();

	/**
	 * Query the Hypervisor Vendor Name.
	 * 
	 * @return string identifying the vendor of the Hypervisor if running on a Hypervisor, null otherwise.
	 */
	private native String getVendorImpl();

}
