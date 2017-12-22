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

import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.util.LinkedList;
import java.util.List;

import javax.management.ObjectName;

/**
 * Runtime type for {@link MemoryManagerMXBean}.
 * <p>
 * There is only ever one instance of this class in a virtual machine.
 * The type does not need to be modeled as a DynamicMBean, as it is structured 
 * statically, without attributes, operations, notifications, etc., configured,
 * on the fly. The StandardMBean model is sufficient for the bean type.  
 * </p>
 * @since 1.5
 */
public class MemoryManagerMXBeanImpl implements MemoryManagerMXBean {

	private final String name;

	/**
	 * The internal identifier.
	 */
	protected final int id;

	private final List<MemoryPoolMXBean> managedPoolList;

	private final ObjectName objectName;

	/**
	 * Sets the metadata for this bean. 
	 * @param objectName
	 * @param name 
	 * @param id
	 * @param memBean 
	 */
	MemoryManagerMXBeanImpl(ObjectName objectName, String name, int id, MemoryMXBeanImpl memBean) {
		super();
		this.objectName = objectName;
		this.name = name;
		this.id = id;
		this.managedPoolList = new LinkedList<>();
	}
    
    /**
     * add managed pool for this bean 
     * @param poolBean	managed pool bean
     */
	protected void addMemoryPool(MemoryPoolMXBean poolBean) {
		managedPoolList.add(poolBean);
	}

	/**
	 * @return <code>true</code> if the memory pool is managed by this memory manager;
	 *         otherwise <code>false</code>.
	 */
	native boolean isManagedPoolImpl(int id, int poolID);

	/**
	 * Retrieves the list of memory pool beans managed by this manager.
	 * 
	 * @return the list of <code>MemoryPoolMXBean</code> instances
	 */
	List<MemoryPoolMXBean> getMemoryPoolMXBeans() {
        return managedPoolList;
    }

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String[] getMemoryPoolNames() {
		String[] names = new String[managedPoolList.size()];
		int idx = 0;
		for (MemoryPoolMXBean bean : managedPoolList) {
			names[idx++] = bean.getName();
		}
		return names;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getName() {
		return this.name;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isValid() {
		return true;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		return objectName;
	}

}
