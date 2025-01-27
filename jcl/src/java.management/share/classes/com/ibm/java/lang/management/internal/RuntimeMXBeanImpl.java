/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2005
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.java.lang.management.internal;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;

import javax.management.ObjectName;

import com.ibm.oti.vm.VM;

/**
 * Implementation of the standard RuntimeMXBean.
 */
public class RuntimeMXBeanImpl implements RuntimeMXBean {

	private static final RuntimeMXBean instance = new RuntimeMXBeanImpl();

	private ObjectName objectName;

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean.
	 */
	protected RuntimeMXBeanImpl() {
		super();
	}

	/**
	 * Singleton accessor method.
	 *
	 * @return the <code>RuntimeMXBeanImpl</code> singleton.
	 */
	public static RuntimeMXBean getInstance() {
		return instance;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getBootClassPath() {
		if (!isBootClassPathSupported()) {
			/*[MSG "K05F5", "VM does not support boot classpath."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05F5")); //$NON-NLS-1$
		}

		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkMonitorPermission();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		return VM.internalGetProperties().getProperty("sun.boot.class.path"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getClassPath() {
		return System.getProperty("java.class.path"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getLibraryPath() {
		return System.getProperty("java.library.path"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getManagementSpecVersion() {
		return "1.0"; //$NON-NLS-1$
	}

	/**
	 * @return the name of this running virtual machine.
	 * @see #getName()
	 */
	private native String getNameImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getName() {
		return this.getNameImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getSpecName() {
		return System.getProperty("java.vm.specification.name"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getSpecVendor() {
		return System.getProperty("java.vm.specification.vendor"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getSpecVersion() {
		return System.getProperty("java.vm.specification.version"); //$NON-NLS-1$
	}

	/**
	 * @return the virtual machine start time in milliseconds.
	 * @see #getStartTime()
	 */
	private native long getStartTimeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getStartTime() {
		return this.getStartTimeImpl();
	}

	/**
	 * @return the number of milliseconds the virtual machine has been running.
	 * @see #getUptime()
	 */
	private native long getUptimeImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final long getUptime() {
		return this.getUptimeImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getVmName() {
		return System.getProperty("java.vm.name"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getVmVendor() {
		return System.getProperty("java.vm.vendor"); //$NON-NLS-1$
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final String getVmVersion() {
		return System.getProperty("java.vm.version"); //$NON-NLS-1$
	}

	/**
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 * @see #isBootClassPathSupported()
	 */
	private native boolean isBootClassPathSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final boolean isBootClassPathSupported() {
		return this.isBootClassPathSupportedImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final List<String> getInputArguments() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		checkMonitorPermission();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		return Collections.unmodifiableList(ManagementUtils.convertStringArrayToList(VM.getVMArgs()));
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final Map<String, String> getSystemProperties() {
		Map<String, String> result = new HashMap<>();
		Properties props = System.getProperties();
		Set<String> propNames = props.stringPropertyNames();
		for (String key : propNames) {
			result.put(key, props.getProperty(key));
		}
		return result;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public final ObjectName getObjectName() {
		if (objectName == null) {
			objectName = ManagementUtils.createObjectName(ManagementFactory.RUNTIME_MXBEAN_NAME);
		}
		return objectName;
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	public static void checkMonitorPermission() {
		@SuppressWarnings("removal")
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPMONITOR);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

}
