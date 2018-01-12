/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.nativememory;

import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class NativeMemoryTagParser extends TagParser implements INativeMemoryTypes
{

	public NativeMemoryTagParser()
	{
		super(NATIVEMEM_SECTION);
	}

	/**
	 * Line rule to cope with all *MEMUSER lines.
	 * 
	 * Unusual in that it adds a pseudo depth token
	 * @author andhall
	 */
	private static class NativeMemoryUserLineRule extends LineRule
	{
		private final int depth;
		
		NativeMemoryUserLineRule(int depth)
		{
			this.depth = depth;
		}
		
		//Line format: 3MEMUSER       |  +--Classes: 2,468,456 bytes / 69 allocations
		protected void processLine(String source, int startingOffset)
		{
			/* Add pseudo-token containing the depth */
			addToken(A_DEPTH, Integer.toString(depth));
			
			/* Strip off any | | |*/
			final boolean foundPipes = consumeUntilFirstMatch(NativeMemoryPatternMatchers.pipes);
			
			final boolean foundCrossMinusMinus = consumeUntilFirstMatch(NativeMemoryPatternMatchers.crossminusminus);
			
			//Only worth looking on if either this is a level 1 
			//memuser with no foundPipes, or foundCrossMinusMinus is set
			final boolean level1WithData = depth == 1 && foundPipes;
			
			if (level1WithData || foundCrossMinusMinus) {
				addToken(A_NAME, NativeMemoryPatternMatchers.categoryName);
				
				consumeUntilFirstMatch(CommonPatternMatchers.colon);
				
				addToken(A_DEEP_BYTES, NativeMemoryPatternMatchers.commaDelimitedNumeric);
				
				consumeUntilFirstMatch(NativeMemoryPatternMatchers.bytesAndSeparator);
				
				addToken(A_DEEP_ALLOCATIONS, NativeMemoryPatternMatchers.commaDelimitedNumeric);
			}

		}
		
	}
	
	protected void initTagAttributeRules()
	{
		addTag(T_0MEMUSER, null);
		for (int i = 1; i < T_MEMUSERS.length; i++) {
			addTag(T_MEMUSERS[i], new NativeMemoryUserLineRule(i));
		}
	}

	
}
