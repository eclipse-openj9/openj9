/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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

import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;

/**
 * Parses a string source pertaining to a tag line from a javacore, and returns
 * a map of tokens, where the token type (java.lang.String) is the key and token value (java.lang.String)is the value.
 * 
 * The Line Rule is generally referenced internally by the framework, so the subtype implementers don't
 * need to worry about where a line rule is being used. All that needs to be done is implementing
 * the logic of how to parse a string, and how to generate the tokens.
 *
 */
public interface ILineRule {
	/**
	 * 
	 * @param source to parse
	 * @param lineNumber on disk of the source line
	 * @param startingOffset of the line on disk.
	 * @return map of the tokens where token type is the key (java.lang.String), and token value the java.lang.String value.
	 */
	public IAttributeValueMap parseLine(String source, int lineNumber, int startingOffset);
}
