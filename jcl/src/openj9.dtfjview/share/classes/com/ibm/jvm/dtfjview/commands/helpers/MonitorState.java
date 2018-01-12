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

import com.ibm.dtfj.java.JavaMonitor;

public class MonitorState {
	
	public static int WAITING_TO_ENTER = 1;
	public static int WAITING_TO_BE_NOTIFIED_ON = 2;

	private JavaMonitor jm;
	private int status;
	
	public MonitorState(JavaMonitor _jm, int _status)
	{
		jm = _jm;
		status = _status;
	}
	
	public JavaMonitor getMonitor()
	{
		return jm;
	}
	
	public int getStatus()
	{
		return status;
	}
	
	public String getStatusString()
	{
		if (status == WAITING_TO_ENTER) {
			return "waiting to enter";
		} else if (status == WAITING_TO_BE_NOTIFIED_ON) {
			return "waiting to be notified on";
		} else {
			return "<unknown status>";
		}
	}
	
	public static String getStatusString(int statusToGet)
	{
		if (statusToGet == WAITING_TO_ENTER) {
			return "waiting to enter";
		} else if (statusToGet == WAITING_TO_BE_NOTIFIED_ON) {
			return "waiting to be notified on";
		} else {
			return "<unknown status>";
		}
	}
}
