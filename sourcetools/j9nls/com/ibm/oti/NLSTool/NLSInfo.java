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

import java.util.Vector;

public class NLSInfo implements Comparable {
	private String module;
	private String path;
	private String headerName;
	private Vector locales;
	private Vector msgInfo;
	private Comparable key;
	
	public NLSInfo() {
		locales = new Vector();
	}

	public void setModule(String module) {
		this.module = module;
		this.key = module;
	}
	
	public void setPath(String path) {
		this.path = path;
	}
	
	public void setHeaderName(String headerName) {
		this.headerName = headerName;
	}
	
	public void setLocales(Vector locales) {
		this.locales = locales;
	}
	
	public void setMsgInfo(Vector msgInfo) {
		this.msgInfo = msgInfo;
	}
	
	public String getModule() {
		return module;
	}
	
	public String getPath() {
		return path;
	}
	
	public String getHeaderName() {
		return headerName;
	}
	
	public Vector getLocales() {
		return locales;
	}
	
	public Vector getMsgInfo() {
		return msgInfo;
	}
	
	public Comparable getKey() {
		return key;
	}
	
	public void addLocale(String locale) {
		locales.addElement(locale);
	}
	
	public void addMsgInfo(MsgInfo msgInfo) {
		this.msgInfo.addElement(msgInfo);
	}
	
	public boolean isSameNLS(String module, String headerName) {
		return this.module.equalsIgnoreCase(module)	&& this.headerName.equalsIgnoreCase(headerName);
	}

	public int compareTo(Object obj) {
		return key.compareTo(((NLSInfo)obj).getKey());
	}
}
