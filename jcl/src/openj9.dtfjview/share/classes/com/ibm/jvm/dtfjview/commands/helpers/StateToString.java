/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.helpers;

public class StateToString {
	
	private static int JVMTI_THREAD_STATE_ALIVE						=     0x0001;
	private static int JVMTI_THREAD_STATE_TERMINATED				=     0x0002;
	private static int JVMTI_THREAD_STATE_RUNNABLE					=     0x0004;
	private static int JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER	=     0x0400;
	private static int JVMTI_THREAD_STATE_WAITING					=     0x0080;
	private static int JVMTI_THREAD_STATE_WAITING_INDEFINITELY		=     0x0010;
	private static int JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT		=     0x0020;
	private static int JVMTI_THREAD_STATE_SLEEPING					=     0x0040;
	private static int JVMTI_THREAD_STATE_IN_OBJECT_WAIT			=     0x0100;
	private static int JVMTI_THREAD_STATE_PARKED					=     0x0200;
	private static int JVMTI_THREAD_STATE_SUSPENDED					=   0x100000;
	private static int JVMTI_THREAD_STATE_INTERRUPTED				=   0x200000;
	private static int JVMTI_THREAD_STATE_IN_NATIVE					=   0x400000;
	private static int JVMTI_THREAD_STATE_VENDOR_1					= 0x10000000;
	private static int JVMTI_THREAD_STATE_VENDOR_2					= 0x20000000;
	private static int JVMTI_THREAD_STATE_VENDOR_3					= 0x40000000;
	
	private static int JVMTI_JAVA_LANG_THREAD_STATE_MASK =
		JVMTI_THREAD_STATE_TERMINATED |
		JVMTI_THREAD_STATE_ALIVE |
		JVMTI_THREAD_STATE_RUNNABLE |
		JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER |
		JVMTI_THREAD_STATE_WAITING |
		JVMTI_THREAD_STATE_WAITING_INDEFINITELY |
		JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_NEW =
		0;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED =
		JVMTI_THREAD_STATE_TERMINATED;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE =
		JVMTI_THREAD_STATE_ALIVE |
		JVMTI_THREAD_STATE_RUNNABLE;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_BLOCKED =
		JVMTI_THREAD_STATE_ALIVE |
		JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_WAITING =
		JVMTI_THREAD_STATE_ALIVE |
		JVMTI_THREAD_STATE_WAITING |
		JVMTI_THREAD_STATE_WAITING_INDEFINITELY;
	private static int JVMTI_JAVA_LANG_THREAD_STATE_TIMED_WAITING =
		JVMTI_THREAD_STATE_ALIVE |
		JVMTI_THREAD_STATE_WAITING |
		JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT;

	public static String getJVMTIStateString(int state)
	{
		String retval = "";
		
		if (0 == state) {
			return "<new or terminated> ";
		}
		
		if ((state & JVMTI_THREAD_STATE_ALIVE) == JVMTI_THREAD_STATE_ALIVE) {
			retval += "ALIVE ";
		}
		if ((state & JVMTI_THREAD_STATE_TERMINATED) == JVMTI_THREAD_STATE_TERMINATED) {
			retval += "TERMINATED ";
		}
		if ((state & JVMTI_THREAD_STATE_RUNNABLE) == JVMTI_THREAD_STATE_RUNNABLE) {
			retval += "RUNNABLE ";
		}
		if ((state & JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER) == JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER) {
			retval += "BLOCKED_ON_MONITOR_ENTER ";
		}
		if ((state & JVMTI_THREAD_STATE_WAITING) == JVMTI_THREAD_STATE_WAITING) {
			retval += "WAITING ";
		}
		if ((state & JVMTI_THREAD_STATE_WAITING_INDEFINITELY) == JVMTI_THREAD_STATE_WAITING_INDEFINITELY) {
			retval += "WAITING_INDEFINITELY ";
		}
		if ((state & JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT) == JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT) {
			retval += "WAITING_WITH_TIMEOUT ";
		}
		if ((state & JVMTI_THREAD_STATE_SLEEPING) == JVMTI_THREAD_STATE_SLEEPING) {
			retval += "SLEEPING ";
		}
		if ((state & JVMTI_THREAD_STATE_IN_OBJECT_WAIT) == JVMTI_THREAD_STATE_IN_OBJECT_WAIT) {
			retval += "IN_OBJECT_WAIT ";
		}
		if ((state & JVMTI_THREAD_STATE_PARKED) == JVMTI_THREAD_STATE_PARKED) {
			retval += "PARKED ";
		}
		if ((state & JVMTI_THREAD_STATE_SUSPENDED) == JVMTI_THREAD_STATE_SUSPENDED) {
			retval += "SUSPENDED ";
		}
		if ((state & JVMTI_THREAD_STATE_INTERRUPTED) == JVMTI_THREAD_STATE_INTERRUPTED) {
			retval += "INTERRUPTED ";
		}
		if ((state & JVMTI_THREAD_STATE_IN_NATIVE) == JVMTI_THREAD_STATE_IN_NATIVE) {
			retval += "IN_NATIVE ";
		}
		if ((state & JVMTI_THREAD_STATE_VENDOR_1) == JVMTI_THREAD_STATE_VENDOR_1) {
			retval += "VENDOR_1 ";
		}
		if ((state & JVMTI_THREAD_STATE_VENDOR_2) == JVMTI_THREAD_STATE_VENDOR_2) {
			retval += "VENDOR_2 ";
		}
		if ((state & JVMTI_THREAD_STATE_VENDOR_3) == JVMTI_THREAD_STATE_VENDOR_3) {
			retval += "VENDOR_3 ";
		}
		
		if (retval.equals("")) {
			retval = "<no matching state> ";
		}
		
		return retval;
	}
	
	public static String getThreadStateString(int state)
	{
		String retval = "";
		
		state &= JVMTI_JAVA_LANG_THREAD_STATE_MASK;
		
		if (state ==  JVMTI_JAVA_LANG_THREAD_STATE_NEW) {
			return "NEW ";
		}
		if ((state & JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED) == JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED) {
			retval += "TERMINATED ";
		}
		if ((state & JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE) == JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE) {
			retval += "RUNNABLE ";
		}
		if ((state & JVMTI_JAVA_LANG_THREAD_STATE_BLOCKED) == JVMTI_JAVA_LANG_THREAD_STATE_BLOCKED) {
			retval += "BLOCKED ";
		}
		if ((state & JVMTI_JAVA_LANG_THREAD_STATE_WAITING) == JVMTI_JAVA_LANG_THREAD_STATE_WAITING) {
			retval += "WAITING ";
		}
		if ((state & JVMTI_JAVA_LANG_THREAD_STATE_TIMED_WAITING) == JVMTI_JAVA_LANG_THREAD_STATE_TIMED_WAITING) {
			retval += "TIMED_WAITING ";
		}
		
		if (retval.equals("")) {
			retval = "<no matching state> ";
		}
		
		return retval;
	}
}
