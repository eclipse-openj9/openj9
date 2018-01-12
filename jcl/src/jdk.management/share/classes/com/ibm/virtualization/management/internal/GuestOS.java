/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2016 IBM Corp. and others
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

import com.ibm.virtualization.management.GuestOSInfoRetrievalException;
import com.ibm.virtualization.management.GuestOSMXBean;
import com.ibm.virtualization.management.GuestOSMemoryUsage;
import com.ibm.virtualization.management.GuestOSProcessorUsage;

/**
 * Runtime type for {@link GuestOSMXBean}.
 * <p>
 * Implements retrieving Guest (Virtual Machine(VM)/Logical Partition(LPAR))
 * Processor and Memory usage statistics from the Hypervisor Host.
 * </p>
 *
 * @author Dinakar Guniguntala
 * @since 1.7.1
 */
public final class GuestOS implements GuestOSMXBean {

	private static final GuestOS instance = new GuestOS();

	/**
	 * Singleton accessor method.
	 *
	 * @return the {@link GuestOS} singleton.
	 */
	public static GuestOS getInstance() {
		return instance;
	}

	/**
	 * Empty constructor intentionally private to prevent instantiation by others.
	 */
	private GuestOS() {
		super();
	}

	/**
	 * Returns the object name of the MXBean.
	 *
	 * @return objectName representing the MXBean.
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			return new ObjectName("com.ibm.virtualization.management:type=GuestOS"); //$NON-NLS-1$
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public GuestOSProcessorUsage retrieveProcessorUsage() throws GuestOSInfoRetrievalException {
		GuestOSProcessorUsage gpUsage = new GuestOSProcessorUsage();

		return retrieveProcessorUsageImpl(gpUsage);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public GuestOSProcessorUsage retrieveProcessorUsage(GuestOSProcessorUsage gpUsage)
			throws NullPointerException, GuestOSInfoRetrievalException {
		/* If user object is NULL throw an exception */
		if (null == gpUsage) {
			throw new NullPointerException();
		}

		/* Return current snapshot of Processor usage statistics */
		return retrieveProcessorUsageImpl(gpUsage);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public GuestOSMemoryUsage retrieveMemoryUsage() throws GuestOSInfoRetrievalException {
		/* Instantiate a new object of type GuestOSMemoryUsage */
		GuestOSMemoryUsage gmUsage = new GuestOSMemoryUsage();

		/* Return current snapshot of Memory usage statistics */
		return retrieveMemoryUsageImpl(gmUsage);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public GuestOSMemoryUsage retrieveMemoryUsage(GuestOSMemoryUsage gmUsage)
			throws NullPointerException, GuestOSInfoRetrievalException {
		/* If user object is NULL throw an exception */
		if (null == gmUsage) {
			throw new NullPointerException();
		}

		/* Return current snapshot of Memory usage statistics */
		return retrieveMemoryUsageImpl(gmUsage);
	}

	/* Native implementation that returns usage statistics filled in */
	private native GuestOSProcessorUsage retrieveProcessorUsageImpl(GuestOSProcessorUsage gpUsage);

	private native GuestOSMemoryUsage retrieveMemoryUsageImpl(GuestOSMemoryUsage gmUsage);

}
