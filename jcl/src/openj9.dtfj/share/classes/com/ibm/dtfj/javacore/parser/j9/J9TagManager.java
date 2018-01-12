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
package com.ibm.dtfj.javacore.parser.j9;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import com.ibm.dtfj.javacore.parser.framework.tag.ITagManager;
import com.ibm.dtfj.javacore.parser.framework.tag.ITagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

public class J9TagManager implements ITagManager {
	private HashMap fAllTags;
	private HashMap fTagParsers;
	private static J9TagManager fTagManager;
	public static final String CHECK_ALL = "check_all";
	
	private String fCommentType = ICommonTypes.NULL;
	
	public static J9TagManager getCurrent() {
		if (fTagManager == null) {
			fTagManager = new J9TagManager();
		}
		return fTagManager;
	}
	
	
	public J9TagManager()	 {
		fAllTags = new HashMap();
		fTagParsers = new HashMap();
	}
	
	
	
	
	/**
	 * Hardcoded for now.
	 *
	 */
	public void loadTagParsers(ArrayList tagParsers) {
		if (tagParsers == null) {
			return;
		}
		fAllTags.clear();
		fTagParsers.clear();
		for (Iterator it = tagParsers.iterator(); it.hasNext();){
			ITagParser parser = (ITagParser) it.next();
			if (parser != null) {
				fTagParsers.put(parser.getSectionName(), parser);
			}
		}
		fillAllTags();
	}

	
	/**
	 * 
	 *
	 */
	private void fillAllTags() {
		for (Iterator it = fTagParsers.values().iterator(); it.hasNext();) {
			ITagParser parser = (ITagParser) it.next();
			for (Iterator itTags = parser.getTags(); itTags.hasNext();) {
				String tag = (String)itTags.next();
				if (tag != null) {
					fAllTags.put(tag, tag);
				}
			}
		}
	}

	
	/**
	 * 
	 * @param tag to check whether it exists in the Tag Manager.
	 * @return true if the Tag Manager has this tag registered.
	 */
	public boolean hasTag(String identifier) {
		return fAllTags.containsKey(identifier);
	}

	
	
	public ITagParser getTagParser(String section) {
		return (ITagParser)fTagParsers.get(section);
	}
	
	
	/**
	 * 
	 * @param tag
	 * @param section
	 * 
	 */
	public boolean isTagInSection(String tag, String section) {
		boolean result = false;
		if (section.equals(CHECK_ALL)) {
			result = fAllTags.containsKey(tag);
		}
		else {
			ITagParser parser = (ITagParser) fTagParsers.get(section);
			if (parser != null) {
				result = parser.hasTag(tag);
			}
		}
		return result;
	}
	
	

	



	/**
	 * 
	 * @param source
	 * 
	 */
	public boolean isComment(CharSequence source) {
		boolean result = false;
		int length = 0;
		if (source != null && ((length = fCommentType.length()) == source.length())) {
			for (int i = 0; i < length && (result = (fCommentType.charAt(i)==source.charAt(i))); i++);
		}
		return result;
	}
}
