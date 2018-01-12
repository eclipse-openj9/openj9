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

public interface IMonitorTypes {
	public static final String MONITOR_SECTION = "LOCKS";
	
	public static final String T_1LKPOOLINFO = "1LKPOOLINFO";
	public static final String T_2LKPOOLTOTAL = "2LKPOOLTOTAL";
	public static final String T_1LKMONPOOLDUMP = "1LKMONPOOLDUMP";
	
	public static final String T_2LKMONINUSE = "2LKMONINUSE";
	public static final String T_3LKMONOBJECT = "3LKMONOBJECT";
	public static final String T_3LKNOTIFYQ = "3LKNOTIFYQ";
	public static final String T_3LKWAITNOTIFY = "3LKWAITNOTIFY";
	
	public static final String T_3LKWAITERQ = "3LKWAITERQ";
	public static final String T_3LKWAITER = "3LKWAITER";
	
	public static final String T_1LKREGMONDUMP = "1LKREGMONDUMP";
	public static final String T_2LKREGMON = "2LKREGMON";
	
	
//	public static final String T_1LKDEADLOCK = "1LKDEADLOCK";
//	public static final String T_2LKDEADLOCKTHR = "2LKDEADLOCKTHR";
//	public static final String T_3LKDEADLOCKWTR = "3LKDEADLOCKWTR";
//	public static final String T_4LKDEADLOCKMON = "4LKDEADLOCKMON";
//	public static final String T_4LKDEADLOCKOBJ = "4LKDEADLOCKOBJ";
//	public static final String T_4LKDEADLOCKREG = "4LKDEADLOCKREG";
//	public static final String T_3LKDEADLOCKOWN = "3LKDEADLOCKOWN";

	
	/*
	 * Attributes
	 */
	public static final String TOTAL_MONITORS = "mon_total_monitors";
	public static final String SYSTEM_MONITOR = "mon_system_monitor";
	public static final String INFLATED_MONITOR = "mon_inflated_monitor";
	public static final String MONITOR_NAME = "mon_monitor_name";
	public static final String MONITOR_OBJECT_FULL_JAVA_NAME = "mon_obj_full_java_name";
	
	
	public static final String MONITOR_OBJECT_ADDRESS = "monitor_object_address";
	public static final String MONITOR_WORD_ADDRESS_IN_HEADER = "monitor_word_address";
	
	public static final String MONITOR_ADDRESS = "mon_monitor_address";
	
	
	public static final String UNOWNED = "mon_unowned";
	public static final String FLATLOCKED = "mon_flat_locked";
	public static final String MONITOR_THREAD_NAME = "monitor_thd_name";
	public static final String MONITOR_THREAD_ID = "monitor_thread_id";

	
	public static final String MONITOR_ENTRY_COUNT = "mon_entry_count";
	
	
	
	
}
