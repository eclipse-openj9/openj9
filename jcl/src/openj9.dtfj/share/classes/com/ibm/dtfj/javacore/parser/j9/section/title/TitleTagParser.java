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
package com.ibm.dtfj.javacore.parser.j9.section.title;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class TitleTagParser extends TagParser implements ITitleTypes {
	
	public TitleTagParser() {
		super(TITLE_SECTION);
	}

	/**
	 * Initialize parser with rules for the TITLE section in the javacore
	 */
	protected void initTagAttributeRules() {

		addTag(T_1TISIGINFO, null);
		addDateTime();
		addNanoTime();
		addFileName();
	}

	/**
	 * Add rule for the dump date (1TIDATETIME line)
	 */
	private void addDateTime() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 1TIDATETIME    Date:    2009/02/09 at 15:48:30
				//  or (Java 8 SR2 or later, with millisecs added):
				// 1TIDATETIME    Date:    2015/07/17 at 09:46:27:261
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(TI_DATE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1TIDATETIME, lineRule);
	}
	
	/**
	 * Add rule for the dump nanotime (1TINANOTIME line)
	 */
	private void addNanoTime() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				// 1TINANOTIME    System nanotime: 3534320355422
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(TI_NANO, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1TINANOTIME, lineRule);
	}
	

	/**
	 * Add rule for the dump filename (1TIFILENAME line)
	 */
	private void addFileName() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				//1TIFILENAME    Javacore filename:    C:\Documents and Settings\Administrator\My Documents\javacore.20080219.155641.3836.0003.txt
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(TI_FILENAME, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1TIFILENAME, lineRule);
	}

}
