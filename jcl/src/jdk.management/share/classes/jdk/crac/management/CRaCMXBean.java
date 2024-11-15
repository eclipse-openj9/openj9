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
package jdk.crac.management;

import java.lang.management.PlatformManagedObject;
import jdk.crac.management.internal.CRaCMXBeanImpl;

/**
 * A management interface of the CRaC functionality in the Java virtual machine.
 */
public interface CRaCMXBean extends PlatformManagedObject {
	public static final String CRAC_MXBEAN_NAME = "jdk.crac.management:type=CRaC"; //$NON-NLS-1$

	/**
	 * Returns the time since the JVM restore was initiated. Returns -1 if restore has not occurred.
	 *
	 * @return the number of in millseconds since restore, returns -1 if restore has not occurred.
	 */
	public long getUptimeSinceRestore();

	/**
	 * Returns the time when the JVM restore was initiated. Returns -1 if restore has not occurred.
	 *
	 * @return the number of milliseconds since epoch, returns -1 if restore has not occurred.
	 */
	public long getRestoreTime();

	/**
	 * Returns the implementation of CRaCMXBean.
	 *
	 * @return the implementation of CRaCMXBean.
	 */
	public static CRaCMXBean getCRaCMXBean() {
		return CRaCMXBeanImpl.getInstance();
	}
}
