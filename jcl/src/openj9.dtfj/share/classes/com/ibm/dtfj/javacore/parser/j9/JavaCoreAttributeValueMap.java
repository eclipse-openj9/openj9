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

import java.util.Map;

import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;

public class JavaCoreAttributeValueMap implements IAttributeValueMap {
	
	
	private Map fResults;
	
	
	public JavaCoreAttributeValueMap(Map results) {
		if (results == null) {
			throw new NullPointerException("Must pass non-null results");
		}
		fResults = results;
	}

	
	/*
	 * DATA EXTRACTION
	 */
	
	
	
	/**
	 * 
	 * @param results
	 * @param tokenType
	 * 
	 */
	public String getTokenValue(String tokenType) {
		IParserToken token = getToken(tokenType);
		String value = null;
		if (token != null) {
			value = token.getValue();
		}
		return value;
	}
	
	
	/**
	 * 
	 * @param tokenType
	 * 
	 */
	public long getLongValue(String tokenType) {
		IParserToken token = getToken(tokenType);
		long value = IBuilderData.NOT_AVAILABLE;
		if (token != null) {
			String s = token.getValue();
			String s2 = s;
			if (s.length() >= 18) {
				// Avoid overflow with hex values with MSB set
				// Match sign Hex-indicator leading-zeroes high-Hex 15-hex-digits end-of-line
				// Do a textual subtraction, then add back after conversion
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)8(\\p{XDigit}{15})$", "$10$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)9(\\p{XDigit}{15})$", "$11$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[aA](\\p{XDigit}{15})$", "$12$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[bB](\\p{XDigit}{15})$", "$13$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[cC](\\p{XDigit}{15})$", "$14$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[dD](\\p{XDigit}{15})$", "$15$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[eE](\\p{XDigit}{15})$", "$16$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?(?:#|0[xX])0*)[fF](\\p{XDigit}{15})$", "$17$2");
				if (s.equals(s2)) s2 = s.replaceFirst("^(-?010*)([01234567]{21})$", "$10$2");
			}
			if (s.equals(s2)) {
				value = Long.decode(s).longValue();
			} else {
				value = Long.decode(s2).longValue() + Long.MIN_VALUE;
			}
		}
		return value;
	}
	
	
	
	public int getIntValue(String tokenType) {
		IParserToken token = getToken(tokenType);
		int value = IBuilderData.NOT_AVAILABLE;
		if (token != null) {
			value = Integer.decode(token.getValue()).intValue();
		}
		return value;
	}

	
	/**
	 * 
	 * @param tokenType
	 * 
	 */
	public IParserToken getToken(String tokenType) {
		Object oToken = fResults.get(tokenType);
		if (oToken instanceof IParserToken) {
			return (IParserToken) oToken;
		}
		return null;
	}

}
