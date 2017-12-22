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


public class Command extends PredicateList {

	public static final int TYPE_ALL = 0;
	public static final int TYPE_CLEAN = 1;
	public static final int TYPE_DDRGEN = 2;

	int type;
	String command;
	
	
	public Command( String containingFile ) {
		super(containingFile);
	}
	
	public void setCommand(String command) {
		this.command = command;
	}
		
	public void setType(String typeString ) {
		if ( typeString.equalsIgnoreCase("all") ) {
			type = TYPE_ALL;
		} else if ( typeString.equalsIgnoreCase("clean") ) {
			type = TYPE_CLEAN;
		} else if ( typeString.equalsIgnoreCase("ddrgen") ) {
			type = TYPE_DDRGEN;
		}
	}
	
	public String getCommand() {
		return command;
	}
	
	public int getType() {
		return type;
	}
}
 
