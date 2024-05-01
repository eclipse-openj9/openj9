/*[INCLUDE-IF SharedClasses]*/
/*
 * Copyright IBM Corp. and others 1998
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
package com.ibm.oti.shared;

/**
 * Global class that provides SharedClassHelperFactory, SharedDataHelperFactory and sharing status.
 *
 * @see SharedClassHelperFactory
 * @see SharedDataHelperFactory
 */
public class Shared {

	private static final String ENABLED_PROPERTY_NAME = "com.ibm.oti.shared.enabled"; //$NON-NLS-1$
	private static final Object monitor;
	private static SharedClassHelperFactory shcHelperFactory;
	private static SharedDataHelperFactory shdHelperFactory;
	private static final boolean nonBootSharingEnabled;

	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class Monitor {
		Monitor() {
			super();
		}
	}

	static {
		monitor = new Monitor();
		/* Bootstrap class sharing is enabled by default in OpenJ9 Java 11 and up. */
		nonBootSharingEnabled = isNonBootSharingEnabledImpl();
	}

	private static native boolean isNonBootSharingEnabledImpl();

	/**
	 * Checks if sharing is enabled for this JVM.
	 *
	 * @return true if using -Xshareclasses on the command-line, false otherwise
	 */
	public static boolean isSharingEnabled() {
		return nonBootSharingEnabled;
	}

	/**
	 * If sharing is enabled, returns a SharedClassHelperFactory, otherwise returns null.
	 *
	 * @return SharedClassHelperFactory
	 */
	public static SharedClassHelperFactory getSharedClassHelperFactory() {
		synchronized (monitor) {
			if (shcHelperFactory == null && isSharingEnabled()) {
				shcHelperFactory = new SharedClassHelperFactoryImpl();
			}
			return shcHelperFactory;
		}
	}

	/**
	 * If sharing is enabled, returns a SharedDataHelperFactory, otherwise returns null.
	 *
	 * @return SharedDataHelperFactory
	 */
	public static SharedDataHelperFactory getSharedDataHelperFactory() {
		synchronized (monitor) {
			if (shdHelperFactory == null && isSharingEnabled()) {
				shdHelperFactory = new SharedDataHelperFactoryImpl();
			}
			return shdHelperFactory;
		}
	}

	/*[IF]*/
	/*
	 * This explicit constructor is equivalent to the one implicitly defined
	 * in earlier versions. It was probably never intended to be public nor
	 * was it likely intended to be used, but rather than break any existing
	 * uses, we simply mark it deprecated.
	 */
	/*[ENDIF]*/
	/**
	 * Constructs a new instance of this class.
	 */
	@Deprecated
	public Shared() {
		super();
	}

}
