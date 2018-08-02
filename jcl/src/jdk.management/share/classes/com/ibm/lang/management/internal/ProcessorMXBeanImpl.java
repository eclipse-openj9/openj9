/*[INCLUDE-IF Sidecar17 & !Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

import com.ibm.java.lang.management.internal.ManagementPermissionHelper;
import com.ibm.lang.management.ProcessorMXBean;

/**
 * Runtime type for {@link ProcessorMXBean}.
 * <p>
 * Provides processor information and enables processor resource management.
 * </p>
 * 
 * @author Bjorn Vardal
 * @since 1.7.1
 */
public final class ProcessorMXBeanImpl implements ProcessorMXBean {

	// constants used to retrieve the number of cpus for each type
	final static int PHYSICAL = 1;
	final static int ONLINE = 2;
	final static int BOUND = 3;
	final static int TARGET = 4;

	private static final ProcessorMXBean instance = new ProcessorMXBeanImpl();

	/**
	 * Returns an instance of {@link ProcessorMXBeanImpl}
	 * 
	 * @return a static instance of {@link ProcessorMXBeanImpl}
	 */
	public static ProcessorMXBean getInstance() {
		return instance;
	}

	private ProcessorMXBeanImpl() {
		super();
	}

	/**
	 * Returns the object name of the MXBean
	 * 
	 * @return objectName representing the MXBean
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			ObjectName name = new ObjectName("com.ibm.lang.management:type=Processor"); //$NON-NLS-1$
			return name;
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}
	
	/**
	 * @param type
	 * 			<br />{@link PHYSICAL} - see {@link #getNumberPhysicalCPUs()}
	 * 			<br />{@link ONLINE} - see {@link #getNumberOnlineCPUs()}
	 * 			<br />{@link BOUND} - see {@link #getNumberBoundCPUs()}
	 * 			<br />{@link TARGET} - see {@link #getNumberTargetCPUs()}
	 * 
	 * @return the number corresponding to the argument <code>type</code>
	 */
	native int getNumberCPUsImpl(int type);
	
	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getNumberPhysicalCPUs() {
		return getNumberCPUsImpl(PHYSICAL);
	}
	
	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getNumberOnlineCPUs() {
		return getNumberCPUsImpl(ONLINE);
	}
	
	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getNumberBoundCPUs() {
		return getNumberCPUsImpl(BOUND);
	}
	
	/**
	 * {@inheritDoc}
	 */
	@Override
	public int getNumberTargetCPUs() {
		return getNumberCPUsImpl(TARGET);
	}
	
	/**
	 * @param number
	 * 			The number of CPUs to specify the process to use. The process
	 * 			will behave as if <code>number</code> CPUs are available. If
	 *			this is set to 0, it will reset the number of CPUs specified
	 *			and the JVM will use the the number of CPUs detected on the system.
	 * 
	 * @see #setNumberActiveCPUs(int)
	 */
	private native void setNumberActiveCPUsImpl(int number);

	/**
	 * {@inheritDoc}
	 */
		@Override
	public void setNumberActiveCPUs(int number) throws IllegalArgumentException {
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}

		if (0 > number) {
			/*[MSG "K0567", "Number of entitled CPUs cannot be negative."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0567")); //$NON-NLS-1$
		}
		setNumberActiveCPUsImpl(number);
	}
}
