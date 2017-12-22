/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

import java.security.Permission;
import java.security.PermissionCollection;
import java.util.Enumeration;
import java.util.Hashtable;

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SharedClassPermissionCollection provides permission collection to support SharedClassPermission
 * <p>
 * @see SharedClassPermission
 */
public class SharedClassPermissionCollection extends PermissionCollection {

	private static final long serialVersionUID = 1578153523759170949L;

	private Hashtable<String, Permission> permissions = new Hashtable<>(10);

	/**
	 * Adds a permission to this collection
	 * <p>
	 * @param		perm java.security.Permission.
	 *					The Permission object to add
	 */
	@Override
	public void add(Permission perm) {
		if (!isReadOnly()) {
			Permission previous = permissions.put(perm.getName(), perm);
			// if the permission already existed but with only "read" or "write" set, then replace with both set
			if (previous != null && !previous.getActions().equals(perm.getActions()))
				permissions.put(perm.getName(), new SharedClassPermission(perm.getName(), "read,write")); //$NON-NLS-1$
		} else throw new IllegalStateException();
	}

	/**
	 * Returns permissions as an enumeration
	 * <p>
	 * @return		java.util.Enumeration.
	 * 					The Permissions as an enumeration
	 */
	@Override
	public Enumeration<Permission> elements() {
		return permissions.elements();
	}

	/**
	 * Returns <code>true</code> if the permission given is implied by any of the
	 * permissions in the collection
	 * <p>
	 * @param		perm java.security.Permission.
	 * 					The permission to check
	 * @return		java.util.Enumeration.
	 * 					The Permissions as an enumeration
	 */
	@Override
	public boolean implies(Permission perm) {
		Enumeration<Permission> elemEnum = elements();
		while (elemEnum.hasMoreElements())
			if (elemEnum.nextElement().implies(perm))
				return true;
		return false;
	}

}
