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

import java.security.AccessControlException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * SharedAbstractHelperFactory is an abstract superclass for helper factory subclasses.
 */
public abstract class SharedAbstractHelperFactory {

	/*
	 * single id count shared by all factories
	 * (bootstrap loader is 0)
	 */
	private static final AtomicInteger idCount = new AtomicInteger(1);

	@SuppressWarnings("removal")
	static boolean checkPermission(ClassLoader loader, String type) {
		boolean result = true;
		SecurityManager sm = System.getSecurityManager();
		if (sm != null) {
			try {
				sm.checkPermission(new SharedClassPermission(loader, type));
			} catch (AccessControlException e) {
				result = false;
			}
		}
		return result;
	}

	static boolean canFind(ClassLoader loader) {
		return checkPermission(loader, "read"); //$NON-NLS-1$
	}

	static boolean canStore(ClassLoader loader) {
		return checkPermission(loader, "write"); //$NON-NLS-1$
	}

	static int getNewID() {
		return idCount.getAndIncrement();
	}

	/**
	 * Constructs a new instance of this class.
	 */
	public SharedAbstractHelperFactory() {
		super();
	}
}
