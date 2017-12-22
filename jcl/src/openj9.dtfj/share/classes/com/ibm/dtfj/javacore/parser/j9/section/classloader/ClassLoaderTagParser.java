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
package com.ibm.dtfj.javacore.parser.j9.section.classloader;

import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.TagParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class ClassLoaderTagParser extends TagParser implements IClassLoaderTypes {

	public ClassLoaderTagParser() {
		super(CLASSLOADER_SECTION);
	}
	
	protected void initTagAttributeRules() {
		addTag(T_1CLTEXTCLLOS, null);
		addTag(T_1CLTEXTCLLSS, null);
		addTextCLLoader();
		addNumberLoadedLib();
		addNumberLoadedClasses();
		addTag(T_1CLTEXTCLLIB, null);
		addTextCLLib2();
		addTextLib();
		addTag(T_1CLTEXTCLLOD, null);
		addTextCLLoad();
		addTextClass();
	}
	


	/**
	 * 
	 *
	 */
	private void addTextCLLoader() {
		ILineRule lineRule = new ClassLoaderLineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addAllCharactersAsTokenUntilFirstMatch(CL_ATT_ACCESS_PERMISSIONS, CommonPatternMatchers.whitespace);
				matchLoaderAndAddAttNameAndAddress(ClassLoaderPatternMatchers.system, CommonPatternMatchers.java_absolute_name, CL_ATT__NAME);
				addPrefixedHexToken(CL_ATT_ADDRESS);
				if (consumeUntilFirstMatch(ClassLoaderPatternMatchers.shadow)) {
					addPrefixedHexToken(CL_ATT_SHADOW_ADDRESS);
					// Sov VMs have the parent loader address as the shadow address, so don't add them
					// as regular loaders because the address will be wrong.
				} else if (consumeUntilFirstMatch(ClassLoaderPatternMatchers.parent)) {
					addAttributeNameAndAddress(ClassLoaderPatternMatchers.none, CommonPatternMatchers.java_absolute_name, CL_ATT_PARENT_NAME);
					addPrefixedHexToken(CL_ATT_PARENT_ADDRESS);
				}
			}
		};
		addTag(T_2CLTEXTCLLOADER, lineRule);
	}
	
	
	
	/**
	 * 
	 *
	 */
	private void addNumberLoadedLib() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				addToken(CL_ATT_NMBR__LIB, CommonPatternMatchers.dec);
			}
		};
		addTag(T_3CLNMBRLOADEDLIB, lineRule);
	}
	
	
	
	/**
	 * 
	 *
	 */
	private void addNumberLoadedClasses() {
		ILineRule lineRule = new LineRule() {
			public void processLine(String source, int startingOffset) {
				addToken(CL_ATT_NMBR_LOADED_CL, CommonPatternMatchers.dec);
			}
		};
		addTag(T_3CLNMBRLOADEDCL, lineRule);
	}
	
	
	
	/**
	 * 
	 *
	 */
	private void addTextCLLib2() {
		ILineRule lineRule = new ClassLoaderLineRule() {
			public void processLine(String source, int startingOffset) {
				matchLoaderAndAddAttNameAndAddress(ClassLoaderPatternMatchers.system, CommonPatternMatchers.java_absolute_name, CL_ATT__NAME);
				addPrefixedHexToken(CL_ATT_ADDRESS);
			}
		};
		addTag(T_2CLTEXTCLLIB, lineRule);
	}
	
	
	
	
	/**
	 * 
	 *
	 */
	private void addTextLib() {
		ILineRule lineRule = new ClassLoaderLineRule() {
			public void processLine(String source, int startingOffset) {
				consumeUntilFirstMatch(CommonPatternMatchers.whitespace);
				addToken(CL_ATT_LIB_NAME, CommonPatternMatchers.allButLineFeed);
			}
		};
		addTag(T_3CLTEXTLIB, lineRule);
	}
	
	
	
	/**
	 * 
	 *
	 */
	private void addTextCLLoad() {
		ILineRule lineRule = new ClassLoaderLineRule() {
			public void processLine(String source, int startingOffset) {
				matchLoaderAndAddAttNameAndAddress(ClassLoaderPatternMatchers.system, CommonPatternMatchers.java_absolute_name, CL_ATT__NAME);
				addPrefixedHexToken(CL_ATT_ADDRESS);
			}
		};
		addTag(T_2CLTEXTCLLOAD, lineRule);
	}
	
	
	
	/**
	 * 
	 *
	 */
	private void addTextClass() {
		ILineRule lineRule = new ClassLoaderLineRule() {
			public void processLine(String source, int startingOffset) {
				addAttributeNameAndAddress(ClassLoaderPatternMatchers.locked, CommonPatternMatchers.java_absolute_name_array, CLASS_ATT_NAME);
				addPrefixedHexToken(CLASS_ATT_ADDRESS);
			}
		};
		addTag(T_3CLTEXTCLASS, lineRule);
	}
}
