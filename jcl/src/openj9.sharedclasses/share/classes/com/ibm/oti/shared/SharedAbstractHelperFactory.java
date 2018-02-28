/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

import java.security.AccessControlException;

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
 * SharedAbstractHelperFactory is an abstract superclass for helper factory subclasses.
 * <p>
 */
public abstract class SharedAbstractHelperFactory {

	private static int idCount;		/* Single id count shared by all factories */
	static Object monitor;

	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class Monitor {
		Monitor() { super(); }
	}
	
	static {
		idCount = 1;			/* bootstrap loader is 0 */
		monitor = new Monitor();		/* anything which changes idCount must synchronize on the monitor */ 
	}
	
	static boolean checkPermission(ClassLoader loader, String type) {
		boolean result = true;
		SecurityManager sm = System.getSecurityManager();
		if (sm!=null) {
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
		synchronized(monitor) {
			return idCount++;
		}
	}
}
