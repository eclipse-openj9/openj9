/*[INCLUDE-IF Sidecar18-SE]*/
package com.ibm.oti.util;

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
 * RuntimePermission objects represent access to runtime
 * support.
 *
 * @author		IBM
 * @version		initial
 */
public class RuntimePermissions {

	/**
	 * RuntimePermission "accessDeclaredMembers"
	 */
	public static final RuntimePermission permissionAccessDeclaredMembers = new RuntimePermission("accessDeclaredMembers");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "enableContextClassLoaderOverride"
	 */
	public static final RuntimePermission permissionEnableContextClassLoaderOverride = new RuntimePermission("enableContextClassLoaderOverride");	//$NON-NLS-1$
	/**
	 * RuntimePermission "getClassLoader"
	 */
	public static final RuntimePermission permissionGetClassLoader = new RuntimePermission("getClassLoader");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "getProtectionDomain"
	 */
	public static final RuntimePermission permissionGetProtectionDomain = new RuntimePermission("getProtectionDomain");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "getStackTrace"
	 */
	public static final RuntimePermission permissionGetStackTrace = new RuntimePermission("getStackTrace");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "modifyThreadGroup"
	 */
	public static final RuntimePermission permissionModifyThreadGroup = new RuntimePermission("modifyThreadGroup");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "setContextClassLoader"
	 */
	public static final RuntimePermission permissionSetContextClassLoader = new RuntimePermission("setContextClassLoader");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "setDefaultUncaughtExceptionHandler"
	 */
	public static final RuntimePermission permissionSetDefaultUncaughtExceptionHandler = new RuntimePermission("setDefaultUncaughtExceptionHandler");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "setIO"
	 */
	public static final RuntimePermission permissionSetIO = new RuntimePermission("setIO");	//$NON-NLS-1$ 
	/**
	 * RuntimePermission "setSecurityManager"
	 */
	public static final RuntimePermission permissionSetSecurityManager = new RuntimePermission("setSecurityManager");	//$NON-NLS-1$
	/**
	 * RuntimePermission "stopThread"
	 */
	public static final RuntimePermission permissionStopThread = new RuntimePermission("stopThread");	//$NON-NLS-1$
	/**
	 * RuntimePermission "loggerFinder"
	 */
	public static final RuntimePermission permissionLoggerFinder = new RuntimePermission("loggerFinder");	//$NON-NLS-1$
	/**
	 * RuntimePermission "defineClass"
	 */
	public static final RuntimePermission permissionDefineClass = new RuntimePermission("defineClass");	//$NON-NLS-1$

}
