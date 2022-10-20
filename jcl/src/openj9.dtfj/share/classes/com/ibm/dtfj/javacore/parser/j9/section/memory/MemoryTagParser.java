/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2007
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

/**
 * Parser rules for lines in the thread history section in the javacore.
 */
public class MemoryTagParser extends TagParser implements IMemoryTypes {

	public MemoryTagParser() {
		super(MEMORY_SECTION);
	}

	/**
	 * Initialize parser with rules for lines in the host platform (XH) section in
	 * the javacore.
	 */
	@Override
	protected void initTagAttributeRules() {

		addSectionName();
		addHeapSpace();
		addHeapDetails();
		addSectionDetails();
	}

	protected void addSectionName() {
		ILineRule lineRule = new LineRule() {
			@Override
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_NAME, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1STHEAPTYPE, lineRule);
		addTag(T_1STSEGTYPE, lineRule);
	}

	protected void addSectionDetails() {
		ILineRule lineRule = new LineRule() {
			@Override
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

	protected void addHeapSpace() {
		// NULL           id                 start              end                size               space/region
		// 1STHEAPSPACE   0x0000019EEF4B3E40         --                 --                 --         Generational
		// 1STHEAPREGION  0x0000019EEF53FF60 0x00000007224F0000 0x0000000722E70000 0x0000000000980000 Generational/Tenured Region
		ILineRule lineRule = new LineRule() {
			@Override
			public void processLine(String source, int startingOffset) {
				addPrefixedHexToken(MEMORY_SEGMENT_ID);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_HEAD, CommonPatternMatchers.non_whitespace_attribute);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_TAIL, CommonPatternMatchers.non_whitespace_attribute);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_SIZE, CommonPatternMatchers.non_whitespace_attribute);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_TYPE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1STHEAPSPACE, lineRule);
	}

	protected void addHeapDetails() {
		// NULL           id                 start              end                size               space/region
		// 1STHEAPSPACE   0x0000019EEF4B3E40         --                 --                 --         Generational
		// 1STHEAPREGION  0x0000019EEF53FF60 0x00000007224F0000 0x0000000722E70000 0x0000000000980000 Generational/Tenured Region
		ILineRule lineRule = new LineRule() {
			@Override
			public void processLine(String source, int startingOffset) {
				addPrefixedHexToken(MEMORY_SEGMENT_ID);
				addPrefixedHexToken(MEMORY_SEGMENT_HEAD);
				addPrefixedHexToken(MEMORY_SEGMENT_TAIL);
				addPrefixedHexToken(MEMORY_SEGMENT_SIZE);
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(MEMORY_SEGMENT_TYPE, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_1STHEAPREGION, lineRule);
	}
}

