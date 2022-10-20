/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dtfj.javacore.parser.j9.section.gchistory;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

/**
 * Parser rules for lines in the GC history section in the javacore.
 */
public class GCHistoryTagParser extends TagParser implements IGCHistoryTypes {

	/**
	 * Construct a tag parser that handles the tags
	 * and data from the GC history section of a javacore.
	 */
	public GCHistoryTagParser() {
		super(GCHISTORY_SECTION);
	}

	/**
	 * Initialize parser with rules for lines in the GC history section in
	 * the javacore.
	 */
	@Override
	protected void initTagAttributeRules() {
		ILineRule lineRule = new LineRule() {
			@Override
			public void processLine(String source, int startingOffset) {
				// The name of the history is on a single line.
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(GCHISTORY_NAME, CommonPatternMatchers.allButLineFeed);
			}
		};

		addTag(T_1STGCHTYPE, lineRule);

		lineRule = new LineRule() {
			// 3STHSTTYPE     04:36:21:235528600 GMT j9mm.51 -   SystemGC end: newspace=1794016/2097152 oldspace=4050144/9961472 loa=498688/498688
			@Override
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(GCHISTORY_LINE, CommonPatternMatchers.allButLineFeed);
			}
		};

		addTag(T_3STHSTTYPE, lineRule);
	}

}
