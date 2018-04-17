/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
package Utilities;

/**
 * @author Matthew Kilner
 */
public class StringManipulator {

	public String getStringElement(int elementIndex, String string){
		
		char seperator = ',';
		int currentElement = -1;
		int startIndex = 0;
		int endIndex = 0;
		int currentIndex = 0;
		String element = "";
		
		while(currentElement != elementIndex){
			startIndex = currentIndex;
			endIndex = string.indexOf(seperator, currentIndex);
			if(endIndex == -1){
				element = string.substring(currentIndex);
			} else {
				currentIndex = endIndex + 1;
				element = string.substring(startIndex, endIndex);
			}
			currentElement += 1;
		}				
		return element;
	}
	
	public static String extractDummyNameSuffix(String string)
	{
		char seperator = '/';
		int startIndex = 0;
		String suffix = "";
		
		int lastIndex = string.lastIndexOf(seperator);
		for(int currentIndex = 0; currentIndex < lastIndex; currentIndex = string.indexOf(seperator, startIndex)){
			startIndex = currentIndex + 1;
		}
		suffix = string.substring(startIndex, lastIndex);
			
		return suffix.length() == 0 ? null : suffix;
	}
	
	public static String extractJarDummyNameSuffix(String string)
	{
		char seperator = '/';
		char pling = '!';
		int startIndex = 0;
		String suffix = "";
		
		int plingIndex = string.lastIndexOf(pling);
		String tempString = string.substring(startIndex, plingIndex);
		int lastIndex = tempString.lastIndexOf(seperator);
		for(int currentIndex = 0; currentIndex < lastIndex; currentIndex = tempString.indexOf(seperator, startIndex)){
			startIndex = currentIndex + 1;
		}
		suffix = tempString.substring(startIndex, lastIndex);
			
		return suffix.length() == 0 ? null : suffix;
	}
	
}
