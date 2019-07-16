/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package openj9.management.internal;

/**
 * Container for lock information for use by classes which have access only to the
 * java.base module.
 * This is typically wrapped in a LockInfo object.
 */
public class LockInfoBase {
	private final String className;
	private final int identityHashCode;

	@SuppressWarnings("unused") /* Used only by native code. */
	private LockInfoBase(Object object) {
		this(object.getClass().getName(), System.identityHashCode(object));
	}

	/**
	 * @param classNameVal        the name (including the package prefix) of the
	 *                         associated lock object's class
	 * @param identityHashCodeVal the value of the associated lock object's identity hash code.
	 * @throws NullPointerException if <code>className</code> is <code>null</code>
	 */
	public LockInfoBase(String classNameVal, int identityHashCodeVal) {
		if (classNameVal == null) {
			/*[MSG "K0600", "className cannot be null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0600")); //$NON-NLS-1$
		}
		className = classNameVal;
		identityHashCode = identityHashCodeVal;
	}

	/**
	 * Provides callers with a string value representing the associated lock. The
	 * string will hold both the name of the lock object's class and its identity
	 * hash code expressed as an unsigned hexadecimal.
	 *
	 * @return a string containing the key details of the lock
	 */
	@Override
	public String toString() {
		return className + '@' + Integer.toHexString(identityHashCode);
	}

	/**
	 * @return class name
	 */
	public String getClassName() {
		return className;
	}

	/**
	 * @return lock's identity hash code
	 */
	public int getIdentityHashCode() {
		return identityHashCode;
	}

}
