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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.util.HashMap;
import java.util.Map;

import com.ibm.j9ddr.StructureTypeManager;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

public class StructureCommandUtil 
{
	public static Context cachedContext;
	public static Map<String, StructureDescriptor> structureMap;
	public static StructureTypeManager typeManager;
	
	private static String pointerFormatString;
	
	public static Map<String, StructureDescriptor> getStructureMap(Context context)
	{
		checkContextCache(context);
		
		return structureMap;
	}
	
	public static StructureDescriptor getStructureDescriptor(String command, Context context)
	{
		checkContextCache(context);
		
		return structureMap.get(command.toLowerCase());
	}
	
	public static int getTypeCode(String type, Context context)
	{
		checkContextCache(context);
		
		return typeManager.getType(type);
	}
	
	public static String typeToCommand(String type) 
	{
		type = type.replace("struct", "");
		type = type.replace("class", "");
		type = type.replace("*", "");
		type = type.trim();
		return "!" + type.toLowerCase();
	}

	private static void checkContextCache(Context context) 
	{
		if (context != cachedContext) {
			cachedContext = context;
			loadStructureMap();
		}
	}

	private static void loadStructureMap()
	{
		structureMap = new HashMap<String, StructureDescriptor>();
		
		for (StructureDescriptor descriptor : cachedContext.vmData.getStructures()) {
			structureMap.put(descriptor.getName().toLowerCase(), descriptor);
		}
		
		typeManager = new StructureTypeManager(cachedContext.vmData.getStructures());
		
		StringBuilder b = new StringBuilder();
			
		b.append("0x%0");
		b.append(2 * cachedContext.process.bytesPerPointer());
		b.append("X");
		
		pointerFormatString = b.toString();
	}
	
	public static String formatPointer(long address, Context context)
	{
		checkContextCache(context);
		
		return String.format(pointerFormatString, address);
	}
}
