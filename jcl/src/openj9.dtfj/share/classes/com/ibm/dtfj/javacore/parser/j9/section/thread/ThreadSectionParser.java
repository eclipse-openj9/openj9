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
package com.ibm.dtfj.javacore.parser.j9.section.thread;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

/**
 * 
 *
 */
public class ThreadSectionParser extends SectionParser implements IThreadTypes{
	

	private IJavaRuntimeBuilder fRuntimeBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	
	
	public ThreadSectionParser(){
		super(THREAD_SECTION);
	}
	
	/**
	 * 
	 * @param lookAheadBuffer
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
		fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		currentThreadInfoSubsection();
		allThreadInfoSubsection();
	}
	
	/**
	 * current_thread_info := CURRENT_THREAD_INFO_SUBSECTION thread_info_section | CURRENT_THREAD_INFO_SUBSECTION
	 * <br><br>
	 * This implementation allows for the parsing process to continue even after errors are caught. See the error handler
	 * for more information.
	 * 
	 * @see #handleError(Exception)
	 * 
	 * @param scanner
	 * @throws ParserException
	 */
	protected void currentThreadInfoSubsection() throws ParserException {
		/*
		 * Process and ignore T_1XMCURTHDINFO. No meaningful DTFJ data.
		 */
		if (processTagLineOptional(T_1XMCURTHDINFO) != null) {
			// Parse the thread info and stack trace
			// We have to generate build model here as 
			// Java 7.0 doesn't have it in the allthreadinfo section.
			threadInfo(true, true);
		}
	}
	
	/**
	 * 
	 * 
	 * @param scanner
	 */
	protected void allThreadInfoSubsection() throws ParserException {
		processTagLineRequired(T_1XMTHDINFO);
		IAttributeValueMap results = processTagLineOptional(T_2XMFULLTHDDUMP);
		if (results != null) {
			int pointerSize = results.getIntValue(ICommonTypes.POINTER_SIZE);
			fImageProcessBuilder.setPointerSize(pointerSize);
		}
		threadInfo(true, false);
	}
	
	/**
	 * @param buildModel true if data parsed should be added into the builder.
	 * @param currentThread This is the current thread
	 * @throws ParserException
	 */
	protected void threadInfo(boolean buildModel, boolean currentThread) throws ParserException {
		IAttributeValueMap results = null;
		int currentLineNumber = getCurrentFileLineNumber();
		if ((results = processTagLineRequired(T_3XMTHREADINFO)) != null) {
			// Always parse the first thread of the section so that the first thread in 
			// the current thread section will be generated first. It will be reparsed later,
			// but that doesn't matter.
			processThreadandStackTrace(results, buildModel, currentThread, currentLineNumber);
			if (!currentThread) {
				while((results = processTagLineOptional(T_3XMTHREADINFO)) != null) {
					processThreadandStackTrace(results, buildModel, false, currentLineNumber);
				}
			}
		}
	}
	
	/**
	 * 
	 * @param javaThreadResults
	 * @param buildModel
	 * @param currentThread Is this the current thread?
	 * @param currentLineNumber
	 * @return updated line number
	 * @throws ParserException
	 */
	protected int processThreadandStackTrace(IAttributeValueMap javaThreadResults, boolean buildModel, boolean currentThread, int currentLineNumber) throws ParserException {
		JavaThread javaThread = null;
		// 3XMTHREADINFO1 found only in J9 2.4 or higher. Native thread ID is contained
		// in the latter. For all other older VMs, native thread ID is contained in 3XMTHREADINFO
		IAttributeValueMap nativeResults = processTagLineOptional(T_3XMTHREADINFO1);
		
		IAttributeValueMap nativeStack;
		ArrayList nativeStacks = new ArrayList();
		while ((nativeStack = processTagLineOptional(T_3XMTHREADINFO2)) != null) {
			nativeStacks.add(nativeStack);
		}

		IAttributeValueMap blockerInfo = processTagLineOptional(T_3XMTHREADBLOCK);
		
		IAttributeValueMap cpuTimes = processTagLineOptional(T_3XMCPUTIME);

		if (buildModel) {
			javaThread = addThread(javaThreadResults, nativeResults, nativeStacks, blockerInfo, cpuTimes, currentLineNumber);
		}
		long imageThreadID = (nativeResults != null) ? nativeResults.getLongValue(NATIVE_THREAD_ID) : javaThreadResults.getLongValue(NATIVE_THREAD_ID);
		long tid = javaThreadResults.getLongValue(VM_THREAD_ID);
		if (imageThreadID == IBuilderData.NOT_AVAILABLE) {
			imageThreadID = tid;
		}
		parseStackTrace(javaThread, currentLineNumber, buildModel);
		parseNativeStackTrace(imageThreadID, buildModel);
		if (currentThread) {
			// Indicate we have a possible current thread
			fImageProcessBuilder.setCurrentThreadID(imageThreadID);
		}
		return getCurrentFileLineNumber();
	}
	
	/**
	 * 
	 * @param javaThreadResults
	 * @param nativeStacks list of attributes of the native stacks
	 * @param currentFileLineNumber
	 * @return java thread 
	 * @throws ParserException if error handler decides to throw it due java thread or image thread error creation
	 */
	private JavaThread addThread(IAttributeValueMap javaThreadResults, IAttributeValueMap nativeResults, List nativeStacks, IAttributeValueMap blockerInfo, IAttributeValueMap cpuTimes, int currentFileLineNumber) throws ParserException {
		long imageThreadID = (nativeResults != null) ? nativeResults.getLongValue(NATIVE_THREAD_ID) : javaThreadResults.getLongValue(NATIVE_THREAD_ID);
		long tid = javaThreadResults.getLongValue(VM_THREAD_ID);
		if (imageThreadID == IBuilderData.NOT_AVAILABLE) {
			imageThreadID = tid;
		}

		ImageThread imageThread = null;
		JavaThread javaThread = null;
		
		//CMVC 158108 : fixed so that if the thread name is not present then it is not subsequently parsed 
		String threadName = javaThreadResults.getTokenValue(JAVA_THREAD_NAME);
		
		if (threadName != null && threadName.length() >= 2) {
			threadName = threadName.substring(1, threadName.length() - 1); // remove quotes
		}
		String threadState = javaThreadResults.getTokenValue(JAVA_STATE);
		int threadPriority = javaThreadResults.getIntValue(VM_THREAD_PRIORITY);
		
		long abstractThreadID = javaThreadResults.getLongValue(ABSTRACT_THREAD_ID);
		Properties properties = new Properties();
		if (abstractThreadID != IBuilderData.NOT_AVAILABLE) {
			addAsProperty(properties, ABSTRACT_THREAD_ID, "0x"+Long.toHexString(abstractThreadID));
		}
		if (nativeResults != null) {
			addAsProperty(properties, NATIVE_THREAD_PRIORITY, nativeResults.getTokenValue(NATIVE_THREAD_PRIORITY));
			addAsProperty(properties, NATIVE_THREAD_POLICY, nativeResults.getTokenValue(NATIVE_THREAD_POLICY));
			addAsProperty(properties, SCOPE, nativeResults.getTokenValue(SCOPE));
			addAsProperty(properties, VM_STATE, nativeResults.getTokenValue(VM_STATE));
			addAsProperty(properties, VM_FLAGS, nativeResults.getTokenValue(VM_FLAGS));
		}
		
		if (cpuTimes != null ) {
			addAsProperty(properties, CPU_TIME_TOTAL, cpuTimes.getTokenValue(CPU_TIME_TOTAL));
			addAsProperty(properties, CPU_TIME_USER, cpuTimes.getTokenValue(CPU_TIME_USER));
			addAsProperty(properties, CPU_TIME_SYSTEM, cpuTimes.getTokenValue(CPU_TIME_SYSTEM));
		}
		
		String blockerObjectClassName = null;
		//CMVC 180977 : the parser expects missing data to be represented by a -ve number, so change the default address from 0 to IBuilderData.NOT_AVAILABLE (currently -1)
		//this parser check will break on a sufficiently large 64bit number, but fixing that is beyond the scope of this defect.
		long blockerObjectAddress = IBuilderData.NOT_AVAILABLE;		 
		if( blockerInfo != null ) {
			blockerObjectClassName =  blockerInfo.getTokenValue(BLOCKER_OBJECT_FULL_JAVA_NAME);
			blockerObjectAddress = blockerInfo.getLongValue(BLOCKER_OBJECT_ADDRESS);
		}
		
		long javaObjID = javaThreadResults.getLongValue(JAVA_THREAD_OBJ);
		if (javaObjID == IBuilderData.NOT_AVAILABLE && nativeResults == null) {
			String vmthread = javaThreadResults.getTokenValue(VM_THREAD_ID);
			// Sov 1.4.2 vmthread id is the Java object
			// Java 5.0 vmthreads tend to start with 0x0, and are not Java objects
			if (vmthread != null && !vmthread.startsWith("0x0"))
				javaObjID = tid;
		}
		
		try {
			imageThread = fImageProcessBuilder.addImageThread(imageThreadID, abstractThreadID, properties);
			if (threadName != null || tid != IBuilderData.NOT_AVAILABLE) {
				javaThread = fRuntimeBuilder.addJavaThread(imageThread, threadName, tid, abstractThreadID, javaObjID, IBuilderData.NOT_AVAILABLE, threadState, threadPriority, blockerObjectAddress, blockerObjectClassName);
			}
		} catch (BuilderFailureException e) {
			handleErrorAtLineNumber(currentFileLineNumber, "Failed to add thread: " + threadName + " " + imageThreadID, e);
		}
		
		for (Iterator it = nativeStacks.iterator(); it.hasNext(); ) {
			IAttributeValueMap stackInfo = (IAttributeValueMap)it.next();
			long from = stackInfo.getLongValue(NATIVE_STACK_FROM);
			long to = stackInfo.getLongValue(NATIVE_STACK_TO);
			long size = stackInfo.getLongValue(NATIVE_STACK_SIZE);
			if (from != IBuilderData.NOT_AVAILABLE && size != IBuilderData.NOT_AVAILABLE) {
				ImageSection section = fImageAddressSpaceBuilder.addImageSection("Native stack section", from, size);
				fImageProcessBuilder.addImageStackSection(imageThread, section);
			}
		}
		
		return javaThread;
	}
	
	/**
	 * 
	 * @param results from parsing the threadinfo tag
	 * @throws ParserException
	 */
	private void parseStackTrace(JavaThread javaThread, int currentFileLineNumber, boolean buildModel) throws ParserException{

		/*
		 * Consume data from buffer even if there is no valid java thread in which
		 * to add the stack frames.
		 */
		IAttributeValueMap stackTraceResults = null;
		currentFileLineNumber = getCurrentFileLineNumber();
		processTagLineOptional(T_3XMTHREADINFO3);
		while ((stackTraceResults = processTagLineOptional(T_4XESTACKTRACE)) != null) {
			if (javaThread != null) {
				addStackTrace(stackTraceResults, javaThread, currentFileLineNumber);
				currentFileLineNumber = getCurrentFileLineNumber();
			}
		}
		/*
		 * Sov hook (i.e., for Sov 1.4.2 javacores, there is additional information after
		 * the last 4XESTACKTRACE tag of a 3XMTHREADINFO tag.
		 */
		sovOnlyRules(T_3XMTHREADINFO);
	}
	
	/**
	 * Will generate a stack trace object based on the parsed attributes passed into it. If a valid javathread
	 * does not exist, any builder exceptions will be handled by the error handler.
	 * 
	 * @param stackTraceResults
	 * @param javaThread
	 */
	private void addStackTrace(IAttributeValueMap stackTraceResults, JavaThread javaThread, int fileLineNumber) throws ParserException{
		if (stackTraceResults == null) {
			return;
		}
		String className = stackTraceResults.getTokenValue(FULL_NAME);
		String methodName = stackTraceResults.getTokenValue(METHOD_NAME);
		String methodType = stackTraceResults.getTokenValue(STACKTRACE_METHOD_TYPE);
		String classFile = null;
		String compilationLevel = null;
		int lineNumber = IBuilderData.NOT_AVAILABLE;
		if (methodType.equals(STACKTRACE_JAVA_METHOD)) {
			classFile = stackTraceResults.getTokenValue(CLASS_FILE_NAME);
			compilationLevel = stackTraceResults.getTokenValue(COMPILATION_LEVEL);
			lineNumber = stackTraceResults.getIntValue(STACKTRACE_LINE_NUMBER);
		}
		if (methodType.equals(STACKTRACE_NATIVE_METHOD)) {
			// Ugly, but we don't know all the modifiers
			classFile = "Native Method";
		}
		try {
			fRuntimeBuilder.addJavaStackFrame(javaThread, className, classFile, methodName, methodType, compilationLevel, lineNumber);
		} catch (BuilderFailureException e) {
			handleErrorAtLineNumber(fileLineNumber, "Failed to add stack frame: " + className + "." + methodName + " " + lineNumber, e);
		}
	}
	
	/**
	 * Parse the native stack line information
	 * @throws ParserException
	 */
	private void parseNativeStackTrace(long threadID, boolean buildModel) throws ParserException {
		IAttributeValueMap results = null;
		
		// Process the version lines
		processTagLineOptional(T_3XMTHREADINFO3);
		while ((results = processTagLineOptional(T_4XENATIVESTACK)) != null) {
			if (!buildModel) continue;
			String module = results.getTokenValue(STACK_MODULE);
			String routine = results.getTokenValue(STACK_ROUTINE);
			long address = results.getLongValue(STACK_PROC_ADDRESS);
			long routine_address = results.getLongValue(STACK_ROUTINE_ADDRESS);
			long routine_offset = results.getLongValue(STACK_ROUTINE_OFFSET);
			long module_offset = results.getLongValue(STACK_MODULE_OFFSET);
			String file = results.getTokenValue(STACK_FILE);
			int line = results.getIntValue(STACK_LINE);

			// Allow for missing data
			if (routine_address == IBuilderData.NOT_AVAILABLE
					&& address != IBuilderData.NOT_AVAILABLE
					&& routine_offset != IBuilderData.NOT_AVAILABLE) {
				routine_address = address - routine_offset;
			} else if (routine_offset == IBuilderData.NOT_AVAILABLE
					&& address != IBuilderData.NOT_AVAILABLE
					&& routine_address != IBuilderData.NOT_AVAILABLE) {
				routine_offset = address - routine_address;
			} else if (address == IBuilderData.NOT_AVAILABLE
					&& routine_offset != IBuilderData.NOT_AVAILABLE
					&& routine_address != IBuilderData.NOT_AVAILABLE) {
				address = routine_address + routine_offset;
			}

			String name;
			if (module != null) {
				name = module;
			} else {
				name = "";
			}
			if (file != null) {
				name = name + "(" + file;
				if (line != IBuilderData.NOT_AVAILABLE) {
					name = name + ":" + line;
				}
				name = name+")";
			}
			if (module != null) {
				ImageModule mod = fImageProcessBuilder.addLibrary(module);
				if (address != IBuilderData.NOT_AVAILABLE
						&& module_offset != IBuilderData.NOT_AVAILABLE) {
					String modAddress = "0x"+Long.toHexString(address - module_offset);
					fImageProcessBuilder.addProperty(mod, "Load address", modAddress);
				}
				if (routine != null
						&& address != IBuilderData.NOT_AVAILABLE
						&& routine_offset != IBuilderData.NOT_AVAILABLE
						&& routine_address != IBuilderData.NOT_AVAILABLE) {
					fImageProcessBuilder.addRoutine(mod, routine, routine_address);
					name = name+"::"+routine+(routine_offset >= 0 ? "+" : "-") + routine_offset;
				} else {
					if (routine_offset != IBuilderData.NOT_AVAILABLE) {
						name = name+(routine_offset >= 0 ? "+" : "-") + routine_offset;
					} else {
						if (address != IBuilderData.NOT_AVAILABLE) {
							name = name+"::0x"+Long.toHexString(address);
						}
					}
				}
			} else {
				if (routine != null) {
					if (routine_offset != IBuilderData.NOT_AVAILABLE) {
						name = "::"+routine+(routine_offset >= 0 ? "+" : "-") + routine_offset;
					} else {	
						name = "::"+routine;
					}
				} else {
					if (address != IBuilderData.NOT_AVAILABLE) {
						name = "::0x"+Long.toHexString(address);
					} else {
						name = null;
					}
				}
			}
			fImageProcessBuilder.addImageStackFrame(threadID, name, 0, address);
		}
	}

	
	/**
	 * 
	 * @param startingTag
	 * @throws ParserException
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {
//		SovereignSectionParserPart sovPart = SovereignParserPartManager.getCurrent().getSovPart(getSectionName());
//		if (sovPart != null) {
//			sovPart.computeSovRule(startingTag, getLookAheadBuffer());
//		}
	}
}
