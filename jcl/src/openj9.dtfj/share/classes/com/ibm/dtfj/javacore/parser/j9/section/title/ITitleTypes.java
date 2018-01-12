/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.title;

public interface ITitleTypes {

	public static final String TITLE_SECTION = "TITLE";
	public static final String T_1TISIGINFO = "1TISIGINFO";
	public static final String T_1TIDATETIME = "1TIDATETIME";
	public static final String T_1TIFILENAME = "1TIFILENAME";
	public static final String T_1TINANOTIME = "1TINANOTIME";
	
	/*
	 * Attributes
	 */
	public static final String TI_DATE = "title_date";
	public static final String TI_FILENAME = "title_filename";
	public static final String TI_NANO = "title_nanotime";
}
