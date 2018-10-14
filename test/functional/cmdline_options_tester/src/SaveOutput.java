/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

/**
 * Saveoutput matches and stores desired output from test suite in a variable
 * 
 * Example:
 * 			<saveoutput regex="no" type="success" saveName="moduleAddr" splitIndex="1" splitBy="!j9module ">!j9module 0x</saveoutput>
 * 			This matches output containing "!j9module 0x" and split the line by "!j9module " then take the second substring (Indexed "1").
 */
class SaveOutput extends Output {
	private String _saveName;
	private String _splitIndex;
	private String _splitBy;
	
	/**
	 * Pass the arguments to local variables.
	 * @param saveName Name of the variable the output is put into
	 * @param splitIndex Index of the substring after split
	 * @param splitBy The string that the matching line is split by 
	 */
	public SaveOutput( String matchRegex, String matchJavaUtilPattern, String showRegexMatch, String matchCase, String type, String saveName, String splitIndex, String splitBy ) {
		super(matchRegex, matchJavaUtilPattern, showRegexMatch, matchCase, type);
		_saveName = saveName;
		_splitIndex = splitIndex;
		_splitBy = splitBy;
	}
	
	@Override
	boolean match( Object o ) {
		boolean result = super.match(o);
		if (result && (getType() == TestCondition.REQUIRED || getType() == TestCondition.SUCCESS)) {
			String line = (String) o;
			if (_splitBy == null || _splitBy.equals("")) {
				System.err.println("String split failure: splitBy cannot be empty");
				result = false;
			} else if (!line.contains(_splitBy)) {
				System.err.printf(
						"String split failure: String does not contain splitBy %n" + "String: %s%n" + "splitBy: %s%n",
						line, _splitBy);
				result = false;
			} else {
				String[] splitArray = line.split(_splitBy);
				int arraySize = splitArray.length;
				try {
					int splitIndexInt = Integer.parseInt(_splitIndex);
					if (splitIndexInt >= arraySize || splitIndexInt < 0) {
						System.err.printf("String split failure: splitIndex out of bound %n"
								+ "Size of splitArray: %s%n" + "splitIndex: %d%n", arraySize, splitIndexInt);
						result = false;
					} else {
						TestSuite.putVariable(_saveName, splitArray[splitIndexInt]);
					}
				} catch (NumberFormatException e) {
					System.err.println("String split failure: splitIndex is not a number");
					result = false;
				}
			}
		}
		return result;
	}

}

