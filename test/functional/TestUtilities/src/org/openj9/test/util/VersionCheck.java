/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.util;

import java.lang.reflect.*;

/**
 * Utility class to check the Java version.
 * Checks for the Runtime.Version and uses that if available (JDK9+)
 * otherwise defaults to identifying the JDK as JDK8.
 */
public class VersionCheck {
	
	static final Object versionInstance;
	static final Method majorMethod;
	
	static {
		Object versionInstanceTemp = null;
		Method majorMethodTemp = null;
		try { 
			Method versionMethod = Runtime.class.getDeclaredMethod("version");
			versionInstanceTemp = versionMethod.invoke(null);
			majorMethodTemp = versionInstanceTemp.getClass().getDeclaredMethod("major");
		} catch(NoSuchMethodException e) {
			// Expected for Java 8
		} catch(SecurityException | IllegalAccessException | InvocationTargetException e) {
			throw new RuntimeException(e);
		}
		versionInstance = versionInstanceTemp;
		majorMethod = majorMethodTemp;
	}
	
	
	/**
	 * Get the Major version for the running JVM - either 8, 9, ....
	 * 
	 * @return The JDK major version
	 */
	public static int major() {
		if (versionInstance != null) {
			try {
				return ((Integer)majorMethod.invoke(versionInstance)).intValue();
			} catch(IllegalAccessException | InvocationTargetException e) {
				throw new RuntimeException(e);
			}
		}
		return 8;
	}
}
