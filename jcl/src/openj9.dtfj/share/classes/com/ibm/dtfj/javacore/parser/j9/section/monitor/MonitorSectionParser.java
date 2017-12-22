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
package com.ibm.dtfj.javacore.parser.j9.section.monitor;

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.framework.scanner.IGeneralTokenTypes;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;
import com.ibm.dtfj.javacore.parser.j9.SovereignParserPartManager;
import com.ibm.dtfj.javacore.parser.j9.SovereignSectionParserPart;

public class MonitorSectionParser extends SectionParser implements IMonitorTypes {

	private IJavaRuntimeBuilder fRuntimeBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	
	public MonitorSectionParser() {
		super(MONITOR_SECTION);
	}
	
	/**
	 * 
	 */
	protected void topLevelRule() throws ParserException {
		poolInfo();
		objectLocks();
		systemLocks();
		sovOnlyRules(T_1LKREGMONDUMP);
//		deadLocks();
	}
	
	/**
	 * 
	 * @throws ParserException
	 */
	private void poolInfo() throws ParserException {
		fImageProcessBuilder = fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder();
		fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		
		// General header info. No useful DTFJ data.
		processTagLineRequired(T_1LKPOOLINFO);
		// Sov hook: Sov only information after the 1LKPOOLINFO tag
		sovOnlyRules(T_1LKPOOLINFO);
		// Not needed in DTFJ
		processTagLineRequired(T_2LKPOOLTOTAL);
		// Sov hook: Sov only information after the 2LKPOOLTOTAL tag
		sovOnlyRules(T_2LKPOOLTOTAL);
	}
	
	/**
	 * 
	 * @throws ParserException
	 */
	private void objectLocks() throws ParserException {
		processTagLineRequired(T_1LKMONPOOLDUMP);
		monitorsInUse();
	}
	
	/**
	 * 
	 * @throws ParserException
	 */
	private void systemLocks() throws ParserException {
		processTagLineRequired(T_1LKREGMONDUMP);
		registeredMonitors();
	}

	/**
	 * Will create monitor objects for each 2LKMONINUSE encountered. 
	 * <br>
	 * <br>
	 * For every one of these tags encountered, a valid monitor ID must be parsed
	 * in order to create a valid monitor object. If a valid monitor ID is not
	 * encountered for a particular 2LKMONINUSE tag, an exception is caught and
	 * handled by the error handler. However the operation does not stop until
	 * all 2LKMONINUSE tags are parsed, whether a valid monitor object is created or not,
	 * unless the error handler decides to.
	 * <br>
	 * @throws ParserException
	 */
	private void monitorsInUse() throws ParserException {
		IAttributeValueMap results = null;
		while((results = processTagLineOptional(T_2LKMONINUSE)) != null) {
			// Ignore inflated monitor for now, use system monitor as the monitor ID
			long monitorID = results.getLongValue(SYSTEM_MONITOR);
			long objectID = IBuilderData.NOT_AVAILABLE;
			String className = null;
			long monitorThreadID = IBuilderData.NOT_AVAILABLE;
			if ((results = processTagLineRequired(T_3LKMONOBJECT)) != null) {
				className = results.getTokenValue(MONITOR_OBJECT_FULL_JAVA_NAME);
				className = fixMonitorClassName(className);
				objectID = results.getLongValue(MONITOR_OBJECT_ADDRESS);
				monitorThreadID = results.getLongValue(MONITOR_THREAD_ID);
			}
			try {
				/*
				 * A monitor object must be created if the 2LKMONINUSE tag is encountered, else an 
				 * error encountered.
				 */
				generateMonitor(null, monitorID, objectID, className, monitorThreadID);
			} catch (BuilderFailureException e) {
				handleError("Could not add monitor to builder: " + monitorID, e);
			}
		}
	}

	static String fixMonitorClassName(String className) {
		className = className.replaceAll("\\.", "/"); // dot to slash
		if (className.endsWith("]")) {
			// Sov has unusual names for arrays
			// [B[1] 2d array -> [[B
			// byte[][2] 1d array -> [B
			// java.lang.String[4] -> [Ljava.lang.String;
			// [Ljava.lang.String;[3] -> [[Ljava.lang.String;
			int i = className.lastIndexOf('[');
			if (i > 0) className = className.substring(0, i);
			if (className.startsWith("[")) {
				className = "["+className;
			} else if (!className.endsWith("]")) {
				className = "[L"+className + ";";
			} else if (className.equals("byte[]")) {
				className = "[B";
			} else if (className.equals("short[]")) {
				className = "[S";
			} else if (className.equals("int[]")) {
				className = "[I";
			} else if (className.equals("long[]")) {
				className = "[J";
			} else if (className.equals("float[]")) {
				className = "[F";
			} else if (className.equals("double[]")) {
				className = "[D";
			} else if (className.equals("boolean[]")) {
				className = "[Z";
			} else if (className.equals("char[]")) {
				className = "[C";
			}
		}
		return className;
	}
	
	/**
	 * Must pass a valid monitor ID in order to successfully create a monitor object.
	 * 
	 * @param monitorName optional
	 * @param monitorID required: must pass a valid ID or exception is thrown.
	 * @param objectID optional
	 * @param className optional
	 * @param owningThread optional
	 * 
	 * @throws ParserException
	 * @throws BuilderFailureException if invalid monitorID is passed.
	 */
	private JavaMonitor generateMonitor(String monitorName, long monitorID, long objectID, String className, long owningThread) throws ParserException, BuilderFailureException{
		JavaMonitor monitor = fRuntimeBuilder.addJavaMonitor(monitorName, monitorID, objectID, className, owningThread);
		waitOnNotifyOrEnter(monitor);
		return monitor;
	}
	
	/**
	 * This handles both wait on notify and wait on enter threads for a given java monitor.
	 * <br><br>
	 * If either a wait on notify or wait on enter header tag is parsed (3LKNOTIFYQ or 3LKWAITNOTIFY, respectively),
	 * its corresponding data tag (3LKWAITNOTIFY and 3LKWAITER, respectively) is parsed iteratively.
	 * For each of the data tags, a valid threadID must be parsed. If not, an exception is thrown and
	 * the thread is not added to the monitor wait notify/wait enter thread list.However, the process
	 * will continue regardless of whether errors are encountered or not, as long as the error handler permits it to
	 * do so.
	 * <br>
	 * A valid thread ID must be parsed for each 3LKWAITNOTIFY or 3LKWAITER encountered, or error handler is invoked.
	 * @throws ParserException
	 */
	private void waitOnNotifyOrEnter(JavaMonitor monitor) throws ParserException  {
		boolean waitOnNotify = false;
		String tagName = null;
		while((waitOnNotify = matchOptional(tagName = T_3LKNOTIFYQ)) || matchOptional(T_3LKWAITERQ)) {
			// Consume header tag
			consume();
			// Consume any unnecessary data contained in the same header tag line:
			IParserToken token = lookAhead(1);
			if (token != null && token.getType().equals(IGeneralTokenTypes.UNPARSED_STRING)) {
				consume();
			}
			tagName = (waitOnNotify) ? T_3LKWAITNOTIFY : T_3LKWAITER;

			IAttributeValueMap results = null;
			// This must match at least once. Subsequent matches are optional.
			if ((results = processTagLineRequired(tagName)) != null) {
				processNotifyOrEnterThreads(monitor, results, waitOnNotify);
				while((results = processTagLineOptional(tagName)) != null) {
					processNotifyOrEnterThreads(monitor, results, waitOnNotify);
				}
			}
		}
	}
	
	/**
	 * 
	 * @param monitor
	 * @param results
	 * @param waitOnNotify
	 */
	private void processNotifyOrEnterThreads(JavaMonitor monitor, IAttributeValueMap results, boolean waitOnNotify) throws ParserException {
		String threadName = results.getTokenValue(MONITOR_THREAD_NAME);
		if (threadName != null && threadName.length() >= 2) {
			threadName = threadName.substring(1, threadName.length() - 1); // remove quotes
		}
		long threadID = results.getLongValue(MONITOR_THREAD_ID);
		try {
			if (waitOnNotify) {
				fRuntimeBuilder.addWaitOnNotifyThread(monitor, threadID);
			}
			else {
				fRuntimeBuilder.addBlockedThread(monitor, threadID);
			}
		} catch (BuilderFailureException e) {
			String message = (waitOnNotify) ? "Could not add wait on notify thread" : "could not add blocked thread";
			message += ", thread name: " + threadName + ", thread ID: " + threadID;
			ImagePointer id = monitor.getID();
			if (id != null) {
				message += ", monitor ID: " + id.getAddress();
			}
			handleError(message, e);
		}
	}
	
	/**
	 * 
	 * @throws ParserException
	 */
	private void registeredMonitors() throws ParserException {
		IAttributeValueMap results = null;
		while((results = processTagLineOptional(T_2LKREGMON)) != null) {
			processRegMonitors(results);
		}
	}
	
	/**
	 * 
	 * @param results
	 * @throws ParserException
	 */
	private void processRegMonitors(IAttributeValueMap results) throws ParserException {
		if (results != null) {
			String monitorName = results.getTokenValue(MONITOR_NAME);
			long monitorID = results.getLongValue(MONITOR_ADDRESS);
			try {
				generateMonitor(monitorName, monitorID, IBuilderData.NOT_AVAILABLE, null, IBuilderData.NOT_AVAILABLE);
			} catch (BuilderFailureException e) {
				handleError("Failed to add monitor: " + monitorName + ", " + monitorID, e);
			}
		}
	}
	
	
	/**
	 * Sov specific lock information.
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {
		SovereignSectionParserPart sovPart = SovereignParserPartManager.getCurrent().getSovPart(getSectionName());
		if (sovPart != null) {
			sovPart.readIntoDTFJ(getLookAheadBuffer(), fImageBuilder); // ???
			sovPart.computeSovRule(startingTag, getLookAheadBuffer());
		}
	}
	
	/*
	 * NOTE: deadlock parsing not tested. No DTFJ builder for deadlocks.
	 */
//	/**
//	 * Note: Called by the topline rule. Be sure to uncomment/insert the call in the top line rule.
//	 * @throws ParserException
//	 */
//	private void deadLocks() throws ParserException {
//		while(matchOptional(T_1LKDEADLOCK)) {
//			consume();
//			deadLockThread();
//		}
//	}
//	
//	/**
//	 * Here, a dead lock thread points to further threads until
//	 * the original dead lock thread is reached (i.e a cycle).
//	 * Grammatically, this means that this section must start with
//	 * a dead lock thread header containing a  thread section, and
//	 * then point to zero or more other threads, each with their own thread header and section.
//	 * The cycle then ends when the initial thread points back to its own header (but not the section).
//	 * <br>
//	 * <br>
//	 * algorithm:
//	 * <br>
//	 * <br>
//	 * starting_deadlock_thread_header
//	 * <br>
//	 * process_starting_ThreadSection
//	 * <br>
//	 * while (other_thread_header) {
//	 * <br>
//	 * process_other_ThreadSection }
//	 * <br>
//	 * starting_deadlock_thread_header
//	 * <br>
//	 * @throws ParserException
//	 */
//	private void deadLockThread() throws ParserException {
//		if (matchRequired(T_2LKDEADLOCKTHR)) {
//			consume();
//			boolean hasDeadLockSubsection = deadLockThreadSection(true);
//			while(hasDeadLockSubsection) {
//				if (matchRequired(T_2LKDEADLOCKTHR)) {
//					consume();
//					hasDeadLockSubsection = deadLockThreadSection(false);
//				}
//				else {
//					hasDeadLockSubsection = false;
//				}
//			}
//		}
//	}
//	
//	/**
//	 * 
//	 * @param required
//	 * 
//	 * @throws ParserException
//	 */
//	private boolean deadLockThreadSection(boolean required) throws ParserException {
//		boolean wasMatched = false;
//		if (wasMatched = (required) ? matchRequired(T_3LKDEADLOCKWTR) : matchOptional(T_3LKDEADLOCKWTR)) {
//			consume();
//			if (matchOptional(T_4LKDEADLOCKMON)) {
//				consume();
//				processTagLineRequired(T_4LKDEADLOCKOBJ);
//			}
//			processTagLineRequired(T_4LKDEADLOCKREG);
//			processTagLineRequired(T_3LKDEADLOCKOWN);
//		}
//		return wasMatched;
//	}
	

}
