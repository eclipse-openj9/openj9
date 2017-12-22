/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

/**
 * Interface which lists all the system properties which are recognised by jdmpview.
 * 
 * @author adam
 *
 */
public interface SystemProperties {
	/**
	 * The DTFJ factory used to create an Image from
	 */
	public static final String SYSPROP_FACTORY = "com.ibm.dtfj.image.factoryclass";
	
	/**
	 * Launcher to use 
	 */
	public static final String SYSPROP_LAUNCHER = "com.ibm.jvm.dtfjview.launcher";
	
	/**
	 * Normally jdmpview will terminate with System.exit if an error is encountered when starting up.
	 * This allows batch / scripts to detect abnormal terminations. However for interactive sessions
	 * setting this property changes this behaviour so that a runtime exception can be raised and 
	 * handled by the invoking process.
	 */
	public static final String SYSPROP_NOSYSTEMEXIT = "com.ibm.jvm.dtfjview.nosystemexit";
}
