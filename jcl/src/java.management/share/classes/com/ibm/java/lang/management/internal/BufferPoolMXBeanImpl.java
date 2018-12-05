/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.java.lang.management.internal;

import java.lang.management.BufferPoolMXBean;
import java.util.ArrayList;
import java.util.List;

import javax.management.ObjectName;

/*[IF Java12]*/
import jdk.internal.access.JavaNioAccess.BufferPool;
import jdk.internal.access.SharedSecrets;
/*[ELSE]
/*[IF Sidecar19-SE]
import jdk.internal.misc.JavaNioAccess.BufferPool;
import jdk.internal.misc.SharedSecrets;
/*[ELSE]
import sun.misc.JavaNioAccess.BufferPool;
import sun.misc.SharedSecrets;
/*[ENDIF]*/
/*[ENDIF]*/

/**
 * The implementation MXBean for {@link java.lang.management.BufferPoolMXBean}.
 *
 * @since 1.7
 */
public final class BufferPoolMXBeanImpl implements BufferPoolMXBean {

	private static final List<BufferPoolMXBean> list;

	static {
		// we have two types of buffer pool for now
		list = new ArrayList<>(2);
		list.add(new BufferPoolMXBeanImpl(SharedSecrets.getJavaNioAccess().getDirectBufferPool(), "direct")); //$NON-NLS-1$
		list.add(new BufferPoolMXBeanImpl(sun.nio.ch.FileChannelImpl.getMappedBufferPool(), "mapped")); //$NON-NLS-1$
	}

	/**
	 * Get the list of all buffer pool beans.
	 * 
	 * @return the list of all buffer pool beans
	 */
	public static List<BufferPoolMXBean> getBufferPoolMXBeans() {
		return list;
	}

	private final ObjectName objectName;

	private final BufferPool pool;

	private BufferPoolMXBeanImpl(BufferPool pool, String poolName) {
		super();
		this.objectName = ManagementUtils.createObjectName(ManagementUtils.BUFFERPOOL_MXBEAN_DOMAIN_TYPE, poolName);
		this.pool = pool;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getCount() {
		return pool.getCount();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getMemoryUsed() {
		return pool.getMemoryUsed();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getName() {
		return pool.getName();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		return objectName;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getTotalCapacity() {
		return pool.getTotalCapacity();
	}

}
