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

import java.util.Stack;

import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

public class NativeMemorySectionParser extends SectionParser implements INativeMemoryTypes
{
	
	public NativeMemorySectionParser()
	{
		super(NATIVEMEM_SECTION);
	}

	protected void sovOnlyRules(String startingTag) throws ParserException
	{
		//Do nothing
	}

	protected void topLevelRule() throws ParserException
	{
		IImageProcessBuilder fImageProcessBuilder = fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder();
		IJavaRuntimeBuilder fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		
		IAttributeValueMap results = null;
		Stack categoryStack = new Stack();
		
		processTagLineOptional(T_0MEMUSER);
		
		while ( (results = processMemUserLine() ) != null ) {
			String name = results.getTokenValue(A_NAME);
			
			/* If no name available, this is a spacing line */
			if (name == null) {
				continue;
			}
			
			int depth = results.getIntValue(A_DEPTH);
			
			while (categoryStack.size() >= depth) {
				categoryStack.pop();
			}
			
			long deepBytes = parseCommaDelimitedLong(results.getTokenValue(A_DEEP_BYTES));
			long deepAllocations = parseCommaDelimitedLong(results.getTokenValue(A_DEEP_ALLOCATIONS));
			
			JavaRuntimeMemoryCategory parent = null;
			
			if (categoryStack.size() > 0) {
				parent = (JavaRuntimeMemoryCategory) categoryStack.peek();
			}
			
			if (name.equals(OTHER_CATEGORY)) {
				//"Other" categories are special - they contain the shallow values for the parent.
				
				if (parent == null) {
					throw new ParserException("Parse error: Unexpected NULL parent category for \"Other\" memory category");
				}
				
				fRuntimeBuilder.setShallowCountersForCategory(parent, deepBytes, deepAllocations);
			} else {
				JavaRuntimeMemoryCategory category = fRuntimeBuilder.addMemoryCategory(name, deepBytes, deepAllocations, parent);
				
				categoryStack.push(category);
			}
		}
	}
	
	private long parseCommaDelimitedLong(String tokenValue)
	{
		return Long.parseLong(tokenValue.replaceAll(",", ""));
	}

	/**
	 * The XMEMUSER tag can occur in almost any order. processMemUserLine
	 * looks for any of the XMEMUSER tags
	 * @return
	 * @throws ParserException 
	 */
	private IAttributeValueMap processMemUserLine() throws ParserException
	{
		for (int i=0; i < T_MEMUSERS.length; i++) {
			String tag = T_MEMUSERS[i];
			
			IAttributeValueMap results = processTagLineOptional(tag);
			
			if (results != null) {
				return results;
			}
		}
		
		return null;
	}


}
