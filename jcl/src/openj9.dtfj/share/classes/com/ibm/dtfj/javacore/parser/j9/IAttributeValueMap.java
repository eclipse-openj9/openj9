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

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;


/**
 * Contains a map of values parsed from a tag line. It is assumed that each attribute type occurring in
 *  a tag line occurs at most once (e.g., it's not assumed that there are two "tid" attributes in a thread info tag).
 *  Token values are Strings that get mapped to the correct type via specific methods.
 *
 */
public interface IAttributeValueMap {
	/**
	 * 
	 * @param results
	 * @param tokenType
	 * 
	 */
	public String getTokenValue(String tokenType);
	
	/**
	 * 
	 * @param tokenType
	 * 
	 */
	public long getLongValue(String tokenType);
	
	/**
	 * 
	 * @param tokenType
	 * 
	 */
	public int getIntValue(String tokenType);

	/**
	 * 
	 * @param tokenType
	 * 
	 */
	public IParserToken getToken(String tokenType);
}
