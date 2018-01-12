/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
package org.openj9.test.java.lang.management;

import javax.management.openmbean.CompositeData;

// This class is not public API.
import com.ibm.java.lang.management.internal.ManagementUtils;

/**
 * Centralized access to internal management classes.
 */
public final class TestUtil {

	private static final boolean isUnix;

	static {
		String osName = System.getProperty("os.name"); //$NON-NLS-1$ 

		isUnix = "aix".equalsIgnoreCase(osName) //$NON-NLS-1$
				|| "linux".equalsIgnoreCase(osName) //$NON-NLS-1$
				|| "mac os x".equalsIgnoreCase(osName) //$NON-NLS-1$
				|| "z/OS".equalsIgnoreCase(osName); //$NON-NLS-1$
	}

	/**
	* Helper function that tells whether we are running on Unix or not.
	* @return Returns true if we are running on Unix; false, otherwise.
	*/

	public static boolean isRunningOnUnix() {
		return isUnix;
	}

	public static CompositeData toCompositeData(Object data) {
		return ManagementUtils.convertToOpenType(data, CompositeData.class, data.getClass());
	}

	private TestUtil() {
		super();
	}

}
