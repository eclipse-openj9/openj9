/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2018 IBM Corp. and others
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
package com.ibm.dtfj.javacore.builder.javacore;

import java.util.HashMap;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.image.javacore.JCImageAddressSpace;
import com.ibm.dtfj.image.javacore.JCImageProcess;
import com.ibm.dtfj.image.javacore.JCImageThread;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;
import com.ibm.dtfj.java.javacore.JCJavaClass;
import com.ibm.dtfj.java.javacore.JCJavaClassLoader;
import com.ibm.dtfj.java.javacore.JCJavaLocation;
import com.ibm.dtfj.java.javacore.JCJavaMethod;
import com.ibm.dtfj.java.javacore.JCJavaMonitor;
import com.ibm.dtfj.java.javacore.JCJavaObject;
import com.ibm.dtfj.java.javacore.JCJavaRuntime;
import com.ibm.dtfj.java.javacore.JCJavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.javacore.JCJavaStackFrame;
import com.ibm.dtfj.java.javacore.JCJavaThread;
import com.ibm.dtfj.java.javacore.JCJavaVMInitArgs;
import com.ibm.dtfj.java.javacore.JCJavaVMOption;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;

public class JavaRuntimeBuilder extends AbstractBuilderComponent implements IJavaRuntimeBuilder {


	private JCImageProcess fImageProcess = null;
	private JCJavaRuntime fJavaRuntime = null;
	private JCImageAddressSpace fAddressSpace = null;
	private JCJavaVMInitArgs fJavaVMInitArgs = null;
	private HashMap j9ThreadToJNIEnv = new HashMap();
	private String fId;

	public JavaRuntimeBuilder(JCImageProcess imageProcess, String id) throws JCInvalidArgumentsException {
		super(id);
		if (imageProcess == null) {
			throw new JCInvalidArgumentsException("Must pass non-null image process");
		}
		fImageProcess = imageProcess;
		fId = id;
		// Build runtime when needed - only build here if it would cause an error later
		if (fId == null) fJavaRuntime = new JCJavaRuntime(fImageProcess, fId);
		fAddressSpace = fImageProcess.getImageAddressSpace();
	}
	
	/**
	 * Must pass a valid class loader ID in order to generate a class loader object. Else exception
	 * is thrown. 
	 * 
	 * @param classLoaderName optional
	 * @param clID required, or else exception is throw and class loader is not created.
	 * @param objectID optional (although generally this is the same as the clID for most javacores)
	 * @return successfully created JavaClassLoader
	 * @throws BuilderFailureException if class loader ID is invalid.
	 */
	public JavaClassLoader addClassLoader(String classLoaderName, long clID, long objectID) throws BuilderFailureException {
		try {
			JCJavaClassLoader classLoader = getJavaRuntime().findJavaClassLoader(clID);
			if (classLoader == null) {
				classLoader = new JCJavaClassLoader(getJavaRuntime(), clID);
			}
			
			JCJavaObject javaObject = classLoader.getInternalObject();
			if (javaObject == null) {
				JCJavaClass javaClass = getJavaRuntime().findJavaClass(classLoaderName);
				if (javaClass == null) {
					javaClass = new JCJavaClass(getJavaRuntime(), classLoaderName);
				}
					
				ImagePointer objectPointer = fAddressSpace.getPointer(objectID);
				javaObject = new JCJavaObject(objectPointer, javaClass);
				classLoader.setObject(javaObject);
			}
			return classLoader;
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}
	
	/**
	 * Note that even if a class was already registered with a class loader previously, it may contain incomplete data,
	 * so this case has to be considered. 
	 */
	public JavaClass addClass(JavaClassLoader jClassLoader, String name, long classID, long superClassID, String fileName) throws BuilderFailureException {
		try {
			JCJavaClassLoader classLoader = (JCJavaClassLoader) jClassLoader;
			JCJavaClass jClass = generateJavaClass(getJavaRuntime(), name, classID);
			jClass.setJavaSuperClass(superClassID);
			jClass.setClassLoader(classLoader);
			ImagePointer ip = jClass.getID();
			if (ip != null) {
				classLoader.addClass(name, ip);
			} else {
				if (classLoader.internalGetClass(name) == null) {
					classLoader.addClass(name);
				}
			}
			return jClass;
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}
	
	/**
	 * Either retrieves an existing java class registered with the java runtime or it creates one and it fills in missing information, 
	 * if the latter is available.
	 * <br><br>
	 * A valid runtime and class name must be passed in order to successfully generate a java class. An invalid argument exception is
	 * thrown if either one of them is invalid.
	 * <br><br>
	 * A null value should not be returned, as this indicates an error was encountered. Instead, throw the
	 * appropriate exception.
	 * 
	 * @param runtime required
	 * @param name required
	 * @param classID optional
	 * @return added or updated JavaClass, or exception thrown if invalid arguments passed.
	 * @throws JCInvalidArgumentsException
	 */
	private JCJavaClass generateJavaClass(JCJavaRuntime runtime, String name, long classID) throws JCInvalidArgumentsException {
		if (name != null) {
			if (fAddressSpace.isValidAddressID(classID)) {
				JCJavaClass jClass = runtime.findJavaClass(classID);
				if (jClass == null) {
					jClass = runtime.findJavaClass(name);
					if (jClass != null) {
						ImagePointer ip = jClass.getID();
						if (ip == null) {
							jClass.setID(classID);
						} else {
							jClass = new JCJavaClass(runtime, name);
							jClass.setID(classID);
						}
					} else {
						jClass = new JCJavaClass(runtime, name);
						jClass.setID(classID);
					}
				}
				return jClass;
			}			
			
			JCJavaClass jClass = runtime.findJavaClass(name);
			if (jClass == null) {
				jClass = new JCJavaClass(runtime, name);
			}
			if (fAddressSpace.isValidAddressID(classID)) {
				jClass.setID(classID);
			}
			return jClass;
		}
		else {
			throw new JCInvalidArgumentsException("Failed to add class.  Null class name");
		}
	}

	/**
	 * 
	 * @return imageprocess containing the runtime.
	 */
	public ImageProcess getImageProcess() {
		return fImageProcess;
	}

	/**
	 * If successfully added a JavaThread, or updated an existing JavaThread, return the javathread, or otherwise
	 * throw an exception. Note that a javathread cannot be successfully added if no valid threadID is passed.
	 * <br><br>
	 * REQUIREMENT: Thread Id must be valid to properly create a Java Thread, or exception is thrown.
	 * 
	 * @throws BuilderFailureException if arguments lead to an invalid DTFJ object, particularly an invalid threadID
	 * @return a non null JavaThread that was successfully added or updated.
	 */
	public JavaThread addJavaThread(ImageThread imageThread, String name, long tid, long j9thread_t, long javaObjID, long jniEnv, String state, int priority, long blockingObject, String blockingObjectClass) throws BuilderFailureException {
		try {
			if (j9thread_t != IBuilderData.NOT_AVAILABLE && jniEnv != IBuilderData.NOT_AVAILABLE) {
				// Save the JNIEnv for later
				ImagePointer pointer = fAddressSpace.getPointer(jniEnv);
				j9ThreadToJNIEnv.put(Long.valueOf(j9thread_t), pointer);
			}
			if (!fAddressSpace.isValidAddressID(tid)) {
				throw new JCInvalidArgumentsException("Must pass a valid thread id");
			}
			JCJavaThread javaThread = getJavaRuntime().findJavaThread(tid);
			if (javaThread == null) {
				ImagePointer pointer = fAddressSpace.getPointer(tid);
				javaThread = new JCJavaThread(getJavaRuntime(), pointer);
			}
			javaThread.setName(name);
			javaThread.setPriority(priority);
			javaThread.setState(state);
			javaThread.setImageThread((JCImageThread)imageThread);
			if (jniEnv == IBuilderData.NOT_AVAILABLE) {
				// Retrieve the JNIEnv
				ImagePointer pointer = (ImagePointer)j9ThreadToJNIEnv.get(Long.valueOf(j9thread_t));
				if (pointer != null) {
					// Set it for 1.4.2
					javaThread.setJNIEnv(pointer);
				} else {
					// Else TID for J9 seems to be JNIEnv
					javaThread.setJNIEnv(javaThread.getThreadID());
				}
			}
			/*
			 * Add the object only if the object ID is valid.
			 */
			if (fAddressSpace.isValidAddressID(javaObjID)) {
				// Make the type just java.lang.Thread - later we can do better if there is a stack trace.
				JCJavaClass jClass = generateJavaClass(getJavaRuntime(), "java/lang/Thread", IBuilderData.NOT_AVAILABLE);
				ImagePointer pointerObjectID = fAddressSpace.getPointer(javaObjID);
				JCJavaObject jobject = new JCJavaObject(pointerObjectID, jClass);
				javaThread.setObject(jobject);
			}
			if( fAddressSpace.isValidAddressID(blockingObject)) {
				JCJavaClass jClass = generateJavaClass(getJavaRuntime(), blockingObjectClass, IBuilderData.NOT_AVAILABLE);
				ImagePointer pointerObjectID = fAddressSpace.getPointer(javaObjID);
				JCJavaObject jobject = new JCJavaObject(pointerObjectID, jClass);
				javaThread.setBlockingObject(jobject);
			}
			return javaThread;
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}

	/**
	 * 
	 */
	public JavaStackFrame addJavaStackFrame(JavaThread javaThread, String className, String classFileName, String methodName, String methodType, String compilationLevel, int lineNumber) throws BuilderFailureException {
		try {
			JCJavaThread jThread = (JCJavaThread) javaThread;
			JCJavaClass jclass = getJavaRuntime().findJavaClass(className);
			if (jclass == null) {
				jclass = new JCJavaClass(getJavaRuntime(), className);
			}
			JCJavaMethod method = new JCJavaMethod(methodName, jclass);
			JCJavaLocation location = new JCJavaLocation(method);
			location.setFilename(classFileName);
			location.setCompilation(compilationLevel);
			location.setLineNumber(lineNumber);
			JCJavaStackFrame stackFrame = new JCJavaStackFrame(jThread, location);
			if ("run".equals(methodName)) {
				/*
				 * Perhaps the type of the thread is the type of the last run() method.
				 * Not strictly accurate if the class was extended without overriding run(). 
				 * Add the object only if the object ID is valid.
				 */
				try {
					JavaObject jo = jThread.getObject();
					if (jo != null)
					{
						ImagePointer tid = jo.getID();
						if (fAddressSpace.isValidAddressID(tid.getAddress())) {
							JCJavaObject jobject = new JCJavaObject(tid, jclass);
							jThread.setObject(jobject);
						}
					}
				} catch (CorruptDataException e) {
						// Ignore
				}
			}
			return stackFrame;
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}

	/**
	 * Required: monitor ID (throws exception if invalid)
	 * <br>
	 * Optional: object ID, class name, monitor name, owning thread
	 * @return successfully created JavaMonitor
	 * @throws BuilderFailureException if an invalid monitor ID is passed.
	 */
	public JavaMonitor addJavaMonitor(String monitorName, long monitorID, long objectID, String className, long owningThread) throws BuilderFailureException {
		try {
			JCJavaMonitor monitor = getJavaRuntime().findMonitor(monitorID);
			if (monitor == null) {
				ImagePointer pointerMonitorID = fAddressSpace.getPointer(monitorID);
				monitor = new JCJavaMonitor(getJavaRuntime(), pointerMonitorID, monitorName);
			}
			monitor.setOwner(owningThread);
			/*
			 * Adding a class and monitor object are optional, therefore
			 * if the data is not present don't assume it is an error.
			 */
			
			if (className != null) {
				JCJavaClass jClass = generateJavaClass(getJavaRuntime(), className, IBuilderData.NOT_AVAILABLE);
				/*
				 * Add the object only if the class is also present and object ID is valid.
				 */
				if (jClass != null && fAddressSpace.isValidAddressID(objectID)) {
					ImagePointer pointerObjectID = fAddressSpace.getPointer(objectID);
					JCJavaObject jobject = new JCJavaObject(pointerObjectID, jClass);
					monitor.setObject(jobject);
				}
			}
			return monitor;
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}

	/**
	 * 
	 */
	public void addBlockedThread(JavaMonitor monitor, long threadID) throws BuilderFailureException {
		JCJavaMonitor jmonitor = (JCJavaMonitor) monitor;
		ImagePointer pointer = fAddressSpace.getPointer(threadID);
		jmonitor.addEnterWaiter(pointer);
	}

	/**
	 * 
	 */
	public void addWaitOnNotifyThread(JavaMonitor monitor, long threadID) throws BuilderFailureException {
		JCJavaMonitor jmonitor = (JCJavaMonitor) monitor;
		ImagePointer pointer = fAddressSpace.getPointer(threadID);
		jmonitor.addNotifyWaiter(pointer);
	}
	
	/**
	 * Adds an (empty) JavaVMInitArgs
	 */	
	public void addVMInitArgs() throws BuilderFailureException {
		if (fJavaVMInitArgs != null) {
			throw new BuilderFailureException("JCJavaVMInitArgs already created for this JavaRuntime");
		}
		try {
			// Create JavaVMInitArgs object. Note that JNI version and ignoreUnrecognized 
			// flags are not available in javacore - JCJavaVMInitArgs returns DataUnavailable
			// if those fields are requested.
			fJavaVMInitArgs = new JCJavaVMInitArgs(getJavaRuntime(), 0, true);
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}
	
	/**
	 * Adds an individual VM option to JavaVMInitArgs
	 */	
	public void addVMOption(String option) throws BuilderFailureException {
		if (fJavaVMInitArgs == null) {
			throw new BuilderFailureException("JCJavaVMInitArgs must be created before options added");
		}
		try {
			fJavaVMInitArgs.addOption( new JCJavaVMOption(option, null));
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}

	/**
	 * Adds an individual VM option to JavaVMInitArgs, with 'extra information' field 
	 */	
	public void addVMOption(String option, long extraInfo) throws BuilderFailureException {
		if (fJavaVMInitArgs == null) {
			throw new BuilderFailureException("JCJavaVMInitArgs must be created before options added");
		}
		try {
			ImagePointer pointer = fAddressSpace.getPointer(extraInfo);
			fJavaVMInitArgs.addOption( new JCJavaVMOption(option, pointer));
		} catch (JCInvalidArgumentsException e) {
			throw new BuilderFailureException(e);
		}
	}
	
	/**
	 * Sets the Java version string.
	 */
	public void setJavaVersion(String version) {
		getJavaRuntime().setVersion(version);
	}

	/**
	 * Delay creating the Java runtime until we have some data for it.
	 */
	private JCJavaRuntime getJavaRuntime() {
		if (fJavaRuntime == null) {
			try {
				fJavaRuntime = new JCJavaRuntime(fImageProcess, fId);
			} catch (JCInvalidArgumentsException e) {
				IllegalStateException e1 = new IllegalStateException();
				e1.initCause(e);
				throw e1;
			}
		}
		return fJavaRuntime;
	}

	public JavaRuntimeMemoryCategory addMemoryCategory(String name,
			long deepBytes, long deepAllocations,
			JavaRuntimeMemoryCategory parent)
	{
		JCJavaRuntimeMemoryCategory category = new JCJavaRuntimeMemoryCategory(name,deepBytes,deepAllocations);
		
		if (parent != null) {
			( (JCJavaRuntimeMemoryCategory) parent).addChild(category);
		} else {
			getJavaRuntime().addTopLevelMemoryCategory(category);
		}
		
		return category;
	}

	public void setShallowCountersForCategory(
			JavaRuntimeMemoryCategory category, long shallowBytes,
			long shallowAllocations)
	{
		((JCJavaRuntimeMemoryCategory) category).setShallowCounters(shallowBytes, shallowAllocations);
	}

	public void setJITEnabled(boolean enabled) {
		getJavaRuntime().setJITEnabled(enabled);
	}

	public void addJITProperty(String name, String value) {
		getJavaRuntime().addJITProperty(name, value);
	}
	
	public void setStartTime(long startTime) {
		getJavaRuntime().setStartTime(startTime);
	}
	
	public void setStartTimeNanos(long startTimeNanos) {
		getJavaRuntime().setStartTimeNanos(startTimeNanos);
	}
}
