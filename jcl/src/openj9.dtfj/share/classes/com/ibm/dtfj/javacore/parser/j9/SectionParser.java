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

import java.util.Properties;

import com.ibm.dtfj.javacore.builder.IImageBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ILookAheadBuffer;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

public abstract class SectionParser  extends SectionParserGrammar implements ICommonTypes {

	protected IImageBuilder fImageBuilder;

	/**
	 * Depth of at least 3 is needed for J9 javacores,
	 * as parsing a section from the start will
	 * yield three tokens:
	 * SECTION, header information token, first TAG of section
	 * (note that NULL lines are ignored by the scanner). 
	 * Adjust as necessary for deeper analysis.
	 */
	protected static final int LOOKAHEAD_DEPTH = 3;

	public SectionParser(String sectionName) {
		super(sectionName);
		fImageBuilder = null;
	}
		
	/**
	 * 
	 * @param lookahead from where to read tokens
	 * @param tag information containing current tag information at the time of the parse 
	 * @throws NullPointerException if a null lookaheadbuffer is passed.
	 */
	public void readIntoDTFJ(ILookAheadBuffer lookAheadBuffer, IImageBuilder imageBuilder) throws ParserException {
		if ((fImageBuilder = imageBuilder) == null) {
			throw new ParserException("Image builder must not be null");
		}
		setTagManager(J9TagManager.getCurrent());
		setLookAheadBuffer(lookAheadBuffer, LOOKAHEAD_DEPTH);
		topLevelRule();
	}
	
	
	/**
	 * 
	 * @param properties
	 * @param key
	 * @param value
	 * 
	 */
	public boolean addAsProperty(Properties properties, String key, String value) {
		boolean added = false;
		if (added = (key != null && value != null)) {
			properties.setProperty(key, value);
		}
		return added;
	}

	
	/**
	 * Must be implemented by subclass.
	 * @param lookAheadBuffer
	 * @throws ParserException
	 */
	abstract protected void topLevelRule() throws ParserException;

	/**
	 * 
	 * @param startingTag
	 * @throws ParserException
	 */
	abstract protected void sovOnlyRules(String startingTag) throws ParserException;
}
