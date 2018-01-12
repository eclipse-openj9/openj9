/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.common;

public interface ICommonTypes {

	/*
	 * Misc common labels.
	 */
	public static final String COMMON = "common";

	
	
	/*
	 * Common Java Core Tags
	 */
	public static final String SECTION = "0SECTION";
	public static final String NULL = "NULL";
	
	/*
	 * Some common attributes
	 */
	public static final String JRE_NAME = "jre_name";
	public static final String JRE_VERSION = "jre_version";
	public static final String VM_VERSION = "vm_version";
	public static final String POINTER_SIZE = "pointer_size";
	public static final String BUILD_INFO = "build_info";
	public static final String JIT_PRESENT = "jit_present";
	public static final String JIT_BUILD_VERSION = "jit_build_version";
	public static final String UNAVAILABLE_DATA = "unavailable_data";

}
