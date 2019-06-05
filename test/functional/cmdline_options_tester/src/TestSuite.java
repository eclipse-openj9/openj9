/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.io.*;
import java.text.*;
import java.util.*;
import com.oti.j9.exclude.*;

/**
 * This is the test suite object that keeps track of all the tests and runs them.
 */
public class TestSuite {

	/**
	 * The user-defined variables in the configuration file. Substitution of these is done
	 * later, just before the commands are actually executed
	 */
	private static Hashtable _variables = new Hashtable();
	private static final String failedPropertyName = "cmdlinetester.failed_tests";
	private ExcludeList _excludes;
	private long _defaultTimeout;
	private boolean _verbose;
	private int _outputLimit;
	private static SortedSet _processedVariables;
	
	private ArrayList _passed;
	private ArrayList _failed;

	/* ArrayList to hold environment variables */
	private static Hashtable _envVarList = new Hashtable();
	/**
	 * Initializes variables.
	 */
	TestSuite( ExcludeList excludes, long defaultTimeout, boolean verbose, int outputLimit ) {
		_variables = new Hashtable();
		_excludes = excludes;
		_defaultTimeout = defaultTimeout;
		_verbose = verbose;
		_passed = new ArrayList();
		_failed = new ArrayList();
		_outputLimit = outputLimit;
		System.setProperty(failedPropertyName, "0");
	}
	
	void setVerbose( boolean verbose ) {
		_verbose = verbose;
	}
	
	void setOutputLimit( int limit ) {
		_outputLimit = limit;
	}
	
	/**
	 * Add a new test
	 */
	void runTest( Test test ) {
		try {
			if (_excludes != null && !_excludes.shouldRun( test.getId() )) {
				System.out.println( "Test excluded: " + test.getId() + '\n' );
				return;
			}
			String testName = test.getId();
			System.out.println( "Testing: " + testName );
			test.setOutputLimit(_outputLimit);
			boolean result = test.run( _defaultTimeout );	// <-- actually run the test
			if (result) {
				_passed.add( testName );
				System.out.println( "Test result: PASSED" );
				if (_verbose) {
					test.dumpVerboseOutput(result);
				}
				System.out.print( '\n' );
			} else {
				_failed.add( testName );
				System.out.println( "Test result: FAILED" );
				test.dumpVerboseOutput(result);	// print verbose output in case of failure
				System.out.print( '\n' );
				
				int failedTests = Integer.parseInt(System.getProperty(failedPropertyName));
				failedTests++;
				System.setProperty(failedPropertyName, Integer.toString(failedTests));
			}
			test.destroy();
		} catch (Exception e) {
			TestSuite.printStackTrace(e);
		}
	}
	
	void printResults() {
		// print aggregate statistics
		System.out.println("\n---TEST RESULTS---");
		System.out.println("Number of PASSED tests: " + _passed.size() + " out of " + (_passed.size() + _failed.size()));
		System.out.println("Number of FAILED tests: " + _failed.size() + " out of " + (_passed.size() + _failed.size()));

		if (_failed.size() > 0) {
			System.out.println("\n---SUMMARY OF FAILED TESTS---");
			for (int i = 0; i < _failed.size(); i++) {
				System.out.println( (String)_failed.get(i) );
			}
			System.out.println("-----------------------------\n");
		}
	}
	
	int getFailureCount() {
		return _failed.size();
	}
	
	public static void putVariable( String key, String value ) {
		_variables.put( key, value );
	}
	
	public static String getVariable( String key ) {
		//return (String)_variables.get( key );
		/* MODIFIED by Oliver Deakin - 20050211 - treat system properties the same as variables - do this under the covers
			so that there is no distinction between system properties and variables */
		String returnString = (String) (_variables.get(key));
		if (returnString == null)
		{
			// If key is not in the cmdlinetester variables, check system properties
			returnString = (String) (TestSuite.getSystemProperty(key));
		}

		return returnString;
	}

	public static String evaluateVariables( String s ) {
		return evaluateVariables(s, null);
	}
	
	/* A new way to implement use of environment variables */
	public static void putEnvironmentVariable( String key, String value ) {
		_envVarList.put( key, value );
	}

	/* This will return a list of the environment variables in a format that Runtime.exec can understand:
	"name=value"
	 */
	public static String[] getEnvironmentVariableList() {
		//Create an array string from the current environment variables
		String[] envVarArray;
		if (_envVarList.size() == 0)
		{
			return null;
		} 

		envVarArray = new String[_envVarList.size()];

		// Iterate through the Hashtable turning key/value pairs into the correct format
		Object[] keyNames = _envVarList.keySet().toArray();
		for (int keyNum = 0; keyNum<_envVarList.size(); keyNum++)
		{
			String key = (String)keyNames[keyNum];
			String value = (String)_envVarList.get(key);
			envVarArray[keyNum] = key + "=" + value;
		}

		return envVarArray;
	}

	public static String getSystemProperty( String key ) {
		return (String) (System.getProperties().get( key ));
	}

	/**
	 * Does a variable replacement on s using the variables in <code>_variables</code> and
	 * System.properties. Also does a replacement of nested variables within variable
	 * names. For example, if $HI$ = "WORD", $WORD$ = "ING", and $TESTING$ = "SOMETHING",
	 * then $TEST{$HI$}$ would evaluate as follows:
	 * 
	 *     $TEST{$HI$}$ -> $TEST{WORD}$ -> $TESTING$ -> SOMETHING
	 * 
	 * Curly brackets have no special meaning if outside of a set of dollar signs. When
	 * inside a set dollar signs, curly brackets always have special meaning.  For
	 * example, "{}TEST$TEST{{HI}}${HI}" would evaluate as follows (using the same
	 * variable values as in the previous example):
	 * 
	 *     {}TEST$TEST{{HI}}${HI} -> {}TEST$TEST{WORD}${HI} -> {}TEST$TESTING${HI} ->
	 *     {}TESTSOMETHING{HI}
	 * 
	 * For each (<code>key</code>, <code>value</code>) pair in <code>_variables</code>
	 * and System.properties, this method replaces all instances of $<code>key</code>$
	 * with <code>value</code> (taking into account the nested variable replacement
	 * described above). All the substitutions from <code>_variables</code> are
	 * done before any of the substitutions from System.properties.
	 * 
	 * Note: this method will "expand" the String "$$" to "$" after all other expansions
	 * have been performed.  This is done so that the user can specify a literal "$" if
	 * desired.
	 * 
	 * @param s - The source string
	 * @param variableName - The name of the variable whose value is being evaluated
	 *     if applicable; null if it is not a variable's value that is being evaluated 
	 * @return A copy of s with all the substitutions performed.
	 */
	public static String evaluateVariables( String s, String variableName ) {
		if (s == null || s.equals("$$")) {
			return s;
		}
		
		String value = s;
		
		if (null == variableName) {
			_processedVariables = new TreeSet();
		} else {
			if (_processedVariables.contains(variableName)) {
				System.err.println(
						"The variable \"" + variableName + "\" with value \"" + value +
						"\" cannot be resolved because it refers to itself somewhere " +
						"along the variable substitution chain.");
				System.exit(1);
			} else {
				_processedVariables.add(variableName);
			}
		}

		Stack tokens = new Stack();
		int varStartIndex = -1;
		
		for (int i = 0; i < s.length(); i++)
		{
			char currChar = s.charAt(i);
			switch (currChar)
			{
			case '$':
				if (!tokens.isEmpty() && '$' == ((Character)tokens.peek()).charValue()) {
					tokens.pop();
					if (tokens.isEmpty())
					{
						String newString = s.substring(0, varStartIndex - 1) +
							expandVariable(s.substring(varStartIndex, i));
						int newIndex = newString.length() - 1;	// adjust for i++ at end of loop

						if ( (i + 1) < s.length()) {
							newString += s.substring(i + 1);
						}

						s = newString;
						i = newIndex;
						varStartIndex = -1;
					}
				} else {
					if (tokens.isEmpty()) {
						varStartIndex = i + 1;
					}
					tokens.push(new Character('$'));
				}
				break;
				
			case '{':
				if (!tokens.isEmpty()) {
					tokens.push(new Character('{'));
				}
				break;
				
			case '}':
				if (!tokens.isEmpty()) {
					if ('{' == ((Character)tokens.peek()).charValue()) {
						tokens.pop();
					} else {
						System.err.println(
								"The variable \"" + variableName + "\" with value \"" + value +
								"\" cannot be resolved because it contains a '}' after " +
								"an opening '$'.");
						System.exit(1);
					}
				}
			}
		}
		
		if (!tokens.isEmpty())
		{
			System.err.println(
					"The variable \"" + variableName + "\" with value \"" + value +
					"\" cannot be resolved because it is missing at least one token " +
					"signifying the end of a variable ('$' or '}'); the first " +
					"unbalanced start token is a '" +
					((Character)tokens.pop()).charValue() + "'");
			System.exit(1);
		}
		
		if (null != variableName) {
			_processedVariables.remove(variableName);
		}
		
		if (null == variableName)
		{
			boolean lastWasDollar = false;
			for (int i = 0; i < s.length(); i++)
			{
				char c = s.charAt(i);
				if ('$' == c)
				{
					if (lastWasDollar)
					{
						s = s.substring(0, i - 1) + s.substring(i);
						i--;	// we removed a char from the String so adjust i accordingly
						
						lastWasDollar = false;
					} else {
						lastWasDollar = true;
					}
				}
			}
		}
		
		return s;
	}

	private static String expandVariable(String s)
	{
		String unexpandedName = s;
		Stack tokens = new Stack();
		int varStartIndex = -1;
		
		for (int i = 0; i < s.length(); i++)
		{
			char currChar = s.charAt(i);
			switch (currChar)
			{
			case '$':
				if (!tokens.isEmpty() && '$' == ((Character)tokens.peek()).charValue()) {
					tokens.pop();
					if (tokens.isEmpty())
					{
						String newString = s.substring(0, varStartIndex - 1) +
							expandVariable(s.substring(varStartIndex, i));
						int newIndex = newString.length() - 1;	// adjust for i++ at end of loop

						if ( (i + 1) < s.length()) {
							newString += s.substring(i + 1);
						}

						s = newString;
						i = newIndex;
						varStartIndex = -1;
					}
				} else {
					if (tokens.isEmpty()) {
						varStartIndex = i + 1;
					}
					tokens.push(new Character('$'));
				}
				break;
				
			case '{':
				if (tokens.isEmpty()) {
					varStartIndex = i + 1;
				}
				tokens.push(new Character('{'));
				break;
				
			case '}':
				if (tokens.isEmpty()) {
					System.err.println(
							"The variable name \"" + unexpandedName + "\" cannot be " +
							"resolved because it contains a '}' that does not refer " +
							"to a corresponding '{'.");
					System.exit(1);
				} else {
					if ('{' == ((Character)tokens.peek()).charValue()) {
						tokens.pop();
						if (tokens.isEmpty())
						{
							String newString = s.substring(0, varStartIndex - 1) +
								expandVariable(s.substring(varStartIndex, i));
							int newIndex = newString.length() - 1;	// adjust for i++ at end of loop

							if ( (i + 1) < s.length()) {
								newString += s.substring(i + 1);
							}

							s = newString;
							i = newIndex;
							varStartIndex = -1;
						}
					} else {
						System.err.println(
								"The variable name \"" + unexpandedName + "\" cannot " +
								"be resolved because it contains a '}' after " +
								"an opening '$'.");
						System.exit(1);
					}
				}
			}
		}
		
		if (!tokens.isEmpty())
		{
			System.err.println(
					"The variable name \"" + unexpandedName + "\" cannot " +
					"be resolved because it is missing at least one token " +
					"signifying the end of a variable ('$' or '}'); the first " +
					"unbalanced start token is a '" +
					((Character)tokens.pop()).charValue() + "'");
			System.exit(1);
		}
		
		String varValue = getVariableValue(s);
		
		if (null == varValue)
		{
			System.err.println(
					"The variable \"" + s + "\" does not exist; original string " +
					"used to construct the variable name was \"" + unexpandedName + "\"");
			System.exit(1);
		}
		
		return evaluateVariables(varValue, s);
	}
	
	private static String getVariableValue(String variableName)
	{
		if (variableName.equals("")) {
			return "$$";
		}
		
		String value;
		
		value = getVariable(variableName);
		if (null != value)
		{
			return value;
		}
		
		value = (String)System.getProperties().get(variableName);
		return value;
	}
		
	/**
	 * Basic implementation of the String.replaceAll that's in Java 1.4+
	 * 
	 * @param s - The source string
	 * @param find - Substring to find
	 * @param replace - Substring to replace all instances of <code>find</code> with
	 * @return A copy of <code>s<code>, with all instances of <code>find</code> replaced with
	 *         <code>replace</code>
	 */
	public static String doReplaceAll( String s, String find, String replace ) {
		int lastIndex = 0;
		int index = s.indexOf( find, lastIndex );
		while (index >= 0) {
			s = s.substring( 0, index ) + replace + s.substring( index + find.length() );
			lastIndex = index + replace.length();
			index = s.indexOf( find, lastIndex );
		}
		return s;
	}
	
	public static void printErrorMessage(String message) {
		Calendar calendar = Calendar.getInstance();
		DateFormat df = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
		System.out.println("***[TEST INFO " + df.format(calendar.getTime()) +  "] " + message + "***");
	}
	
	public static void printStackTrace(Throwable e) {
		StringWriter sw = new StringWriter();
		PrintWriter pw = new PrintWriter(sw);
		e.printStackTrace(pw);
		
		String[] stack = sw.toString().split("\n");
		for (int i = 0; i < stack.length; i++) {
			String element = stack[i];
			printErrorMessage(element);
		}
		
		pw.close();
	}
}
