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
import java.util.Map;

import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.autoblob.linenumbers.SourceLocation;

/**
 * A variable declaration - a name and a type.
 * @author andhall
 *
 */
public class Declaration
{
	private final String name;
	private final Type type;
	private final SourceLocation location;
	private String typeOverride;
	private static Map<String, String> aliasmap;
	
	static {
		try {
			aliasmap = StructureReader.loadAliasMap(26);		//TODO get this from the command line
		} catch (Exception e) {
			//bail out at this point otherwise there will be a mis-match in data types 
			throw new RuntimeException("Failed to load alias file for superset generation. Aborting to prevent inconsistent definitions", e);
		}
	}
	
	public Declaration(String name, Type type, SourceLocation location)
	{
		this.name = name;
		this.type = type;
		this.location = location;
	}
	
	public String getName()
	{
		return name;
	}
	
	public Type getType()
	{
		return type;
	}
	
	public SourceLocation getLocation()
	{
		return location;
	}
	
	public void overrideTypeString(String typeString)
	{
		typeOverride = typeString;
	}
	
	private String getTypeString()
	{
		if (typeOverride != null) {
			return typeOverride;
		} else {
			return type.getFullName();
		}
	}
	
	public void checkForRawSRP(String parentTypeName)
	{
		String typeString = getTypeString();
		
		if (typeString.equals("J9SRP") || typeString.equals("J9WSRP")) {
			System.err.println("Warning raw SRP found: " + parentTypeName + "." + name + " : " + typeString);
		}
	}
	
	public void writeOut(String typeName, String fieldPrefix, PrintWriter out, PrintWriter ssout)
	{
		if (name == null || name.length() == 0) {
			return;
		}
		//Treat bitfields differently
		String typeString = getTypeString();
		String declaredName = fieldPrefix + name;
		String fieldname = (fieldPrefix + name).replace(".", "$");		//superset is in java format so replace . with $
		FieldDescriptor field = new FieldDescriptor(0, typeString, typeString, fieldname, declaredName);
		field.cleanUpTypes();
		field.applyAliases(aliasmap);
		
		if (typeString.contains(":") && ! typeString.contains("::")) {
			out.println("\tJ9DDRBitFieldTableEntry(\"" + fieldPrefix + name + "\"," + typeString + ")");
			ssout.println(field.deflate());
		} else {
			if (type instanceof UserDefinedType && type.isAnonymous()) {
				((UserDefinedType)type).writeFieldEntries(typeName, fieldPrefix + name + "." , out, ssout);
			} else {
				out.println("\tJ9DDRFieldTableEntry("+ typeName +  "," + fieldPrefix + name + "," + typeString + ")");
				if(!field.getDeclaredName().startsWith("_j9padding")) {
					if(name.equals("utf8.data")) {
						System.out.println("Def");
					}
					ssout.println(field.deflate());		//don't include padding fields
				}
				
			}
		}
	}
	
}
