/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;

public class OutputFile {

	BufferedReader in;
	String line;
	String firstReference;
	String secondReference;

	public OutputFile(String fileName) throws IOException {
		in = new BufferedReader(new FileReader(fileName));
		firstReference = null;
	}
	
	/**
	 * Yes, finalizers may not have a very good reputation but it can't do any harm
	 */
	protected void finalize() throws Throwable {
	     try {
	         in.close(); 
	     } finally {
	         super.finalize();
	     }
	 }


	/**
	 * Read through the file looking for the given string. Keep going to the end of the file if necessary.
	 * If the string is found, return true, set the instance variable line, and keep that position in the file.
	 * If the string is not found, return false, leave the file positioned at the end.

	 * @param lookFor the string to look for
	 * @return true or false according to whether a line with the string was found
	 * @throws IOException
	 */
	public boolean skipUnlimitedToLineContaining(String lookFor) throws IOException {
		line = in.readLine();
		while (line != null) {
			if (line.indexOf(lookFor) != -1) {
				System.err.printf("Successfully found the following line containing \"%s\"\n", lookFor);
				System.err.printf(line);
				System.err.printf("\n");
				return true;
			}
			line = in.readLine();
		}
		System.err.printf("***Reached end of file before finding a line containing \"%s\"\n", lookFor);
		return false;
	}
	
	/**
	 * Read through the file looking for the given string. Keep going to the end of the file if necessary.
	 * If the string is found AT THE END OF THE LINE, return true, set the instance variable line, and keep that position in the file.
	 * If the string is not found, return false, leave the file positioned at the end.
	 * 
	 * This method is especially for searching for the line ending "OBJ com/ibm/dump/tests/types/packed/NestedPacked" in the 
	 * classic heapdump. Just using skipUnlimitedToLineContaining finds the lines for NestedPacked1 and NestedPacked2

	 * @param lookFor the string to look for
	 * @return true or false according to whether a line with the string was found
	 * @throws IOException
	 */
	public boolean skipUnlimitedToLineEnding(String lookFor) throws IOException {
		line = in.readLine();
		while (line != null) {
			if (line.endsWith(lookFor)) {
				System.err.printf("Successfully found the following line ending \"%s\"\n", lookFor);
				System.err.printf(line);
				System.err.printf("\n");
				return true;
			}
			line = in.readLine();
		}
		System.err.printf("***Reached end of file before finding a line ending \"%s\"\n", lookFor);
		return false;
	}

	/**
	 * Read no more than maxLinesToSkip down the file looking for a line containing the given string
	 * Read some number of lines down the file looking for the given string. Read no more than maxLinesToSkip lines. 
	 * If the string is found, hold the line in instance variable line and keep the position in the file.
	 * If the string is not found, return false and restore the position in the file to where it was on entry:
	 * by doing so one failing test does not damage the tests that follow. 
	 * @param maxLinesToSkip skip no more lines than this
	 * @param lookFor the string to look for
	 * @return true or false according to whether a line with the string was found
	 * @throws IOException
	 */
	public boolean skipLimitedToLineContaining(int maxLinesToSkip, String lookFor) throws IOException {
		int linesRead = 0;
		in.mark(maxLinesToSkip * 200); // generous allocation to allow reset to work. Most lines are < 100 bytes long
		line = in.readLine();
		while (line != null && linesRead < maxLinesToSkip) {
			//				System.err.printf("line read was %s\n", line);
			if (line.indexOf(lookFor) != -1) {
				System.err.printf("Successfully found a line containing \"%s\"\n", lookFor);
				return true;
			}
			line = in.readLine();
			linesRead++;
		}
		System.err.printf("***Failed to find a line containing \"%s\"\n", lookFor);
		in.reset();
		return false;
	}


	/**
	 * Examine the last line read and check that the word at the given position is equal to given string
	 * @param lookFor the string to look for
	 * @param whichWord the index of the word to check
	 * @return
	 */
	public boolean linePassesCheckForStringAtWord(String lookFor, int whichWord) {
		if (line == null) {
			// previous call to skip[Un]limitedToLineContaining has failed
			// so appropriate error message has been emitted already
			// just return false and allow test to contine. 
			return false;
		}
		return StringUtils.linePassesCheckForStringAtWord(line, whichWord, lookFor);
	}

	/**
	 * Return true or false depending on whether the word at position whichWord is in the list
	 * of values in validValues
	 * 
	 * @param validValues
	 * @param whichWord
	 * @return
	 */
	public boolean linePassesCheckForStringsAtWord(String[] validValues, int whichWord) {
		if (line == null) {
			// previous call to skip[Un]limitedToLineContaining has failed
			// so appropriate error message has been emitted already
			// just return false and allow test to contine. 
			return false;
		}
		return StringUtils.linePassesCheckForStringsAtWord(line, whichWord, validValues);
	}

	/**
	 * 
	 * @return the first reference that was found by the previous call to skipToNextLineAndCheckReferencesCount.
	 * This is just the first token that was on that line. 
	 */
	public String getFirstReference() {
		return firstReference;
	}

	/**
	 * 
	 * @return the second reference that was found by the previous call to skipToNextLineAndCheckReferencesCount.
	 * This is just the second token that was on that line. 
	 */
	public String getSecondReference() {
		return secondReference;
	}
	
	/**
	 * Skip down to the lines containing "references:" then check the number of references on the 
	 * succeeding line.  
	 * @param maxLinesToSkip
	 * @param expectedNumberOfReferences
	 * @return true or false depending on whether the number of references matches the expected number.
	 * @throws IOException
	 */
	public boolean skipToLineContainingListOfReferencesAndCheckCount(int maxLinesToSkip, int expectedNumberOfReferences) throws IOException {
		
		if (! skipLimitedToLineContaining(maxLinesToSkip, "references:")) {
			// if we cannot find the references line, don't go further
			return false;
		}
		return skipToNextLineAndCheckReferencesCount(expectedNumberOfReferences);

	}

	/**
	 * Skip down the output from jdmpview's x/j command looking for the line containing "references:"
	 * @param maxLinesToSkip
	 * @return
	 * @throws IOException
	 */
	public boolean skipLimitedToReferencesLine(int maxLinesToSkip) throws IOException {
		if (! skipLimitedToLineContaining(maxLinesToSkip, "references:")) {
			// if we cannot find the references line, don't go further
			return false;
		}
		return true;
	}
	
	/**
	 * Read the next line, tokenise it, and check that the number of tokens equals the given parameter.
	 * As a side effect, set the instance variable firstReference.
	 * 
	 * This method can be called either from the classic heapdump checkers, when the file will be already 
	 * positioned on the OBJ line and the references always come on the next line, 
	 * or called from skipToLineContainingListOfReferencesAndCheckCount() when checking the 
	 * output from jdmpview's x/j command. In either case, the list of references is on the very next line. 
	 * 
	 * 
	 * 
	 * @param expectedNumberOfReferences
	 * @return true or false depending on whether the line does or does not contain the given number of tokens.
	 * @throws IOException
	 */
	public boolean skipToNextLineAndCheckReferencesCount(int expectedNumberOfReferences) throws IOException {
		// assert
		// if we are called by one of the classic heapdump checkers we are currently on the OBJ line and 
		// skipping to the next line will put us on the list of references, or
		// we will have been called from skipToReferencesLineAndCheckCount if we are checking the output
		// from jdmpview's x/j command during phd checking
		
		
		String line = in.readLine();
		if (line == null) {
			System.err.println("***Unable to check count of references - previous call to find the references lines has failed.");
			return false;
		}

		ArrayList<String> references = StringUtils.extractTokensFromLine(line);
		if (references.size() > 0) {
			firstReference = references.get(0);
		} else {
			firstReference = null;
		}
		if (references.size() > 1) {
			secondReference = references.get(1);
		} else {
			secondReference = null;
		}
		if (references.size() != expectedNumberOfReferences) {
			System.err.println("***Failure: there should have been exactly " + expectedNumberOfReferences + " reference(s), not " + references.size());
			return false;
		} else {
			System.err.println("Successfully found exactly " + expectedNumberOfReferences + " reference(s)");
			return true;
		}
	}

	/**
	 * Read the next line, tokenise it, and return the the number of tokens (references).
	 * 
	 * This method is called from the classic heapdump checkers, when the file will be already
	 * positioned on the OBJ line and the references always come on the next line 
	 * 
	 * @return number of references
	 * @throws IOException
	 */
	public int skipToNextLineAndCountReferences() throws IOException {
		// if we are called by one of the classic heapdump checkers we are currently on the OBJ line and 
		// skipping to the next line will put us on the list of references
		String line = in.readLine();
		if (line == null) {
			System.err.println("***Unable to check count of references - previous call to find the references lines has failed.");
			return 0;
		}
		
		ArrayList<String> references = StringUtils.extractTokensFromLine(line);
		return references.size();
	}
	
	public static boolean checkReferenceInClassicHeapdumpIsToObject(String fileName, String reference, String className) throws IOException {
		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
		System.err.println("Checking that the reference " + reference + " is to an object of type " + className);
		passed &= o.skipUnlimitedToLineBeginning(reference);
		passed &= o.linePassesCheckForStringAtWord(className,4);
		return passed;
	}

	private boolean skipUnlimitedToLineBeginning(String lookFor) throws IOException {
		line = in.readLine();
		while (line != null) {
			if (line.startsWith(lookFor) ) {
				System.err.printf("Successfully found a line beginning \"%s\"\n", lookFor);
				System.err.printf(line);
				System.err.printf("\n");
				return true;
			}
			line = in.readLine();
		}
		System.err.printf("***Reached end of file before finding a line beginning \"%s\"\n", lookFor);
		return false;
	}

	public static boolean checkReferenceIsToObject(String fileName, String reference, String className) throws IOException {
		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
		System.err.println("Checking that the reference " + reference + " is to an object of type " + className);
		passed &= o.skipUnlimitedToLineContaining(className + " @ " + reference);
		return passed;
	}

}

