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
package com.ibm.dtfj.javacore.parser.j9.section.monitor;

public interface IMonitorTypesSov {
	public static final String LK_POOL_INIT = "2LKPOOLINIT";
	public static final String LK_POOL_EXP_NUM = "2LKPOOLEXPNUM";
	public static final String LK_POOL_EXP_BY = "2LKPOOLEXPBY";
	public static final String LK_POOL_FREE = "2LKPOOLFREE";
	
	public static final String LK_FLAT_MON_DUMP = "1LKFLATMONDUMP";
	public static final String LK_FLAT_MON = "2LKFLATMON";
	public static final String LK_OBJ_MON_DUMP = "1LKOBJMONDUMP";
	public static final String LK_INFLATED_MON = "2LKINFLATEDMON";
	public static final String LK_INFL_DETAILS = "3LKINFLDETAILS";
	public static final String LK_FLAT_LOCKED = "2LKFLATLOCKED";
	public static final String LK_FLAT_DETAILS = "3LKFLATDETAILS";
	
	public static final String MONITOR_THREAD_EE = "monitor_thread_ee";
	public static final String MONITOR_FLAT_ID = "monitor_flat_id";
}
