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

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public abstract class MonitorObjectLineRule extends LineRule implements IMonitorTypes{
	/**
	 * 
	 *
	 */
	protected void addMonitorObjectOwnedAttributes() {
		if(addToken(UNOWNED, MonitorPatternMatchers.unowned) == null) {
			if (findFirst(MonitorPatternMatchers.flat_lock_by)) {
				addToken(FLATLOCKED, MonitorPatternMatchers.flat_lock_by);
			}
			addVMThreadInformation();
			addToken(MONITOR_ENTRY_COUNT, CommonPatternMatchers.dec);
		}
	}
	
	protected void addVMThreadInformation() {
		addToken(MONITOR_THREAD_NAME, CommonPatternMatchers.quoted_stringvalue);
		// Some older versions of Sovereign display the threadID without the "0x" prefix.
		addHexToken(MONITOR_THREAD_ID);
	}
	
	/**
	 * 
	 *
	 */
	protected void addMonitorLockNameAndAddress() {
		consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
		addAllCharactersAsTokenAndConsumeFirstMatch(MONITOR_NAME, MonitorPatternMatchers.lock);
		addPrefixedHexToken(MONITOR_ADDRESS);
	}
	
	protected IParserToken addHexToken(String token) {
		IParserToken ret;
		// Be careful as the thread ID can be prefixed while the monitor object might not be
		CommonPatternMatchers.hex_0x.reset(fSource);
		if (CommonPatternMatchers.hex_0x.lookingAt()) { 
			// Immediate 0x
			ret = addPrefixedHexToken(token);
		} else {
			CommonPatternMatchers.hex.reset(fSource);
			if (CommonPatternMatchers.hex.lookingAt()) {
				// Immediate non prefixed hex
				// Some older versions of Sovereign or J9 display the threadID/monitor object without the "0x" prefix.
				ret = addNonPrefixedHexToken(token);
			} else {
				// Try looking for 0x first
				ret = addPrefixedHexToken(token);
				if (ret == null) {
					// Some older versions of Sovereign or J9 display the threadID/monitor object without the "0x" prefix.
					ret = addNonPrefixedHexToken(token);
				};
			}
		};
		return ret;
	}
	
	/**
	 * 
	 *
	 */
	protected void addMonitorObjectNameAndAddress() {
		consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
		if (findFirst(CommonPatternMatchers.at_symbol)) {
			addAllCharactersAsTokenAndConsumeFirstMatch(MONITOR_OBJECT_FULL_JAVA_NAME, CommonPatternMatchers.at_symbol);
			addHexToken(MONITOR_OBJECT_ADDRESS);
			consumeUntilFirstMatch(CommonPatternMatchers.forward_slash);
			addHexToken(MONITOR_WORD_ADDRESS_IN_HEADER);
		} else {
			if (findFirst(CommonPatternMatchers.colon)) {
				addAllCharactersAsTokenAndConsumeFirstMatch(MONITOR_OBJECT_FULL_JAVA_NAME, CommonPatternMatchers.colon);
			} else {
				addToken(MONITOR_OBJECT_FULL_JAVA_NAME, CommonPatternMatchers.allButLineFeed);
			}
		}
		consumeUntilFirstMatch(CommonPatternMatchers.colon);
		addMonitorObjectOwnedAttributes();
	}
	
	
	
	/**
	 * 
	 *
	 */
	protected void addSystemAndInflatedThdInfo() {
		consumeUntilFirstMatch(CommonPatternMatchers.colon);
		addPrefixedHexToken(SYSTEM_MONITOR);
		consumeUntilFirstMatch(CommonPatternMatchers.colon);
		addPrefixedHexToken(INFLATED_MONITOR);
	}
}
