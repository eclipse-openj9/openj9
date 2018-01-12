/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.builder;

import java.util.Map;
import java.util.Properties;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.dtfj.image.ImageThread;

/**
 * Factory for building a com.ibm.dtfj.image.ImageProcess
 * <br>
 * Each Image Process factory must have at least one 
 * Java runtime factory that builds {@link com.ibm.dtfj.java.JavaRuntime}
 * <br>
 * Basic support for multiple java runtime factories is present.
 */
public interface IImageProcessBuilder {
	/**
	 * At least one java runtime factory must be associated with an image process factory.
	 * In multiple runtime environments, the last java runtime generated for this image process
	 * may be considered the current java runtime factory.
	 * @return current java runtime factory. Must never be null.
	 */
	public IJavaRuntimeBuilder getCurrentJavaRuntimeBuilder();
	
	/**
	 * 
	 * @param builderID unique id that looks up a java runtime factory
	 * @return found java runtime factory, or null.
	 */
	public IJavaRuntimeBuilder getJavaRuntimeBuilder(String builderID);
	
	/**
	 * Will add an com.ibm.dtfj.image.ImageModule for the specified library name.
	 * If the image module already exists, it will return the latter. A null library name
	 * returns a null ImageModule.
	 * @param name shared library to be added to the com.ibm.dtfj.image.ImageProcess being
	 * built by this image process factory.
	 * @return added/found ImageModule, or null if not added (if the library name isnull)
	 */
	public ImageModule addLibrary(String name);
	
	/**
	 * Adds a com.ibm.dtfj.image.ImageThread to the Image process being built.
	 * If the arguments are invalid and a valid ImageThread cannot be constructed,
	 * error occurs.
	 * <br><br>
	 * If the thread already exists, it will populate any missing data into the image thread, 
	 * and return the latter.
	 * <br><br>
	 * If the thread does not exist, it will create a new ImageThread and register it
	 * with the image process being built.
	 * @param nativeThreadID
	 * @param systemThreadID
	 * @param properties
	 * @return generated ImageThread. Must not be null. If a valid image thread cannot be created or found, throw exception.
	 * @throws BuilderFailureException if valid image thread was not created or found/updated.
	 */
	public ImageThread addImageThread(long nativeThreadID, long systemThreadID, Properties properties) throws BuilderFailureException;
	
	/**
	 * Adds a stack section to an image thread
	 * @param thread
	 * @param section the area in memory used for the stack
	 * @return
	 */
	public ImageSection addImageStackSection(ImageThread thread, ImageSection section);
	/**
	 * Adds a stack frame to an image thread
	 * @param nativeThreadID
	 * @param name of routine
	 * @param the address of the frame
	 * @param the address of the code
	 * @return
	 */
	public ImageStackFrame addImageStackFrame(long nativeThreadID, String name, long baseAddress, long procAddress);
	
	/**
	 * Generates a new java runtime factory. If generation fails, an exception is thrown.
	 * If the java runtime factory already exists, it returns the latter.
	 * @param id unique id to identify a new java runtime factory to be generated
	 * @return generated java runtime factory.
	 * @throws BuilderFailureException if java runtime factory is not created.
	 */
	public IJavaRuntimeBuilder generateJavaRuntimeBuilder(String id) throws BuilderFailureException;
	
	/**
	 * Valid values: 64, 32, or 31 (s390) bits.
	 * @param pointer size for this javacore. Usually parsed or computed from the data found in the javacore. 
	 */
	public void setPointerSize(int size);
	
	/**
	 * Set signal value if available in javacore.
	 * @param generic signal value. 
	 */
	public void setSignal(int signal);
	
	/**
	 * Set command line if available in javacore.
	 * @param command line string 
	 */
	public void setCommandLine(String cmdLine);
	
	/**
	 * Set registers if available in javacore.
	 * @param regs Map of registers 
	 */
	public void setRegisters(Map regs);
	
	
	/**
	 * Add environment variables
	 * @param name
	 * @param value 
	 */
	public void addEnvironmentVariable(String name, String value);
	
	/**
	 * Add a routine to a module
	 * @param library
	 * @param name
	 * @param address
	 * @return
	 */
	public ImageSymbol addRoutine(ImageModule library, String name, long address);

	/**
	 * Sets the module as the process executable
	 * @param execMod
	 */
	public void setExecutable(ImageModule execMod);

	/**
	 * Sets the id of the process
	 * @param pid String
	 */
	public void setID(String pid);

	/**
	 * Sets the current thread
	 * @param imageThreadID
	 */
	public void setCurrentThreadID(long imageThreadID);

	/**
	 * Adds/updates a property for the library
	 * @param library the module
	 * @param name the property name
	 * @param value the property value
	 */
	public void addProperty(ImageModule library, String name, String value);
}
