/*[INCLUDE-IF CRAC_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2024
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
package jdk.crac.management.internal;

import java.util.concurrent.TimeUnit;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import jdk.crac.management.CRaCMXBean;
import openj9.internal.criu.InternalCRIUSupport;

/**
 * An implementation of the CRaCMXBean interface.
 */
public class CRaCMXBeanImpl implements CRaCMXBean {
	private final static CRaCMXBean INSTANCE = createInstance();

	private CRaCMXBeanImpl() {
	}

	private static CRaCMXBeanImpl createInstance() {
		return new CRaCMXBeanImpl();
	}

	/**
	 * Returns an instance of {@link CRaCMXBeanImpl}.
	 *
	 * @return an instance of {@link CRaCMXBeanImpl}.
	 */
	public static CRaCMXBean getInstance() {
		return INSTANCE;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getUptimeSinceRestore() {
		long restoreTime = getRestoreTime();
		return (restoreTime > 0) ? (System.currentTimeMillis() - restoreTime) : -1;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getRestoreTime() {
		long processRestoreStartTime = InternalCRIUSupport.getProcessRestoreStartTime();
		return (processRestoreStartTime > 0) ? TimeUnit.NANOSECONDS.toMillis(processRestoreStartTime) : -1;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		try {
			return ObjectName.getInstance(CRAC_MXBEAN_NAME);
		} catch (MalformedObjectNameException e) {
			throw new InternalError(e);
		}
	}
}
