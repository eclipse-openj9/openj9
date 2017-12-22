/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.j9.dump.indexsupport;

import org.xml.sax.Attributes;

/**
 * @author jmdisher
 *
 */
public abstract class NodeAbstract implements IParserNode
{
	/**
	 * Helper method for parsing strings into numbers.
	 * Strips the 0x from strings, if they exist.  Also handles the case of 16 character numbers which would be greater than MAX_LONG
	 * @param value The string representing the number
	 * @param defaultValue The value which will be returned if value is null
	 * @return defaultValue if the value is null or the long value of the given string
	 */
	public static long _longFromString(String value, long defaultValue)
	{
		long translated = defaultValue;
		int radix = 10;
		
		if (null != value) {
			if (value.startsWith("0x")) {
				value = value.substring(2);
				radix = 16;
				if (16 == value.length()) {
					//split and shift since this would overflow
					String highS = value.substring(0, 8);
					String lowS = value.substring(8, 16);
					long high = Long.parseLong(highS, radix);
					long low = Long.parseLong(lowS, radix);
					translated = (high << 32) | low;
				} else {
					translated = Long.parseLong(value, radix);
				}
			} else {
				translated = Long.parseLong(value, radix);
			}
		}
		return translated;
	}
	
	
	/**
	 * Helper method for parsing strings into numbers.
	 * Strips the 0x from strings, if they exist.  Also handles the case of 16 character numbers which would be greater than MAX_LONG
	 * @param value The string representing the number
	 * @return 0 if the value is null or the long value of the given string
	 */
	public static long _longFromString(String value)
	{
		return _longFromString(value, 0);
	}
	
	/**
	 * Looks up the given key in rawText and finds the long that corresponds to it.  ie: "... key=number..."
	 * Returns 0 if the key is not found.
	 * 
	 * @param rawText
	 * @param key
	 * @return
	 */
	public static long _longByResolvingRawKey(String rawText, String key)
	{
		long value = 0;
		int index = rawText.indexOf(key);
		
		while ((index > 0) && (!Character.isWhitespace(rawText.charAt(index-1)))) {
			index = rawText.indexOf(key, index+1);
		}
		
		if (index >= 0) {
			int equalSign = rawText.indexOf("=", index);
			
			if (equalSign > index) {
				int exclusiveEnd = equalSign +1;
				
				while ((exclusiveEnd < rawText.length()) && (!Character.isWhitespace(rawText.charAt(exclusiveEnd)))) {
					exclusiveEnd++;
				}
				//the value on the right of the equal sign is always supposed to be hexadecimal
				String number = "0x" + rawText.substring(equalSign+1, exclusiveEnd);
				value = _longFromString(number);
			}
		}
		return value;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		//default implementation only claims an error
		return unexpectedNode(qName, attributes);
	}
	
	private IParserNode unexpectedNode(String qName, Attributes attributes)
	{
		IParserNode node = null;
		if (qName.equals("error")) {
			node = NodeError.errorTag(attributes);
		} else {
			node = NodeUnexpectedTag.unexpectedTag(qName, attributes);
		}
		return node;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#stringWasParsed(java.lang.String)
	 */
	public void stringWasParsed(String string)
	{
		//since most implementations do nothing, this is provided here to remove the pointlessly redundant empty method from every sub-class
	}
	
	public void didFinishParsing()
	{
		//this can be implemented to do any cleanup or to write-back state to persistent DTFJ runtime objects.  Most nodes don't use it
	}
}
