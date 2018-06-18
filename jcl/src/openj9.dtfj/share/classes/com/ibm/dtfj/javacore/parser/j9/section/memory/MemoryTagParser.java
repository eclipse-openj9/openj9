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
package com.ibm.dtfj.javacore.parser.j9.section.memory;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class MemoryTagParser extends TagParser implements IMemoryTypes {
	
	public MemoryTagParser() {
		super(MEMORY_SECTION);
	}

	/**
	 * Initialize parser with rules for lines in the host platform (XH) section in 
	 * the javacore
	 */
	protected void initTagAttributeRules() {

		addSectionName();
		addSectionDetails();
	}

	protected void addSectionName() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_NAME, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1STSEGTYPE, lineRule);
	}

	protected void addSectionDetails() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				if (addPrefixedHexToken(MEMORY_SEGMENT_ID) == null) {
					addNonPrefixedHexToken(MEMORY_SEGMENT_ID);
					addNonPrefixedHexToken(MEMORY_SEGMENT_HEAD);
					addNonPrefixedHexToken(MEMORY_SEGMENT_FREE);
					addNonPrefixedHexToken(MEMORY_SEGMENT_TAIL);
					addNonPrefixedHexToken(MEMORY_SEGMENT_TYPE);
					addNonPrefixedHexToken(MEMORY_SEGMENT_SIZE);
				} else {
					addPrefixedHexToken(MEMORY_SEGMENT_HEAD);
					addPrefixedHexToken(MEMORY_SEGMENT_FREE);
					addPrefixedHexToken(MEMORY_SEGMENT_TAIL);
					addPrefixedHexToken(MEMORY_SEGMENT_TYPE);
					addPrefixedHexToken(MEMORY_SEGMENT_SIZE);
				};
			}
		};
		addTag(T_1STSEGMENT, lineRule);
	}
}

