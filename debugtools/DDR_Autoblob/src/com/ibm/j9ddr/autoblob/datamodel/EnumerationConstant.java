/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.datamodel;

import java.io.PrintWriter;

/**
 * Constant used for enumeration values.
 * 
 * @author andhall
 *
 */
public class EnumerationConstant extends Constant
{
	private static final String CPLUSPLUS_SEPARATOR = "::";
	private String rValuePrefix;
	
	public EnumerationConstant(EnumType parent, String name)
	{
		super(name);
		
		String parentName = parent.getName();
		
		if (parentName != null && parentName.contains(CPLUSPLUS_SEPARATOR)) {
			rValuePrefix = parentName.substring(0, parentName.lastIndexOf(CPLUSPLUS_SEPARATOR) + CPLUSPLUS_SEPARATOR.length());
		} else {
			rValuePrefix = "";
		}
	}

	@Override
	public void writeOut(PrintWriter out, PrintWriter ssout)
	{
		//If our parent is an inner class, we need to prefix the value
		writeConstantTableEntry(out, ssout, name, getRValue());
	}
	
	public void setRValuePrefix(String prefix)
	{
		rValuePrefix = prefix;
	}

	private String getRValue()
	{
		return rValuePrefix + name;
	}
	
}
