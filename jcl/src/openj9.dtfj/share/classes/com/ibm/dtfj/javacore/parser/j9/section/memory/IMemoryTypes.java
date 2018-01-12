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
package com.ibm.dtfj.javacore.parser.j9.section.memory;

public interface IMemoryTypes {

	public static final String MEMORY_SECTION = "MEMINFO";
	public static final String T_1STSEGTYPE = "1STSEGTYPE";
	public static final String T_1STSEGMENT = "1STSEGMENT";
	public static final String T_1STHEAPFREE = "1STHEAPFREE";
	public static final String T_1STHEAPALLOC = "1STHEAPALLOC";
	public static final String T_1STGCHTYPE = "1STGCHTYPE";
	public static final String T_3STHSTTYPE = "3STHSTTYPE";

	/*
	 * Attributes
	 */
	public static final String MEMORY_SEGMENT_NAME = "memory_segment_name";
	public static final String MEMORY_SEGMENT_ID = "memory_segment_id";
	public static final String MEMORY_SEGMENT_HEAD = "memory_segment_head";
	public static final String MEMORY_SEGMENT_SIZE = "memory_segment_size";
	public static final String MEMORY_SEGMENT_FREE = "memory_segment_free";
	public static final String MEMORY_SEGMENT_TAIL = "memory_segment_tail";
	public static final String MEMORY_SEGMENT_TYPE = "memory_segment_type";

}
