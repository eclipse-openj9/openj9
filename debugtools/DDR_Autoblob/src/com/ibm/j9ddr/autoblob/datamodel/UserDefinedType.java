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
import java.util.SortedSet;
import java.util.TreeSet;

import com.ibm.j9ddr.autoblob.config.Configuration;
import com.ibm.j9ddr.autoblob.linenumbers.SourceLocation;

/**
 * A C type defined by the user.
 * 
 * In addition to Type, has a definition location and can be tagged
 * with constants.
 * 
 * @author andhall
 *
 */
public abstract class UserDefinedType extends Type implements Comparable<UserDefinedType>
{	
	protected SortedSet<Constant> constants = new TreeSet<Constant>();
	
	private final SourceLocation location;
	
	protected UserDefinedType superClass;
	
	protected boolean isComplete = true;
	
	public UserDefinedType(String name, SourceLocation definitionLocation)
	{
		super(name);
		this.location = definitionLocation;
	}

	public SourceLocation getDefinitionLocation()
	{
		return location;
	}
	
	public void addConstant(Constant constant)
	{
		constants.add(constant);
	}
	
	public final boolean isAnonymous()
	{
		return getName() == null;
	}
	
	public final void writeFieldsAndConstantsAsBlobC(PrintWriter out, PrintWriter ssout)
	{
		String name = getBlobStructName();
		name = name.replace("__", "$");			//replicate the runtime adjustment made when reading structures directly from the blob
		if(superClass != null) {
			String sname = superClass.getBlobStructName().replace("__", "$");
			ssout.println("S|" + name + "|" + name + "Pointer|" + sname);
		} else {
			ssout.println("S|" + name + "|" + name + "Pointer|");
		}

		writeFields(out, ssout);
	
		writeConstants(out, ssout);
	}
		
	private void writeConstants(PrintWriter out, PrintWriter ssout)
	{
		out.println("J9DDRConstantTableBegin(" + getBlobStructName() + ")");
		
		writeConstantEntries(out, ssout);
		
		out.println("J9DDRConstantTableEnd");
		out.println();
	}

	protected void writeConstantEntries(PrintWriter out, PrintWriter ssout)
	{
		for (Constant constant : constants) {
			constant.writeOut(out, ssout);	
		}
	}

	private void writeFields(PrintWriter out, PrintWriter ssout)
	{
		out.println("J9DDRFieldTableBegin(" + getBlobStructName() + ")");
		
		writeFieldEntries(getFullName(), "", out, ssout);
		
		out.println("J9DDRFieldTableEnd");
	}

	protected void writeFieldEntries(String typeName, String fieldPrefix, PrintWriter out, PrintWriter ssout)
	{
		//No-op by default
	}
	
	public boolean shouldBeInBlob()
	{
		return isComplete && hasConstants() && !isAliased();
	}
	
	protected boolean hasConstants()
	{
		return constants.size() > 0;
	}
	
	protected String getBlobStructName()
	{
		return getName().replaceAll("::", "__");
	}

	public final void writeStructDefinitionAsBlobC(PrintWriter out)
	{
		if (! isEmptyStruct()) {
			if (! getBlobStructName().equals(getFullName())) {
				out.println("\tJ9DDRStructWithName(" + getFullName () + "," + getBlobStructName() + ", " + getSuperClassString() + ")");
			} else {
				out.println("\tJ9DDRStruct(" + getBlobStructName() + ", " + getSuperClassString() + ")");
			}
		} else {
			out.println("\tJ9DDREmptyStruct(" + getBlobStructName() + ", " + getSuperClassString() + ")");
		}
	}

	protected boolean isEmptyStruct()
	{
		return false;
	}

	private String getSuperClassString()
	{
		if (superClass == null) {
			return "NULL";
		} else {
			return "\"" + superClass.getBlobStructName() + "\"";
		}
	}

	public void applyTypeOverrides(Configuration autoblobConfiguration)
	{
		//Do nothing by default
	}

	public void setSuperClass(UserDefinedType superclass)
	{
		this.superClass = superclass;
	}
	
	public void setComplete(boolean complete)
	{
		isComplete = complete;
	}

	public void attachInnerEnum(EnumType type)
	{
		throw new UnsupportedOperationException("Should have been overloaded on base class " + this.getClass().getName());
	}

	public int compareTo(UserDefinedType o)
	{
		if (name == null && o.name != null) {
			return -1;
		} else if (name == null && o.name == null) {
			return 0;
		} else if (name != null && o.name == null) {
			return 1;
		} else {
			return name.compareTo(o.name);
		}
	}
	

	public boolean equals(Object o)
	{
		if (o instanceof UserDefinedType) {
			return (0 == compareTo((UserDefinedType)o));
		} else {
			return false;
		}
	}
	
	
	public int hashCode()
	{
		return name.hashCode();
	}
	
}
