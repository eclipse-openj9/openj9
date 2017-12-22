/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump;

import java.util.Map;

import com.ibm.jvm.dtfjview.Session;
import com.ibm.jvm.dtfjview.SessionProperties;

/**
 * Encapsulates the marshalling and unmarshalling of heapdump
 * settings through the properties map.
 * 
 * @author andhall
 *
 */
public class HeapDumpSettings
{
	public static final String HEAP_DUMP_FILE_PROPERTY = "heap_dump_file";
	public static final String HEAP_DUMP_FORMAT_PROPERTY = "heap_dump_format"; 
	public static final String MULTIPLE_HEAPS_MULTIPLE_FILES_PROPERTY = "heap_dump_multiple_heaps_multiple_files";
	public static final String HEAP_DUMP_RUNTIME_ID = "heap_dump_runtime_id";
	
	public static void setFileName(String fileName,Map properties) 
	{
		properties.put(HEAP_DUMP_FILE_PROPERTY, fileName);
	}
	
	/**
	 * Returns the filename to use for writing out a heap dump, either based
	 * on what the user has set or by generating the default name based on the 
	 * image name
	 */
	public static String getFileName(Map properties)
	{
		String propertyFileName = (String) properties.get(HEAP_DUMP_FILE_PROPERTY);
		
		if(propertyFileName != null) {
			return propertyFileName;
		} else {
			return getDefaultHeapDumpFileName(properties);
		}
	}
	
	public static boolean heapDumpFileNameSet(Map properties) {
		return properties.containsKey(HEAP_DUMP_FILE_PROPERTY);
	}
	
	private static String getDefaultHeapDumpFileName(Map properties)
	{
		String baseFileName = (String) properties.get(SessionProperties.CORE_FILE_PATH_PROPERTY);
		String runtimeID = (String) properties.get(HEAP_DUMP_RUNTIME_ID);
		if( runtimeID == null ) {
			runtimeID = "";
		}
		
		if(areHeapDumpsPHD(properties)) {
			return baseFileName + runtimeID +".phd";
		} else {
			return baseFileName + runtimeID +".txt";
		}
	}
	
	public static void setClassicHeapDumps(Map properties)
	{
		properties.put(HEAP_DUMP_FORMAT_PROPERTY, "classic");
	}
	
	public static boolean areHeapDumpsPHD(Map properties) 
	{
		Object formatValue = properties.get(HEAP_DUMP_FORMAT_PROPERTY);
		
		if(formatValue != null && formatValue.equals("classic")) {
			return false;
		} else {
			return true;
		}
	}
	
	public static void setPHDHeapDumps(Map properties)
	{
		properties.put(HEAP_DUMP_FORMAT_PROPERTY, "phd");
	}

	public static void setMultipleHeapsMultipleFiles(Map properties)
	{
		properties.put(MULTIPLE_HEAPS_MULTIPLE_FILES_PROPERTY, "true");
	}
	
	public static void setMultipleHeapsSingleFile(Map properties)
	{
		properties.put(MULTIPLE_HEAPS_MULTIPLE_FILES_PROPERTY, "false");
	}
	
	public static void setRuntimeID(Map properties, int id) {
		properties.put(HEAP_DUMP_RUNTIME_ID, "." + id);
	}
	
	public static boolean multipleHeapsInMultipleFiles(Map properties)
	{
		Object multipleFilesValue = properties.get(MULTIPLE_HEAPS_MULTIPLE_FILES_PROPERTY);
		
		if(multipleFilesValue == null) {
			return false;
		} else {
			return multipleFilesValue.equals("true");
		}
	}
}
