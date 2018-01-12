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

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;
import com.ibm.dtfj.javacore.parser.j9.section.common.PointerSizeLineRule;

public class ThreadTagParser extends TagParser {
	
	public ThreadTagParser() {
		super(IThreadTypes.THREAD_SECTION);
	}
	
	/**
	 * Call inherited method addTag() to add a tag->rule pair. 
	 */
	protected void initTagAttributeRules() {	
		/*
		 * Tags with attributes
		 */
		initThreadInfoTag();
		initThreadInfo1Tag();
		initThreadInfo2Tag();
		initCPUTimeTag();
		initThreadBlockTag();
		initStackTraceTag();
		initNativeStackTraceTag();
		initOSInfoTag();
		
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
			}
		};
		addTag(IThreadTypes.T_1XMCURTHDINFO, lineRule);
		/*
		 * Tags with no attributes (or attributes to be ignored)
		 */
		addTag(IThreadTypes.T_1XMTHDINFO, null);
	}

	private void initCPUTimeTag() {
		// 3XMCPUTIME               CPU usage total: 9.656250000 secs, user: 4.781250000 secs secs, system: 4.875000000 secs
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				
				consumeUntilFirstMatch(ThreadPatternMatchers.cpu_time_total);
				addToken(IThreadTypes.CPU_TIME_TOTAL, ThreadPatternMatchers.cpu_time);
				
				/*
				 * Optional: user time
				 */
				if( consumeUntilFirstMatch(ThreadPatternMatchers.cpu_time_user)) {
					addToken(IThreadTypes.CPU_TIME_USER, ThreadPatternMatchers.cpu_time);
				}
				
				/*
				 * Optional: system time
				 */
				if( consumeUntilFirstMatch(ThreadPatternMatchers.cpu_time_system)) {
					addToken(IThreadTypes.CPU_TIME_SYSTEM, ThreadPatternMatchers.cpu_time);
				}
			}
		};
		addTag(IThreadTypes.T_3XMCPUTIME, lineRule);
	}

	/**
	 * Both J9 and SOV 1.4.2 , and J9 5.0 (2.3 - 2.4)
	 *
	 */
	private void initThreadInfoTag() {
		ILineRule lineRule = new ThreadInfoLineRule();
		addTag(IThreadTypes.T_3XMTHREADINFO, lineRule);
	}
	
	
	/**
	 * 
	 *
	 */
	private void initOSInfoTag() {
		ILineRule lineRule = new PointerSizeLineRule();
		addTag(IThreadTypes.T_2XMFULLTHDDUMP, lineRule);
	}
	
	
	/**
	 *
	 */
	private void initThreadInfo1Tag() {
		// 3XMTHREADINFO1            (native thread ID:0x88C, native priority:0x5, native policy:UNKNOWN, vmstate:R, vm thread flags:0x00000020)
		// vmstate and vm thread flags added in LIR 14327
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				/*
				 * NATIVE_THREAD_ID
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(IThreadTypes.NATIVE_THREAD_ID);
				/*
				 * NATIVE_THREAD_PRIORITY
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(IThreadTypes.NATIVE_THREAD_PRIORITY);
				/*
				 * NATIVE_THREAD_POLICY
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addToken(IThreadTypes.NATIVE_THREAD_POLICY, CommonPatternMatchers.lettervalue);
				
				/*
				 * Optional: Native Scope
				 */
				if(findFirst(ThreadPatternMatchers.scope) && consumeUntilFirstMatch(CommonPatternMatchers.colon)) {
					addPrefixedHexToken(IThreadTypes.SCOPE);
				}
				/*
				 * Optional: VM thread state
				 */
				if(findFirst(ThreadPatternMatchers.vmstate) && consumeUntilFirstMatch(CommonPatternMatchers.colon)) {
					addToken(IThreadTypes.VM_STATE, CommonPatternMatchers.lettervalue);
				}
				/*
				 * Optional: VM thread flags
				 */
				if(findFirst(ThreadPatternMatchers.vmflags) && consumeUntilFirstMatch(CommonPatternMatchers.colon)) {
					addPrefixedHexToken(IThreadTypes.VM_FLAGS);
				}
				
			}	
		};	
		addTag(IThreadTypes.T_3XMTHREADINFO1, lineRule);	
	}
	
	/**
	 * Parse the native stack address range information
	 */
	private void initThreadInfo2Tag() {
		// 3XMTHREADINFO2            (native stack address range from:0x915CA000, to:0x91589000, size:0x41000)
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				/*
				 * NATIVE_STACK_FROM
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(IThreadTypes.NATIVE_STACK_FROM);
				/*
				 * NATIVE_STACK_TO
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(IThreadTypes.NATIVE_STACK_TO);
				/*
				 * NATIVE_STACK_SIZE
				 */
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				addPrefixedHexToken(IThreadTypes.NATIVE_STACK_SIZE);
			}
		};
		addTag(IThreadTypes.T_3XMTHREADINFO2, lineRule);
	}
	
	/**
	 * 
	 * Both Sov and J9
	 */
	private void initStackTraceTag() {
		addTag(IThreadTypes.T_3XMTHREADINFO3, null);
		ILineRule lineRule = new StackTraceLineRule();
		addTag(IThreadTypes.T_4XESTACKTRACE, lineRule);	
	}

	/**
	 * 
	 */
	private void initNativeStackTraceTag() {
		ILineRule lineRule = new NativeStackTraceLineRule();
		addTag(IThreadTypes.T_4XENATIVESTACK, lineRule);	
	}
	
	/**
	 * J9 R2.6+ only
	 */
	private void initThreadBlockTag() {
		// 3XMTHREADBLOCK     Blocked on: java/lang/String@0x4D8C90F8 Owned by: "main" (J9VMThread:0x00129100, java/lang/Thread:0x00DD4798)
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				/*
				 * BLOCKER_OBJECT_FULL_JAVA_NAME
				 */
				addAllCharactersAsTokenAndConsumeFirstMatch(IThreadTypes.BLOCKER_OBJECT_FULL_JAVA_NAME, CommonPatternMatchers.at_symbol);
				/*
				 * BLOCKER_OBJECT_ADDRESS
				 */
				addPrefixedHexToken(IThreadTypes.BLOCKER_OBJECT_ADDRESS);
			}
		};
		addTag(IThreadTypes.T_3XMTHREADBLOCK, lineRule);
		
	}
}
