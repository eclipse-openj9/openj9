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


public class Library extends PredicateList {
	
	public static final int TYPE_BUILD = 0;
	public static final int TYPE_SYSTEM = 1;
	public static final int TYPE_EXTERNAL = 2;
	public static final int TYPE_MACRO = 3;

	String name;
	int type;
	boolean delayLoad;
	
	public Library(String containingFile) {
		super(containingFile);
	}
	
	public void setName(String name) {
		this.name = name;
	}
	
	public String getName() {
		return name;
	}
	
	public void setType(String typeString ) {
		if ( typeString.equalsIgnoreCase("build") ) {
			type = TYPE_BUILD;
		} else if ( typeString.equalsIgnoreCase("system") ) {
			type = TYPE_SYSTEM;
		} else if ( typeString.equalsIgnoreCase("external") ) {
			type = TYPE_EXTERNAL;
		} else if ( typeString.equalsIgnoreCase("macro") ) {
			type = TYPE_MACRO;
		}
	}
	
	public int getType() {
		return type;
	}
	
	public void setDelayLoad(boolean dl) {
		delayLoad = dl;
	}
	
	public boolean getDelayLoad(){
		return delayLoad;
	}
	
	@Override
	public String toString()
	{
		return this.getClass().getName() + "[name=" + getName() + ",type=" +  getType() + "]";
	}
}
