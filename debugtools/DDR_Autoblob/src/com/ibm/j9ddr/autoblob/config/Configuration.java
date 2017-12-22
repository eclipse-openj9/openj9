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
import java.io.FileInputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

import com.ibm.j9ddr.autoblob.datamodel.PseudoType;
import com.ibm.j9ddr.autoblob.datamodel.UserDefinedType;

/**
 * AutoBlob configuration.
 * 
 * @author andhall
 *
 */
public class Configuration
{
	public static final String PROPERTY_PREFIX = "ddrblob";
	public static final String PROPERTY_DIRECTORY_ROOT_PREFIX = "ddrblob" + ".directory.root";
	public static final String DDRBLOB_HEADERS_PROPERTY = PROPERTY_PREFIX + ".headers";
	public static final String DDRBLOB_HEADERS_DIRECTORIES_PROPERTY = DDRBLOB_HEADERS_PROPERTY + ".directories";
	public static final String DDRBLOB_NAME_PROPERTY = PROPERTY_PREFIX + ".name";
	public static final String DDRBLOB_INHERITANCE_PROPERTY = PROPERTY_PREFIX + ".inheritance.";
	public static final String TYPE_OVERRIDE_PROPERTY = PROPERTY_PREFIX + ".typeoverride.";
	public static final String CONSTANTBEHAVIOUR_PROPERTY_SUFFIX = "constantbehaviour";
	public static final String CONDITIONAL_INCLUSION_PROPERTY_SUFFIX = "oncondition";
	public static final String DDRBLOB_PSEUDOTYPES_PROPERTY = PROPERTY_PREFIX + ".pseudotypes";
	
	private final Collection<BlobHeader> blobHeaders;
	private final String name;
	private final Map<String, Map<String, String>> typeOverrides;
	private final Map<String, String> inheritanceRelationships;
	private final Map<String, UserDefinedType> pseudoTypesByName;
	
	private Configuration(String name, Collection<BlobHeader> blobHeaders, Map<String, Map<String, String>> typeOverrides, Map<String, String> inheritanceRelationships, Map<String, UserDefinedType> pseudoTypes)
	{
		this.blobHeaders = blobHeaders;
		this.name = name;
		this.typeOverrides = typeOverrides;
		this.inheritanceRelationships = inheritanceRelationships;
		this.pseudoTypesByName = pseudoTypes;
	}
	
	public Collection<BlobHeader> getBlobHeaders()
	{
		return Collections.unmodifiableCollection(blobHeaders);
	}
	
	public String getName()
	{
		return name;
	}
	
	/**
	 * 
	 * @return Type overrides. Structure => ( FieldName => Type Override)
	 */
	public Map<String, Map<String, String>> getTypeOverrides()
	{
		return typeOverrides;
	}
	
	public Map<String, ? extends UserDefinedType> getPseudoTypes()
	{
		return Collections.unmodifiableMap(pseudoTypesByName);
	}
	
	/**
	 * 
	 * @return Map of subclass name => superclass name
	 */
	public Map<String, String> getInheritanceRelationships()
	{
		return inheritanceRelationships;
	}
	
	public static Configuration loadConfiguration(File file, File j9Flags) throws IOException
	{
		Properties props = new Properties();
		
		props.load(new FileInputStream(file));
		
		return loadConfiguration(props, j9Flags);
	}
	
	public static Configuration loadConfiguration(Properties props, File j9Flags)
	{
		String name = props.getProperty(DDRBLOB_NAME_PROPERTY);
		
		if (null == name) {
			throw new IllegalArgumentException("No value set for " + DDRBLOB_NAME_PROPERTY);
		}
		
		String headersList = props.getProperty(DDRBLOB_HEADERS_PROPERTY);

		if (null == headersList) {
			throw new IllegalArgumentException("No value set for " + DDRBLOB_HEADERS_PROPERTY);
		}
		
		String[] headers = headersList.split(",");
		
		//Initially fill up blobHeaders with headers from directories.  Subsequent explicit header adds 
		//will then override values in blobHeaders if there are duplicates.  This ensures explicit header
		//adds have priority over blobHeader adds coming from directories. 
		Map<String, BlobHeader> blobHeaders = getBlobHeadersFromDirectories(props, j9Flags);
		
		for (String header : headers) {
			header = header.trim();
			
			if (header.length() == 0) {
				continue;
			}
			
			String constantBehaviourProperty = PROPERTY_PREFIX + "." + header + "." + CONSTANTBEHAVIOUR_PROPERTY_SUFFIX;
			String constantHandlingString = props.getProperty(constantBehaviourProperty);
			ConstantHandlingStrategy headerConstantHandling = ConstantHandlingStrategy.getConstantHandling(constantHandlingString, j9Flags);
			headerConstantHandling.configure(props, constantBehaviourProperty);
			
			String conditionalProperty = PROPERTY_PREFIX + "." + header + "." + CONDITIONAL_INCLUSION_PROPERTY_SUFFIX;
			String conditionalString = props.getProperty(conditionalProperty);
			
			//TODO: lpnguyen add capability to specify paths in header names
			blobHeaders.put(header, new BlobHeader(header, headerConstantHandling, conditionalString));
		}
		
		Map<String, Map<String, String>> typeOverrides = new HashMap<String, Map<String,String>>();
		Map<String, String> inheritanceRelationships = new HashMap<String, String>();
		
		for (Object key : props.keySet()) {
			String property = (String)key;
			
			if (property.startsWith(TYPE_OVERRIDE_PROPERTY)) {
				String specifier = property.substring(TYPE_OVERRIDE_PROPERTY.length());
				
				int index = specifier.indexOf(".");
				
				if (index != -1) {
					String structure = specifier.substring(0,index);
					String field = specifier.substring(index + 1);
					String override = props.getProperty(property);
					
					//System.out.println("Registered type override: " + structure + "." + field + "=" + override);
					
					Map<String,String> structureOverrides = typeOverrides.get(structure);
					
					if (structureOverrides == null) {
						structureOverrides = new HashMap<String, String>();
						typeOverrides.put(structure, structureOverrides);
					}
					
					structureOverrides.put(field, override);
				} else {
					System.err.println("Cannot parse type override: " + property);
				}
			} else if (property.startsWith(DDRBLOB_INHERITANCE_PROPERTY)) {
				String subclass = property.substring(DDRBLOB_INHERITANCE_PROPERTY.length());
				
				String superclass = props.getProperty(property);
				inheritanceRelationships.put(subclass, superclass);
			}
		}
		
		String pseudoTypesList = props.getProperty(DDRBLOB_PSEUDOTYPES_PROPERTY);
		
		Map<String, UserDefinedType> pseudoTypes = new HashMap<String, UserDefinedType>();
		
		if (pseudoTypesList != null) {
			String[] pseudoTypeNames = pseudoTypesList.split(",");
			
			for (String pseudoTypeName : pseudoTypeNames) {
				pseudoTypeName = pseudoTypeName.trim();
				pseudoTypes.put(pseudoTypeName, new PseudoType(pseudoTypeName));
			}
		}
		
		return new Configuration(name, blobHeaders.values(), typeOverrides, inheritanceRelationships, pseudoTypes);
	}

	
	private static Map<String, BlobHeader> getBlobHeadersFromDirectories(Properties props, File j9Flags)
	{
		class headerFilter implements FilenameFilter
		{
			public boolean accept(File file, String name)
			{
				return (name.endsWith(".hpp") || name.endsWith(".h")); 
			}
		}
		
		
		String directoryRoot = props.getProperty(PROPERTY_DIRECTORY_ROOT_PREFIX);
		String dirs = props.getProperty(DDRBLOB_HEADERS_DIRECTORIES_PROPERTY);
		HashMap<String, BlobHeader> blobHeaders = new HashMap<String, BlobHeader>();
		
		//lpnguyen TODO: Implement directory recursion?
		if (null != dirs) {
			String[] dirList = dirs.split(",");
			for (String dir: dirList) {
				String constantBehaviourProperty = PROPERTY_PREFIX + "." + dir + "." + CONSTANTBEHAVIOUR_PROPERTY_SUFFIX;
				String constantHandlingString = props.getProperty(constantBehaviourProperty);
				ConstantHandlingStrategy headerConstantHandling = ConstantHandlingStrategy.getConstantHandling(constantHandlingString, j9Flags);
				headerConstantHandling.configure(props, constantBehaviourProperty);
				
				String conditionalProperty = PROPERTY_PREFIX + "." + dir + "." + CONDITIONAL_INCLUSION_PROPERTY_SUFFIX;
				String conditionalString = props.getProperty(conditionalProperty);
				
				//lpnguyen TODO: Add capability to convert "/" to file.separator
				File file = new File(directoryRoot + "/" + dir);
				for(String header: file.list(new headerFilter())) {
					blobHeaders.put(header, new BlobHeader(header, headerConstantHandling, conditionalString));
				}
			}
		}
		return blobHeaders;
	}

	public void writeCIncludes(PrintWriter writer)
	{
		for (BlobHeader header : getBlobHeaders()) {
			header.writeInclude(writer);
		}
	}
}
