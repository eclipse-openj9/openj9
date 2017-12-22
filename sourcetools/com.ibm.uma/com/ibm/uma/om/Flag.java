/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma.om;


public class Flag extends PredicateList {
	public static final int SINGLE = 0;
	public static final int GROUP = 1;
	
	String flag;
	String value = null;
	boolean cflag = true;
	boolean cxxflag = true;
	boolean cppflag = false;
	boolean asmflag = true;
	boolean definition = true;
	int type;
	String groupName;
	
	public Flag( String groupName, String containingFile) {
		super(containingFile);
		type = GROUP;
		this.groupName = groupName;
	}
	
	public Flag( String flag, String value, String containingFile ) {
		super(containingFile);
		this.flag = flag;
		if ( value != null ) {
			this.value = value;
		} 
	}
	
	public Flag( String flag, 
			String value, 
			String containingFile, 
			boolean cflag,
			boolean cxxflag,
			boolean cppflag,
			boolean asmflag,
			boolean definition ) {
		super(containingFile);
		this.flag = flag;
		if ( value != null ) {
			this.value = value;
		} 
		this.asmflag = asmflag;
		this.cflag = cflag;
		this.cxxflag = cxxflag;
		this.cppflag = cppflag;
		this.definition = definition;
	}

	public String getName() {
		return flag;
	}
	
	public String getValue() {
		return value;
	}
	
	public boolean isAsmflag() {
		return asmflag;
	}

	public void setAsmflag(boolean asmflag) {
		this.asmflag = asmflag;
	}

	public boolean isCflag() {
		return cflag;
	}

	public void setCflag(boolean cflag) {
		this.cflag = cflag;
	}

	public boolean isCppflag() {
		return cppflag;
	}

	public void setCppflag(boolean cppflag) {
		this.cppflag = cppflag;
	}

	public boolean isCxxflag() {
		return cxxflag;
	}

	public void setCxxflag(boolean cxxflag) {
		this.cxxflag = cxxflag;
	}

	public boolean isDefinition() {
		return definition;
	}

	public void setDefinition(boolean definition) {
		this.definition = definition;
	}
	
	public void setType(int type) {
		this.type = type;
	}
	
	public int getType() {
		return type;
	}
}
