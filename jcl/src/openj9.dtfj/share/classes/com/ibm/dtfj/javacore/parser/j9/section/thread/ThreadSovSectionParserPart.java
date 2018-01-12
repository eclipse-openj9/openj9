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

import com.ibm.dtfj.javacore.builder.IImageBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ILookAheadBuffer;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.J9TagManager;
import com.ibm.dtfj.javacore.parser.j9.SovereignSectionParserPart;

public class ThreadSovSectionParserPart extends SovereignSectionParserPart implements IThreadTypes, IThreadTypesSov {
	

	
	public ThreadSovSectionParserPart() {
		super(THREAD_SECTION);

	}
	

	


	public Object readIntoDTFJ(ILookAheadBuffer lookAhead) throws ParserException {
		return null;
	}




	/**
	 * 
	 */
	public void computeSovRule(String hookTag, ILookAheadBuffer lookAheadBuffer) throws ParserException {
		setLookAheadBuffer(lookAheadBuffer);
		setTagManager(J9TagManager.getCurrent());
		if (hookTag.equals(T_3XMTHREADINFO)) {
			registerList();
			nativeStack();
		}
	}
	
	
	
	/**
	 * registerList ::= REGISTER_LIST rule registerValues <br>
	 * registerValues ::= REGISTER_LIST_VALUES rule registerValues | EPSILON <br>
	 * @throws ParserException
	 */
	private void registerList() throws ParserException {
//		if (processTagLineOptional(REGISTER_LIST)) {
//			matchAndSendTagOnlyAtLeastOnce(REGISTER_LIST_VALUES);
//		}
	}
	
	
	/**
	 * nativeStack ::= NATIVE_STACK rule stackLine <br>
	 * stackLine ::= STACK_LINE rule stackLine | EPSILON
	 * <br><br>
	 * Native stacks are optional, but if they are present,
	 * each must have at least one stack line.
	 * <br><br>
	 * In the case of AIX: <br><br>
	 * If the native stack doesn't have one or more stack lines
	 * it must have a stack line error.
	 * @throws ParserException
	 */
	private void nativeStack() throws ParserException {
//		if (processTagLineOptional(NATIVE_STACK)) {
//			matchAndSendTagOnlyAtLeastOnce(STACK_LINE);
//		}
//		else if (processTagLineOptional(NATIVE_STACK_AIX)) {
//			if (!processTagLineOptional(STACK_LINE_ERR_AIX)) {
//				while(processTagLineOptional(STACK_LINE_AIX));
//			}
//		}
	}





	public void readIntoDTFJ(ILookAheadBuffer lookAhead, IImageBuilder imageBuilder) throws ParserException {
		// TODO Auto-generated method stub
		
	}


}
