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
 * @author andhall
 *
 */
public class Constant implements Comparable<Constant>
{

	protected final String name;
	protected final String value;
	
	public Constant(String name)
	{
		this(name, null);
	}
	
	public Constant(String name, String value)
	{
		this.name = name;
		this.value = value;
	}
	
	public void writeOut(PrintWriter out, PrintWriter ssout)
	{
		//TODO check that just writing out the constant and it ultimately doesn't matter if it is in this part of the build is valid or not
		if (value == null) {
			out.println("#if defined(" + name + ")");
			writeConstantTableEntry(out, ssout, name, name);
			out.println("#endif");
		} else {
			writeConstantTableEntry(out, ssout, name, value);
		}
	}

	protected void writeConstantTableEntry(PrintWriter out, PrintWriter ssout, String n, String v)
	{
		if (v.startsWith("#if")) {
			out.println(v);
			out.println("\tJ9DDRConstantTableEntryWithValue(\"" + n + "\",1)");
			out.println("#else");
			out.println("\tJ9DDRConstantTableEntryWithValue(\"" + n + "\",0)");
			out.println("#endif");
		} else {
			out.println("\tJ9DDRConstantTableEntryWithValue(\"" + n + "\"," + v + ")");
		}
		ssout.println("C|" + n);
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		result = prime * result + ((value == null) ? 0 : value.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof Constant)) {
			return false;
		}
		Constant other = (Constant) obj;
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		if (value == null) {
			if (other.value != null) {
				return false;
			}
		} else if (!value.equals(other.value)) {
			return false;
		}
		return true;
	}

	public int compareTo(Constant o)
	{
		return name.compareTo(o.name);
	}
	
	
}
