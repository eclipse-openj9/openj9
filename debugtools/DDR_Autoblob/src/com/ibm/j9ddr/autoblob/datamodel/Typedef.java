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
import java.util.Collections;
import java.util.Map;

import com.ibm.j9ddr.autoblob.config.Configuration;
import com.ibm.j9ddr.autoblob.linenumbers.SourceLocation;

/**
 * Represents a C typedef
 * @author andhall
 *
 */
public class Typedef extends UserDefinedType
{
	private final Type aliasedType;
	
	public Typedef(String name, Type aliasedType, SourceLocation definitionLocation)
	{
		super(name, definitionLocation);
		this.aliasedType = aliasedType;
		
		aliasedType.setAliased();
	}
	
	public Type getAliasedType()
	{
		return aliasedType;
	}
	
	public Type getRootAliasedType()
	{
		Type working = aliasedType;
		
		while (working instanceof Typedef) {
			working = ((Typedef)working).aliasedType;
		}
		
		return working;
	}
	
	@Override
	public boolean shouldBeInBlob()
	{
		//Write out typedefs for UserDefinedTypes
		if (aliasedType instanceof UserDefinedType) {
			UserDefinedType t = (UserDefinedType)aliasedType;
			
			if (t instanceof ClassType) {
				return false;
			}
			
			if (t instanceof RecordType) {
				return t.isComplete;
			}
			
			return t.isComplete && t.hasConstants();
		} else {
			return false;
		}
	}

	@Override
	protected void writeConstantEntries(PrintWriter out, PrintWriter ssout)
	{
		if (aliasedType instanceof UserDefinedType) {
			((UserDefinedType)aliasedType).writeConstantEntries(out, ssout);
		}
	}

	@Override
	protected void writeFieldEntries(String name, String fieldPrefix, PrintWriter out, PrintWriter ssout)
	{
		if (aliasedType instanceof UserDefinedType) {
			((UserDefinedType)aliasedType).writeFieldEntries(this.getFullName(), fieldPrefix, out, ssout);
		}
	}

	@Override
	public void applyTypeOverrides(Configuration autoblobConfiguration)
	{
		if (aliasedType instanceof RecordType) {
			Map<String, String> overrides = autoblobConfiguration.getTypeOverrides().get(getBlobStructName());
			
			if (overrides != null) {
				((RecordType)aliasedType).applyTypeOverrides(getBlobStructName(), overrides);
			} else {
				((RecordType)aliasedType).applyTypeOverrides(getBlobStructName(), Collections.<String,String>emptyMap());
			}
		}
	}

	@Override
	public void setSuperClass(UserDefinedType superclass)
	{
		super.setSuperClass(superclass);
		
		if (aliasedType instanceof UserDefinedType) {
			((UserDefinedType)aliasedType).setSuperClass(superclass);
		}
	}

	@Override
	public void attachInnerEnum(EnumType type)
	{
		if (aliasedType instanceof UserDefinedType) {
			((UserDefinedType)aliasedType).attachInnerEnum(type);
		}
	}
	
	@Override
	public void addConstant(Constant constant)
	{
		if (aliasedType instanceof UserDefinedType) {
			((UserDefinedType)aliasedType).addConstant(constant);
		}
	}

}
