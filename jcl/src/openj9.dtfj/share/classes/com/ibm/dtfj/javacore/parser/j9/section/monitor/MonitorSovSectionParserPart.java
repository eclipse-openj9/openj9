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
package com.ibm.dtfj.javacore.parser.j9.section.monitor;

import java.util.HashMap;

import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ILookAheadBuffer;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.J9TagManager;
import com.ibm.dtfj.javacore.parser.j9.SovereignSectionParserPart;

public class MonitorSovSectionParserPart extends SovereignSectionParserPart implements IMonitorTypes, IMonitorTypesSov {

	private IJavaRuntimeBuilder fRuntimeBuilder;
	private IImageProcessBuilder fImageProcessBuilder;

	public MonitorSovSectionParserPart() {
		super(MONITOR_SECTION);
	}
	
	public void computeSovRule(String sovRuleID, ILookAheadBuffer lookAheadBuffer) throws ParserException {
		setLookAheadBuffer(lookAheadBuffer);
		setTagManager(J9TagManager.getCurrent());
		if (sovRuleID.equals(T_1LKREGMONDUMP)) {
			IAttributeValueMap results;
			if ((results = processTagLineOptional(LK_FLAT_MON_DUMP)) != null) {
				HashMap threads = new HashMap();
				while ((results = processTagLineOptional(LK_FLAT_MON)) != null) {
					int flatid = results.getIntValue(MONITOR_FLAT_ID);
					String name = results.getTokenValue(MONITOR_THREAD_NAME);
					// Remove quotes
					if (name != null && name.length() >= 2) {
						name = name.substring(1, name.length() - 1);
					}
					long tid = results.getLongValue(MONITOR_THREAD_ID);
					long ee = results.getLongValue(MONITOR_THREAD_EE);
					// Remember the flat thread id for later
					threads.put(Integer.valueOf(flatid), Long.valueOf(tid));
					try {
						fRuntimeBuilder.addJavaThread(null, name, IBuilderData.NOT_AVAILABLE, tid, IBuilderData.NOT_AVAILABLE, ee, "", IBuilderData.NOT_AVAILABLE, 0, null);
					} catch (BuilderFailureException e) {
						// Expect a failure, but the JNIEnv will be remembered
					}
				}

				results = processTagLineRequired(LK_OBJ_MON_DUMP);
				do {
					if ((results = processTagLineOptional(LK_INFLATED_MON)) != null) {				
						processTagLineRequired(LK_INFL_DETAILS);
					} else {
						if ((results = processTagLineOptional(LK_FLAT_LOCKED)) != null) {						
							String className = results.getTokenValue(MONITOR_OBJECT_FULL_JAVA_NAME);
							className = MonitorSectionParser.fixMonitorClassName(className);
							long objectID = results.getLongValue(MONITOR_OBJECT_ADDRESS);
							long monitorID = results.getLongValue(MONITOR_OBJECT_ADDRESS);
							results = processTagLineRequired(LK_FLAT_DETAILS);
							int flatid = results.getIntValue(MONITOR_FLAT_ID);
							Object p = threads.get(Integer.valueOf(flatid));
							long threadID = p instanceof Long ? ((Long)p).longValue() : IBuilderData.NOT_AVAILABLE; 
							try {
								if (monitorID != IBuilderData.NOT_AVAILABLE) {
									// A valid monitor ID is required to build a monitor
									fRuntimeBuilder.addJavaMonitor(null, monitorID, objectID, className, threadID);
								}
							} catch (BuilderFailureException e) {
								handleError("Could not add monitor to builder: " + objectID, e);
							}
						}
					}
				} while (results != null);
			}
		}		
	}

	public Object readIntoDTFJ(ILookAheadBuffer lookAhead) throws ParserException {
		
		return null;
	}

	public void readIntoDTFJ(ILookAheadBuffer lookAhead, IImageBuilder imageBuilder) throws ParserException {
		fImageProcessBuilder = imageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder();
		fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		
	}

}
