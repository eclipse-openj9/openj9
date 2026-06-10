/*
 * Copyright IBM Corp. and others 2004
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
import com.ibm.oti.util.regex.Regex;

class Output extends TestCondition {

	private static boolean evalBoolean(String expr) {
		return !"no".equalsIgnoreCase(TestSuite.evaluateVariables(expr));
	}

	private final String _type;
	private final String _matchRegex;
	private final String _matchJavaUtilPattern;
	private final String _matchCase;
	private String _output; // the output string taken from the file
	private final String _showRegexMatch;

	public Output(String matchRegex, String matchJavaUtilPattern, String showRegexMatch, String matchCase, String type) {
		_matchRegex = matchRegex;
		_matchJavaUtilPattern = matchJavaUtilPattern;
		_showRegexMatch = showRegexMatch;
		_matchCase = matchCase;
		_type = type;
		_output = "";
	}

	void setOutput(String s) {
		_output = s;
	}

	int getType() {
		return parseType(TestSuite.evaluateVariables(_type));
	}

	public boolean isJavaUtilPattern() {
		return evalBoolean(_matchJavaUtilPattern);
	}

	boolean match(Object o) {
		String candidate = (String) o;
		boolean matchRegex = evalBoolean(_matchRegex);
		boolean matchCase = evalBoolean(_matchCase);
		boolean matchJavaUtilPattern = evalBoolean(_matchJavaUtilPattern);
		boolean showRegexMatch = evalBoolean(_showRegexMatch);
		String string = TestSuite.evaluateVariables(_output);

		if (matchRegex && !matchJavaUtilPattern) {
			try {
				Regex regex = new Regex(string, matchCase);
				boolean retval = regex.matches(candidate);
				if (retval && showRegexMatch) {
					System.out.println("\tMatch (" + _type + "): " + candidate);
				}
				return retval;
			} catch (Exception e) {
				System.out.println("Exception " + e.getClass().toString() + " message " + e.getMessage());
				System.out.println("OTI Regex:" + string);
				e.printStackTrace();
				return false;
			}
		} else if (matchRegex && matchJavaUtilPattern) {
			/* CMVC 163891: Use of java.util.regex.Pattern/Matcher must be in a separate class, to avoid
			 * problems with the verifier when using the embedded class library.
			 */
			return OutputRegexHelper.ContainsMatches(candidate, string, matchCase, showRegexMatch, _type);
		} else {
			if (!matchCase) {
				string = string.toLowerCase();
				candidate = candidate.toLowerCase();
			}
			boolean retval = candidate.contains(string);
			if (retval && showRegexMatch) {
				System.out.println("\tMatch (" + _type + "): " + candidate);
			}
			return retval;
		}
	}

	@Override
	public String toString() {
		return "Output match: " + TestSuite.evaluateVariables(_output);
	}

}
