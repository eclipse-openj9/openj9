/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.SecureClassLoader;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;

import com.ibm.j9ddr.StructureReader.PackageNameType;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.corereaders.memory.IProcess;

/**
 * This ClassLoader serves three purposes.
 * 
 * 1) Based on partitioning rules and package namespace it ensures that certain classes are loaded once per runtime invocation while
 *    while others are loaded once per CORE file being inspected.
 * 
 * 2) Generate byte codes and runtime for the J9 Structure constants and offsets based on the data in the core file (or strucgture 
 *    meta data file).  These classes are explicitly loaded on a per CORE file basis.
 *    
 * The isolation is accomplished by removing the Application Class Loader from the class load delegation chain AND setting this
 * class loader's classpath to be equal to the Application class loader's classpath.
 * 
 * While the user may replace the application class loader with their own implementation, the applicaton class loader MUST
 * MUST be a subclass of URLClassLoader.
 */

public class J9DDRClassLoader extends SecureClassLoader {

	// Byte code cache
	private HashMap<String, Class<?>> cache = new HashMap<String, Class<?>>();	
	
	//the reader which contains the raw structure data
	private StructureReader reader;
	private final String packageDotName;
	private final String packageBaseDotName;
	
	// We need the vmData to live as long as this classloader.
	// That makes sure that any objects created from this keep
	// the vmData alive and in the cache.
	private IVMData vmData = null;


	public J9DDRClassLoader(StructureReader reader, ClassLoader parent) {
		super(parent);
		this.reader = reader;
		packageDotName = reader.getPackageName(PackageNameType.STRUCTURE_PACKAGE_DOT_NAME);
		String baseName = reader.getPackageName(PackageNameType.PACKAGE_DOT_BASE_NAME);
		if (baseName.endsWith(".")) {
			packageBaseDotName = baseName;
		} else {
			packageBaseDotName = baseName + ".";
		}
	}

	public StructureHeader getHeader() {
		return reader.getHeader();
	}

	protected Class<?> findClass(String binaryName) throws ClassNotFoundException {
		if(cache.containsKey(binaryName)) {
			return cache.get(binaryName);
		}
		
		if(binaryName.startsWith(packageDotName)) {
			// This is a runtime generated class.  Generate and return the bytes.
			byte[] data = getClazz(binaryName);						//try and read requested class from structure file
			Class<?> clazz = defineClass(binaryName, data, 0, data.length);
			cache.put(binaryName, clazz);							//cache the defined class for future use
			return clazz;
		} else {
			//This is a regular class that we need to duplicate for this classloader
			return duplicateClass(binaryName);
		}
	}
	
	/* Creates a new instance of a class, accessible from the parent classloader */
	private Class<?> duplicateClass(String binaryName) throws ClassNotFoundException
	{
		String resourceName = binaryName.replace('.', '/');
		resourceName = resourceName + ".class";
		
		InputStream stream = J9DDRClassLoader.class.getResourceAsStream('/' + resourceName);
		
		if (stream == null) {
			throw new ClassNotFoundException("Couldn't duplicate class " + binaryName + ". Couldn't load resource " + resourceName + ", parent classloader " + getParent());
		}
		
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		
		byte[] localBuffer = new byte[1024];
		
		int read;
		
		try {
			while( (read = stream.read(localBuffer)) != -1) {
				baos.write(localBuffer,0,read);
			}
		} catch (IOException ex) {
			throw new ClassNotFoundException("IOException reading resource " + resourceName + " for class " + binaryName,ex);
		} finally {
			try {
				stream.close();
			} catch (IOException e) {
				// Not much we can do about this here.
			}
		}
		
		byte[] classBuffer = baos.toByteArray();
		
		return defineClass(binaryName, classBuffer, 0, classBuffer.length);
	}
	
	private void definePackage(String name)
	{
		//Split off the class name
		int finalSeparator = name.lastIndexOf(".");
		
		if (finalSeparator != -1) {
			String packageName = name.substring(0, finalSeparator);
			
			if (getPackage(packageName) != null) {
				return;
			}
			
			//TODO think about the correct values here
			definePackage(packageName, "J9DDR", "0.1", "IBM", "J9DDR", "0.1", "IBM", null);
		}
	}

	public Class<?> loadClassRelativeToStream(String name, boolean resolve) throws ClassNotFoundException {
		return loadClass(packageBaseDotName + name, resolve);
	}
	
	// Cause per core file classes to be loaded once per core file, and shared classes to be loaded once per runtime.
	public Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
		if (name.startsWith(packageBaseDotName)) {
			Class<?> clazz = findLoadedClass(name);
			
			if (null == clazz) {
				//We don't delegate here
				clazz = findClass(name);
				
				definePackage(name);
				
				if (resolve) {
					resolveClass(clazz);
				}
			}
			
			return clazz;
		} else if (name.startsWith(reader.getBasePackage())) { 
			//If we're loading any other DDR versioned package, there's been a mistake (we're trying to load 2.6 classes for a 2.3 core dump for example)
			throw new ClassNotFoundException("Cannot load " + name + ". J9DDRClassLoader is configured to load " + packageBaseDotName + " DDR classes only.");
		}else {
			//Anything else can be loaded normally by the parent (which will take care of delegation)
			return getParent().loadClass(name);
		}
	}


	/**
	 * Read the requested class from the structure file and return as a valid Java class
	 * @param binaryName name of the J9 structure to retrieve
	 * @return the bytes which represent a valid Java class
	 * @throws ClassNotFoundException thrown if the J9 structure has not been defined in the file
	 */
	private byte[] getClazz(String binaryName) throws ClassNotFoundException {
		//make sure that we have a reader available
		if(reader == null) {		
			throw new IllegalStateException("The structure reader has not been initialized");
		}
		
		try {
			return reader.getClassBytes(binaryName);
		} catch (IllegalArgumentException e) {
			// Thrown in many places in StructureReader if things go poorly.
			throw new ClassNotFoundException(binaryName);
		}
	}
	
	public Collection<StructureDescriptor> getStructures()
	{
		return Collections.unmodifiableCollection(reader.getStructures());
	}

	public IVMData getIVMData(IProcess process, long address) throws ClassNotFoundException, IllegalAccessException, InstantiationException {
		if( vmData != null ) {
			return vmData;
		}
		// Associate this IVMData instance with this class loader by storing
		// it in an instance field so that it lives as long as the classloader
		// and any classes it generates. (In other words as long as we are
		// still using this dump.)
		Class<?> vmDataClazz = loadClassRelativeToStream("j9.VMData", false);
		vmData = (IVMData) vmDataClazz.newInstance();
		return vmData;
	}
	
	@Override
	public String toString() {
		StringBuilder builder = new StringBuilder();
		builder.append("J9DDRClassloader for ");
		builder.append(packageBaseDotName);
		return builder.toString();
	}
	
	/**
	 * Get the structure reader used by this classloader.
	 * 
	 * @return
	 */
	public StructureReader getReader() {
		return reader;
	}
}
