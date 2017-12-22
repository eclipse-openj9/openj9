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

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class MonitorTagParser extends TagParser implements IMonitorTypes {

	public MonitorTagParser() {
		super(MONITOR_SECTION);
	}

	protected void initTagAttributeRules() {
		addTag(T_1LKPOOLINFO, null);
		addPoolTotal();
		addTag(T_1LKMONPOOLDUMP, null);
		addMonInUse();
		addMonObject();
		addTag(T_3LKWAITERQ, null);
		addWaiter();
		addTag(T_3LKNOTIFYQ, null);
		addWaitNotify();
		addTag(T_1LKREGMONDUMP, null);
		addRegMon();
		
		// Deadlock support skipped for now.
//		addTag(T_1LKDEADLOCK, null);
//		addDeadLockThr();
//		addTag(T_3LKDEADLOCKWTR, null);
//		addDeadLockMon();
//		addDeadLockObj();
//		addDeadLockReg();
//		addTag(T_3LKDEADLOCKOWN, null);
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// Thread identifiers (as used in flat monitors):
				addToken(IMonitorTypes.MONITOR_SECTION, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(IMonitorTypesSov.LK_FLAT_MON_DUMP, lineRule);
		ILineRule lineRule2 = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// ident 0x05 "Finalizer" (0x14B09E0) ee 0x014B0800
				addPrefixedHexToken(IMonitorTypesSov.MONITOR_FLAT_ID);
				addToken(MONITOR_THREAD_NAME, CommonPatternMatchers.quoted_stringvalue);
				addPrefixedHexToken(MONITOR_THREAD_ID);
				addPrefixedHexToken(IMonitorTypesSov.MONITOR_THREAD_EE);
			}
		};
		addTag(IMonitorTypesSov.LK_FLAT_MON, lineRule2);
		addTag(IMonitorTypesSov.LK_OBJ_MON_DUMP, null);
		ILineRule lineRule3 = new MonitorObjectLineRule() {
			public void processLine(String source, int startingOffset) {
				// java.lang.String@C1FF58/C1FF60
				addMonitorObjectNameAndAddress();
			}
		};
		addTag(IMonitorTypesSov.LK_INFLATED_MON, lineRule3);
		addTag(IMonitorTypesSov.LK_INFL_DETAILS, null);
		addTag(IMonitorTypesSov.LK_FLAT_LOCKED, lineRule3);
		ILineRule lineRule4 = new LineRule() {
			public void processLine(String source, int startingOffset) {
				//     locknflags 00050000 Flat locked by thread ident 0x05, entry count 1
				addPrefixedHexToken(IMonitorTypesSov.MONITOR_FLAT_ID);
			}
		};
		addTag(IMonitorTypesSov.LK_FLAT_DETAILS, lineRule4);
	}
	
	/**
	 * 
	 *
	 */
	private void addPoolTotal() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addToken(TOTAL_MONITORS, CommonPatternMatchers.dec);
			}
		};
		addTag(T_2LKPOOLTOTAL, lineRule);
	}
	
	/**
	 * 
	 *
	 */
	private void addMonInUse() {
		ILineRule lineRule = new MonitorObjectLineRule() {
			public void processLine(String source, int startingOffset) {
				addSystemAndInflatedThdInfo();
			}
		};
		addTag(T_2LKMONINUSE, lineRule);
	}
	
	/**
	 * 
	 *
	 */
	private void addMonObject() {
		ILineRule lineRule = new MonitorObjectLineRule() {
			public void processLine(String source, int startingOffset) {
				addMonitorObjectNameAndAddress();
			}
		};
		addTag(T_3LKMONOBJECT, lineRule);
	}
	
	/**
	 * 
	 *
	 */
	private void addWaiter() {
		ILineRule lineRule = new MonitorObjectLineRule()	 {
			public void processLine(String source, int startingOffset) {
				addVMThreadInformation();
			}
		};
		addTag(T_3LKWAITER, lineRule);
	}
	
	/**
	 * 
	 *
	 */
	private void addWaitNotify() {
		ILineRule lineRule = new MonitorObjectLineRule()	 {
			public void processLine(String source, int startingOffset) {
				addVMThreadInformation();
			}	
		};
		addTag(T_3LKWAITNOTIFY, lineRule);
	}
	
	/**
	 * 
	 *
	 */
	private void addRegMon() {
		ILineRule lineRule = new MonitorObjectLineRule()	 {
			public void processLine(String source, int startingOffset) {
				addMonitorLockNameAndAddress();
			}
		};
		addTag(T_2LKREGMON, lineRule);
	}
	
	/*
	 * No DTFJ support for deadlocks. Partially implemented below, but not tested.
	 */
//	/**
//	 * 
//	 *
//	 */
//	private void addDeadLockThr() {
//		ILineRule lineRule = new MonitorObjectLineRule()	 {
//			public void processLine(String source, int startingOffset) {
//				addVMThreadInformation();
//			}
//		};
//		addTag(T_2LKDEADLOCKTHR, lineRule);
//	}
//	
//	/**
//	 * 
//	 *
//	 */
//	private void addDeadLockMon() {
//		ILineRule lineRule = new MonitorObjectLineRule()	 {
//			public void processLine(String source, int startingOffset) {
//				addSystemAndInflatedThdInfo();
//			}	
//		};
//		addTag(T_4LKDEADLOCKMON, lineRule);	
//	}
//	
//	/**
//	 * 
//	 *
//	 */
//	private void addDeadLockObj() {
//		ILineRule lineRule = new MonitorObjectLineRule()	 {
//			public void processLine(String source, int startingOffset) {
//				addMonitorObjectNameAndAddress();
//			}
//		};
//		addTag(T_4LKDEADLOCKOBJ, lineRule);
//	}
//	
//	/**
//	 * 
//	 *
//	 */
//	private void addDeadLockReg() {
//		ILineRule lineRule = new MonitorObjectLineRule()	 {
//			public void processLine(String source, int startingOffset) {
//				addMonitorLockNameAndAddress();
//			}
//		};
//		addTag(T_4LKDEADLOCKREG, lineRule);
//	}
}
