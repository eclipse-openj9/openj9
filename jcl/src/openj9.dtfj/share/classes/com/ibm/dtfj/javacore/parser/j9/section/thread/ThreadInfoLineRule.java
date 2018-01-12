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

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class ThreadInfoLineRule extends LineRule {

	/**
	 * Most VMs list hexadecimal values with the 0x prefix, but
	 * at least one older Sovereign VM does not, therefore check for both
	 * patterns.
	 * 
	 * 7.0
	   3XMTHREADINFO      "main" J9VMThread:0x000CC100, j9thread_t:0x003D5600, java/lang/Thread:0x00A72440, state:R, prio=5
	   3XMTHREADINFO1            (native thread ID:0x1A58, native priority:0x5, native policy:UNKNOWN)
	   3XMTHREADINFO3           Java callstack:
	   4XESTACKTRACE                at java/lang/Runtime.maxMemory(Native Method)
	   3XMTHREADINFO3           Native callstack:
	   4XENATIVESTACK               JVM_MaxMemory+0x5 (vmi.c:209, 0x00996FF5 [jvm+0x6ff5])
	   3XMTHREADINFO      Anonymous native thread
	   3XMTHREADINFO1            (native thread ID:0x378C, native priority: 0x0, native policy:UNKNOWN)
	   3XMTHREADINFO3           Native callstack:
	 * 6.0
	 * 2XMFULLTHDDUMP Full thread dump J9 VM (J2RE 6.0 IBM J9 2.4 Windows XP x86-32 build jvmwi3260-20090215_2988320090215_029883_lHdSMr, native threads):
	   3XMTHREADINFO      "JIT Compilation Thread" TID:0x000A9C00, j9thread_t:0x00015370, state:CW, prio=10
	   3XMTHREADINFO1            (native thread ID:0x267C, native priority:0xB, native policy:UNKNOWN)
	   5.0
	   2XMFULLTHDDUMP Full thread dump J9SE VM (J2RE 5.0 IBM J9 2.3 Windows XP x86-32 build 20051027_03723_lHdSMR, native threads):
	   3XMTHREADINFO      "main" (TID:0x0008E900, sys_thread_t:0x00387A64, state:CW, native ID:0x0000083C) prio=5
	 * 1.4.2
	 * 2XMFULLTHDDUMP Full thread dump Classic VM (J2RE 1.4.2 IBM Windows 32 build cn142-20080130 (SR10), native threads):
	   3XMTHREADINFO      "Finalizer" (TID:0xBAE0B0, sys_thread_t:0x16F29E8, state:CW, native ID:0x14D8) prio=8
	 */
	public void processLine(String source, int startingOffset) {
		/*
		 * JAVA_THREAD_NAME
		 */
		addToken(IThreadTypes.JAVA_THREAD_NAME, CommonPatternMatchers.quoted_stringvalue);
		
		/*
		 * VM_THREAD_ID
		 */
		boolean ret = consumeUntilFirstMatch(CommonPatternMatchers.colon);
		// Give up if no thread ID 
		if (!ret) return;
		addHexValue(IThreadTypes.VM_THREAD_ID);
		
		/*
		 * ABSTRACT_THREAD_ID
		 */
		consumeUntilFirstMatch(CommonPatternMatchers.colon);
		addHexValue(IThreadTypes.ABSTRACT_THREAD_ID);
		
		/*
		 * optional Java thread object
		 */
		if (findFirst(CommonPatternMatchers.forward_slash)) {
			consumeUntilFirstMatch(CommonPatternMatchers.colon);
			addHexValue(IThreadTypes.JAVA_THREAD_OBJ);
		}
		
		/*
		 * Java thread state. Note this state field was actually the internal VM thread state until corrected under LIR 14327. The
		 * DTFJ implementation for javacore exposed it via JavaThread.getState() so we continue to do that with corrected value.
		 */
		consumeUntilFirstMatch(CommonPatternMatchers.colon);
		addToken(IThreadTypes.JAVA_STATE, CommonPatternMatchers.lettervalue);
	
		/*
		 * NATIVE_THREAD_ID (SOV 1.4.2 only).  
		 */
		if (consumeUntilFirstMatch(CommonPatternMatchers.colon)) {
			addHexValue(IThreadTypes.NATIVE_THREAD_ID);
		}
		
		/*
		 * VM_THREAD_PRIORITY
		 */
		consumeUntilFirstMatch(CommonPatternMatchers.equals);
		addToken(IThreadTypes.VM_THREAD_PRIORITY, CommonPatternMatchers.dec);
		
	}
	
	
	/**
	 * 
	 * @param attribute
	 * 
	 */
	private IParserToken addHexValue(String attribute) {
		IParserToken token = null;
		if ((token = addPrefixedHexToken(attribute)) == null) {
			token = addNonPrefixedHexToken(attribute);
		}
		return token;
	}
}
