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
package com.ibm.dtfj.javacore.parser.framework.tag;

import java.util.HashMap;
import java.util.Iterator;


public abstract class TagParser implements ITagParser {
	
	private HashMap fTagRuleMap;
	private String fSectionName;
	
	public TagParser(String sectionName) {
		fSectionName = sectionName;
		fTagRuleMap = new HashMap();
		initTagAttributeRules();
	}

	/**
	 * Must be implemented by the caller. Should not be called, just implemented.
	 * <br>
	 * Typically contains a series of {@link #addTag(String, ILineRule)} calls.
	 * Parent class knows where to call this method.
	 * <br>
	 * <br>
	 *  General guidelines:
	 * <br>
	 * <br>
	 * <b>For tags that contain attributes with meaningful values:</b>
	 * <br>
	 * Specify line rules with patterns that are of interest:
	 * <br>
	 * 1. patterns that precedes a value (e.g., a ":", "=", etc..);
	 * <br>
	 * 2. patterns that match a value;
	 * 
	 * <br>
	 * If an attribute value in a line rule pertaining to some tag has more than one possible pattern 
	 * (e.g., "info" = (Class.java:23)  or "info" = (Native Method) ), list
	 * all of the patterns for that value in its corresponding line rule. If the first doesn't match, nothing happens, so the line rule 
	 * will check the next pattern until a match occurs. 
	 * <br>
	 * Any other pattern occurring in the source string will be ignored.
	 * 
	 * <br>
	 * <br>
	 * <br>
	 * <b>For tags with no attributes, or meaningful values:</b>
	 * <br> 
	 * Specify null for line rules.
	 *
	 */
	abstract protected void initTagAttributeRules();
	
	/**
	 * If a Line rule is missing a pattern list, an exception will be thrown
	 * indicating so, and the line rule will not be added.
	 * 
	 * @param tag
	 * @param rule
	 */
	protected void addTag(String tag, ILineRule lineRule) {
		if (tag == null) {
			return;
		}
		fTagRuleMap.put(tag, lineRule);
	}
	
	
	/**
	 * 
	 */
	public ILineRule getLineRule(String tag) {
		return (ILineRule) fTagRuleMap.get(tag);
	}

	
	/**
	 * 
	 */
	public String getSectionName() {
		return fSectionName;
	}
	
	/**
	 * 
	 */
	public boolean hasTag(String tag) {
		return fTagRuleMap.containsKey(tag);
	}
	
	
	/**
	 * 
	 */
	public Iterator getTags() {
		return fTagRuleMap.keySet().iterator();
	}
	

}
