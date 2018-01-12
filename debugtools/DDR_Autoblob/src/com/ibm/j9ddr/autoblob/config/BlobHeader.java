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
package com.ibm.j9ddr.autoblob.config;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.xml.sax.SAXException;

import com.ibm.j9ddr.autoblob.GenerateBlobC.FileTypes;
import com.ibm.j9ddr.autoblob.datamodel.ITypeCollection;
import com.ibm.j9ddr.autoblob.datamodel.UserDefinedType;

/**
 * A header to be included in the DDR blob.
 * 
 * @author andhall
 *
 */
public class BlobHeader
{
	private final String name;
	private final ConstantHandlingStrategy constantHandling;
	private final Set<String> conditionals;
	
	BlobHeader(String name, ConstantHandlingStrategy constantHandling, String conditionalString)
	{
		this.name = name;
		this.constantHandling = constantHandling;
		this.conditionals = parseConditionals(conditionalString);
	}
	
	private Set<String> parseConditionals(String conditionalString)
	{
		if (conditionalString == null) {
			return Collections.emptySet();
		}
		
		String components[] = conditionalString.split(",");
		
		Set<String> conditionals = new HashSet<String>();
		
		for (String component : components) {
			conditionals.add(component.trim());
		}
		
		return Collections.unmodifiableSet(conditionals);
	}

	public String getName()
	{
		return name;
	}
	
	public ConstantHandlingStrategy getConstantHandling()
	{
		return constantHandling;
	}

	public void writeInclude(PrintWriter writer)
	{
		writer.println("#undef UT_MODULE_LOADED");
		writer.println("#undef UT_MODULE_UNLOADED");
		boolean hasConditionals = conditionals.size() != 0;
		if (hasConditionals) {
			writer.print("#if ");
			writer.println(getConditionalString(writer));
		}
		
		writer.print("#include \"");
		writer.print(getName());
		writer.println("\"");
		
		if (hasConditionals) {
			writer.println("#endif /*" + getConditionalString(writer) + "*/");
		}
	}

	private String getConditionalString(PrintWriter writer)
	{
		boolean first = true;
		StringBuilder result = new StringBuilder();
		
		for (String conditional : conditionals) {
			if (! first) {
				result.append(" && ");
			}
			
			result.append("defined(" + conditional + ")");
			
			first = false;
		}
		
		return result.toString();
	}

	public List<UserDefinedType> loadConstants(File file, FileTypes fileTypes, ITypeCollection allTypes) throws IOException, SAXException
	{
		return constantHandling.loadConstants(file, fileTypes, allTypes);
	}
}
