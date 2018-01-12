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

import java.lang.management.ClassLoadingMXBean;
import java.lang.management.ManagementFactory;

import javax.management.ObjectName;

/**
 * Runtime type for {@link ClassLoadingMXBean}.
 * <p>
 * There is only ever one instance of this class in a virtual machine.
 * The type does no need to be modeled as a DynamicMBean, as it is structured 
 * statically, without attributes, operations, notifications, etc, configured,
 * on the fly. The StandardMBean model is sufficient for the bean type.  
 * </p>
 * @since 1.5
 */
public final class ClassLoadingMXBeanImpl implements ClassLoadingMXBean {

    private static final ClassLoadingMXBeanImpl instance = new ClassLoadingMXBeanImpl();

    private final ObjectName objectName;

    /**
     * Constructor intentionally private to prevent instantiation by others.
     * Sets the metadata for this bean.
     */
    private ClassLoadingMXBeanImpl() {
        super();
        objectName = ManagementUtils.createObjectName(ManagementFactory.CLASS_LOADING_MXBEAN_NAME);
    }

    /**
     * Singleton accessor method.
     * 
     * @return the <code>ClassLoadingMXBeanImpl</code> singleton.
     */
	public static ClassLoadingMXBeanImpl getInstance() {
        return instance;
    }

    /**
     * @return the number of loaded classes
     * @see #getLoadedClassCount()
     */
    private native int getLoadedClassCountImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
	public int getLoadedClassCount() {
        return this.getLoadedClassCountImpl();
    }

    /**
     * @return the total number of classes that have been loaded
     * @see #getTotalLoadedClassCount()
     */
    private native long getTotalLoadedClassCountImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
	public long getTotalLoadedClassCount() {
        return this.getTotalLoadedClassCountImpl();
    }

    /**
     * @return the total number of unloaded classes
     * @see #getUnloadedClassCount()
     */
    private native long getUnloadedClassCountImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
    public long getUnloadedClassCount() {
        return this.getUnloadedClassCountImpl();
    }

    /**
     * @return true if running in verbose mode
     * @see #isVerbose()
     */
    private native boolean isVerboseImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
    public boolean isVerbose() {
        return this.isVerboseImpl();
    }

    /**
     * @param value true to put the class loading system into verbose
     * mode, false to take the class loading system out of verbose mode.
     * @see #setVerbose(boolean)
     */
    private native void setVerboseImpl(boolean value);

	/**
	 * {@inheritDoc}
	 */
    @Override
    public void setVerbose(boolean value) {
        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            security.checkPermission(ManagementPermissionHelper.MPCONTROL);
        }
        this.setVerboseImpl(value);
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public ObjectName getObjectName() {
        return objectName;
    }

}
