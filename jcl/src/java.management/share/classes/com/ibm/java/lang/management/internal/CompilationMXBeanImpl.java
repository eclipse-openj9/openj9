/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2016 IBM Corp. and others
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
package com.ibm.java.lang.management.internal;

import java.lang.management.CompilationMXBean;
import java.lang.management.ManagementFactory;

import javax.management.ObjectName;

/**
 * Runtime type for {@link CompilationMXBean}
 * <p>
 * There is only ever one instance of this class in a virtual machine.
 * The type does no need to be modeled as a DynamicMBean, as it is structured 
 * statically, without attributes, operations, notifications, etc, configured,
 * on the fly. The StandardMBean model is sufficient for the bean type.  
 * </p>
 * @since 1.5
 */
public final class CompilationMXBeanImpl implements CompilationMXBean {

	private static final CompilationMXBeanImpl instance = isJITEnabled() ? new CompilationMXBeanImpl() : null;

	private final ObjectName objectName;

	/**
	 * Query whether the VM is running with a JIT compiler enabled.
	 * 
	 * @return true if a JIT is enabled, false otherwise
	 */
	private static native boolean isJITEnabled();

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean. 
	 */
	private CompilationMXBeanImpl() {
		super();
		objectName = ManagementUtils.createObjectName(ManagementFactory.COMPILATION_MXBEAN_NAME);
	}

	/**
	 * Singleton accessor method.
	 * 
	 * @return the <code>ClassLoadingMXBeanImpl</code> singleton.
	 */
	public static CompilationMXBeanImpl getInstance() {
		return instance;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getName() {
		return com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("java.compiler"); //$NON-NLS-1$
	}

	/**
	 * @return the compilation time in milliseconds
	 * @see #getTotalCompilationTime()
	 */
	private native long getTotalCompilationTimeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getTotalCompilationTime() {
		if (!isCompilationTimeMonitoringSupported()) {
			/*[MSG "K05E0", "VM does not support monitoring of compilation time."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05E0")); //$NON-NLS-1$
		}
		return this.getTotalCompilationTimeImpl();
	}

	/**
	 * @return <code>true</code> if compilation timing is supported, otherwise
	 *         <code>false</code>.
	 * @see #isCompilationTimeMonitoringSupported()
	 */
	private native boolean isCompilationTimeMonitoringSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isCompilationTimeMonitoringSupported() {
		return this.isCompilationTimeMonitoringSupportedImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		return objectName;
	}

}
