/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

/**
 * Global class that provides SharedClassHelperFactory, SharedDataHelperFactory and sharing status.
 * <p>
 * @see SharedClassHelperFactory
 * @see SharedDataHelperFactory
 */
public class Shared {
	
	private static final String ENABLED_PROPERTY_NAME = "com.ibm.oti.shared.enabled"; //$NON-NLS-1$
	private static Object monitor;
	private static SharedClassHelperFactory shcHelperFactory;
	private static SharedDataHelperFactory shdHelperFactory;
	private static boolean nonBootSharingEnabled;

	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class Monitor {
		Monitor() { super(); }
	}

	static {
		monitor = new Monitor();
		/* Bootstrap class sharing is enabled by default in OpenJ9 Java 11 and up */
		nonBootSharingEnabled = isNonBootSharingEnabledImpl();
	}
	
	private static native boolean isNonBootSharingEnabledImpl();

	/**
	 * Checks if sharing is enabled for this JVM.
	 * <p>
	 * @return true if using -Xshareclasses on the command-line, false otherwise.
	 */
	public static boolean isSharingEnabled() {
		return nonBootSharingEnabled;
	}

	/**
	 * If sharing is enabled, returns a SharedClassHelperFactory, otherwise returns null.
	 * <p>
	 * @return		SharedClassHelperFactory
	 */
	public static SharedClassHelperFactory getSharedClassHelperFactory() {
		synchronized(monitor) {
			if (shcHelperFactory==null && isSharingEnabled()) {
				shcHelperFactory = new SharedClassHelperFactoryImpl();
			}
			return shcHelperFactory;
		}
	}

	/**
	 * If sharing is enabled, returns a SharedDataHelperFactory, otherwise returns null.
	 * <p>
	 * @return		SharedDataHelperFactory
	 */
	public static SharedDataHelperFactory getSharedDataHelperFactory() {
		synchronized(monitor) {
			if (shdHelperFactory==null && isSharingEnabled()) {
				shdHelperFactory = new SharedDataHelperFactoryImpl();
			}
			return shdHelperFactory;
		}
	}
}
