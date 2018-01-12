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
package com.ibm.dtfj.javacore.parser.j9.section.thread;

public interface IThreadTypesSov {
	
	public static final String REGISTER_LIST = "3HPREGISTERS";
	public static final String REGISTER_LIST_VALUES = "3HPREGVALUES";
	public static final String NATIVE_STACK = "3HPNATIVESTACK";
	public static final String STACK_LINE = "3HPSTACKLINE";
	
	public static final String NATIVE_STACK_AIX = "3XHNATIVESTACK";
	public static final String STACK_LINE_AIX = "3XHSTACKLINE";
	public static final String STACK_LINE_ERR_AIX = "3XHSTACKLINEERR";

	
	/*
	 * Attributes
	 */
	public static final String STACK_LINE_ADDRESS = "stck_line_addr";
	public static final String STACK_LINE_LOCATION = "stck_line_loc";
	
	
}
