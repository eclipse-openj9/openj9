/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.nativememory;

public interface INativeMemoryTypes
{

	public static final String NATIVEMEM_SECTION = "NATIVEMEMINFO";
	
	public static final String T_0MEMUSER = "0MEMUSER";
	public static final String T_1MEMUSER = "1MEMUSER";
	public static final String T_2MEMUSER = "2MEMUSER";
	public static final String T_3MEMUSER = "3MEMUSER";
	public static final String T_4MEMUSER = "4MEMUSER";
	public static final String T_5MEMUSER = "5MEMUSER";
	public static final String T_6MEMUSER = "6MEMUSER";
	
	public static final String[] T_MEMUSERS = {T_0MEMUSER, T_1MEMUSER, T_2MEMUSER, T_3MEMUSER, T_4MEMUSER, T_5MEMUSER, T_6MEMUSER}; 
	
	public static final String OTHER_CATEGORY = "Other";
	
	/* Attributes */
	
	public static final String A_DEPTH = "nativemem_depth";
	public static final String A_NAME = "nativemem_name";
	public static final String A_DEEP_BYTES = "nativemem_deepbytes";
	public static final String A_DEEP_ALLOCATIONS = "nativemem_allocations";
}
