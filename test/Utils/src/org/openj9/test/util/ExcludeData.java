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
package org.openj9.test.util;

import java.util.ArrayList;

public class ExcludeData {
	private String methodsToExclude;
	private String className;
	private String defectNumber;
	private ArrayList<String> excludeGroupNames;
	
	public ExcludeData(String methodsToExclude, String className, String defectNumber, ArrayList<String> excludeGroupNames) {
		this.methodsToExclude = methodsToExclude;
		this.className = className;
		this.defectNumber = defectNumber;
		this.excludeGroupNames = new ArrayList<String> (excludeGroupNames);
	}
	
	public String getMethodsToExclude() { return methodsToExclude;}
	public String getClassName() { return className;}
	public String getDefectNumber() { return defectNumber;}
	public ArrayList<String> getExcludeGroupNames() { return excludeGroupNames;}
	
	public void setMethodsToExclude(String methodsToExclude) { this.methodsToExclude = methodsToExclude; }
	public void setClassName(String className) { this.className = className; }
	public void setDefectNumber(String defectNumber) { this.defectNumber = defectNumber; }
	public void setMethodsToExclude(ArrayList<String> excludeGroupNames) {this.excludeGroupNames = excludeGroupNames; }
	
}
