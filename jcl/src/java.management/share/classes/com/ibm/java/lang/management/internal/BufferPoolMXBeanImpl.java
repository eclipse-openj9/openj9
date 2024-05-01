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

import java.lang.management.BufferPoolMXBean;
import java.util.ArrayList;
import java.util.List;

import javax.management.ObjectName;

/*[IF JAVA_SPEC_VERSION >= 15]*/
import jdk.internal.misc.VM.BufferPool;
import jdk.internal.access.SharedSecrets;
/*[ELSEIF JAVA_SPEC_VERSION >= 9] JAVA_SPEC_VERSION >= 15 */
import jdk.internal.misc.JavaNioAccess.BufferPool;
import jdk.internal.misc.SharedSecrets;
/*[ELSE] JAVA_SPEC_VERSION >= 15 */
import sun.misc.JavaNioAccess.BufferPool;
import sun.misc.SharedSecrets;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

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

	private ObjectName objectName;

	private final BufferPool pool;

	private final String poolName;

	private BufferPoolMXBeanImpl(BufferPool pool, String poolName) {
		super();
		this.pool = pool;
		this.poolName = poolName;
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
		if (objectName == null) {
			objectName = ManagementUtils.createObjectName(ManagementUtils.BUFFERPOOL_MXBEAN_DOMAIN_TYPE, poolName);
		}
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
