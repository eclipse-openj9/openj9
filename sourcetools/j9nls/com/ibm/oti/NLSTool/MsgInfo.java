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
package com.ibm.oti.NLSTool;

public class MsgInfo {
	private String macro;
	private int id;
	private String key;
	private String msg;
	private String explanation;
	private String systemAction;
	private String userResponse;
	
	public MsgInfo() {
	}

	public void setMacro(String macro) {
		this.macro = macro;
	}
	
	public void setId(int id) {
		this.id = id;
	}
	
	public void setKey(String key) {
		this.key = key;
	}
	
	public void setMsg(String msg) {
		this.msg = msg;
	}
	
	public String getMacro() {
		return macro;
	}
	
	public int getId() {
		return id;
	}
	
	public String getKey() {
		return key;
	}
	
	public String getMsg() {
		return msg;
	}

	public String getSystemAction() {
		return systemAction;
	}
	
	public void setSystemAction(String systemAction) {
		this.systemAction = systemAction;
	}
	
	public String getUserResponse() {
		return userResponse;
	}
	
	public void setUserResponse(String userResponse) {
		this.userResponse = userResponse;
	}

	public String getExplanation() {
		return explanation;
	}

	public void setExplanation(String explanation) {
		this.explanation = explanation;
	}
	
	public boolean containsDiagnostics() {
		return 
			getUserResponse() != null || 
			getExplanation() != null || 
			getSystemAction() != null;
	}
}
