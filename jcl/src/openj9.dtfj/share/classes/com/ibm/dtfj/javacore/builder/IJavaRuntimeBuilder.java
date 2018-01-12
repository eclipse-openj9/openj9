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

import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;

/**
 * Factory for building a com.ibm.dtfj.java.JavaRuntime object.
 * Only one java runtime factory must be associated with a java runtime object.
 *
 */
public interface IJavaRuntimeBuilder {
	
	/**
	 * Adds a com.ibm.dtfj.java.JavaClass into DTFJ. If class already exists, it updates any missing information.
	 * The updated/added class is returned. If error occurs during class generation/update, throw exception
	 * @param jClassLoader that loads the class
	 * @param name of class
	 * @param classID valid address
	 * @param superClassID valid address
	 * @return generated/modified java class.
	 * 
	 */
	public JavaClass addClass(JavaClassLoader jClassLoader, String name, long classID, long superClassID, String fileName) throws BuilderFailureException;
	
	
	/**
	 * Adds a com.ibm.dtfj.java.JavaClassLoader into DTFJ. If the class already exists, it updates missing information.
	 * Returns added/updated classloader. IF error occurs, throw exception.
	 * @param name of classloader
	 * @param clID address classloader
	 * @param objectID address of class loader object instance 
	 * @return added/modified classLoader
	 * @throws BuilderFailureException if failed to add/update classloader
	 */
	public JavaClassLoader addClassLoader(String name, long clID, long objectID) throws BuilderFailureException;
	
	/**
	 * Not available in a javacore: object ID and the java.lang.Thread class name associated with a thread.
	 * 
	 * Adds a javathread, or modifies an existing one with information being passed into the method. Returns the updated/modified javathread.
	 * 
	 * @param imageThread associated with javathread
	 * @param name for now, just the string name parsed directly from the javacore.
	 * @param tid = JNIENV or J9JVMThread, i.e., it equals the internal VM data structure for a java thread
	 * @param j9thread_t = a thread model at a lower level than a tid, which models a native thread: not represented in DTFJ, but pass it anyway as a possible ImageThread property
	 * @param javaObjID The thread object seen from Java
	 * @param jniEnv The JNIENV
	 * @param state
	 * @param priority
	 * @param blockingObjectClassName 
	 * @param blockingObjectAddress 
	 * @return added/modified JavaThread.
	 * @throws BuilderFailureException if failed to add/update java thread
	 */
	public JavaThread addJavaThread(ImageThread imageThread, String name, long tid, long j9thread_t, long javaObjID, long jniEnv, String state, int priority, long blockingObjectAddress, String blockingObjectClassName) throws BuilderFailureException;
	
	/**
	 * Adds a java stack frame to a javathread. It does not check if a stack frame has already been added or not, so it is possible to added
	 * the same stack frame multiple times. Returns the java stack frame added. Throws exception if it failed to generate and add the stack frame to the 
	 * java thread specified in the argument.
	 * @param javaThread where java stack trace is to be added
	 * @param className
	 * @param classFileName
	 * @param methodName
	 * @param methodType whether native or interpreted method.
	 * @param compilationLevel
	 * @param lineNumber as it appears in the javacore
	 * 
	 */
	public JavaStackFrame addJavaStackFrame(JavaThread javaThread, String className, String classFileName, String methodName, String methodType, String compilationLevel, int lineNumber) throws BuilderFailureException;
	
	
	/**
	 * Adds a java monitor or modifies an existing one. Returns the updated/modified monitor. Throws exception if error occurs during monitor
	 * generation or update.
	 * @param name of monitor
	 * @param monitorID
	 * @param objectID
	 * @param className
	 * @param owningThread
	 * @return added/modified
	 */
	public JavaMonitor addJavaMonitor(String name, long monitorID, long objectID, String className, long owningThread) throws BuilderFailureException;
	
	
	/**
	 * 
	 * @param monitor
	 * @param thread
	 */
	public void addWaitOnNotifyThread(JavaMonitor monitor, long threadID) throws BuilderFailureException;
	
	/**
	 * 
	 * @param monitor
	 * @param thread
	 */
	public void addBlockedThread(JavaMonitor monitor, long threadID) throws BuilderFailureException;

	/**
	 * Adds a (empty) JavaVMInitArgs
	 * @param option
	 * @return void
	 */
	public void addVMInitArgs() throws BuilderFailureException;

	/**
	 * Adds an individual Java VM option to JavaVMInitArgs
	 * @param option
	 * @return void
	 */
	public void addVMOption(String option) throws BuilderFailureException;

	/**
	 * Adds an individual Java VM option to JavaVMInitArgs, with 'extra information' field
	 * @param option 
	 * @param extraInfo
	 * @return void
	 */
	public void addVMOption(String option, long extraInfo) throws BuilderFailureException;

	/**
	 * Sets the Java version
	 * @param version
	 */
	public void setJavaVersion(String version);

	/**
	 * Adds a runtime memory category to the JavaRuntime
	 * @param name
	 * @param deepBytes
	 * @param deepAllocations
	 * @param parent The parent category, or NULL if this category is a root
	 * @return
	 */
	public JavaRuntimeMemoryCategory addMemoryCategory(String name, long deepBytes, long deepAllocations, JavaRuntimeMemoryCategory parent);
	
	/**
	 * Sets the shallow memory categories for an existing memory category
	 */
	public void setShallowCountersForCategory(JavaRuntimeMemoryCategory category, long shallowBytes, long shallowAllocations);
	
	/**
	 * Sets if the JIT was enabled for this runtime
	 * @param enabled true if it was enabled
	 */
	public void setJITEnabled(boolean enabled);
	
	/**
	 * Add a property with which the JIT was running for this runtime
	 * @param name property name
	 * @param value	value
	 */
	public void addJITProperty(String name, String value);
	
	/**
	 * Set the time the JVM started
	 * @param startTime the time
	 */
	public void setStartTime(long startTime);
	
	/**
	 * Set the nanotime the JVM was started
	 * @param nanoTime the time
	 */
	public void setStartTimeNanos(long nanoTime);
}
