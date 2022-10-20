/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dtfj.javacore.parser.j9.section.threadhistory;

/**
 * Tags for the thread history.
 */
public interface IThreadHistoryTypes {

	String THREADHISTORY_SECTION = "ThreadHistory";

	String T_1XECTHTYPE = "1XECTHTYPE";
	String T_3XEHSTTYPE = "3XEHSTTYPE";
	/*
	 * Attributes
	 */
	String THREADHISTORY_NAME = "thread_history_name";
	String THREADHISTORY_ID = "thread_history_id";
	String THREADHISTORY_LINE = "thread_history_line";
}
