/*[INCLUDE-IF Sidecar16]*/
package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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
 * RuntimePermission objects represent access to runtime
 * support.
 *
 * @author		OTI
 * @version		initial
 */
public final class RuntimePermission extends java.security.BasicPermission {
	private static final long serialVersionUID = 7399184964622342223L;
	
/**
 * Creates an instance of this class with the given name.
 *
 * @author		OTI
 * @version		initial
 *
 * @param		permissionName String
 *					the name of the new permission.
 */
public RuntimePermission(java.lang.String permissionName)
{
	super(permissionName);
}

/**
 * Creates an instance of this class with the given name and
 * action list. The action list is ignored.
 *
 * @author		OTI
 * @version		initial
 *
 * @param		name String
 *					the name of the new permission.
 * @param		actions String
 *					ignored.
 */
public RuntimePermission(java.lang.String name, java.lang.String actions)
{
	super(name, actions);
}

}
