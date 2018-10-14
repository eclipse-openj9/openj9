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

import java.util.*;
import java.util.regex.Pattern;

import com.oti.j9.exclude.*;

class TestConfigParser {
	private ExcludeList _excludes;
	private String[] _platforms;
	private TestSuite _suite;
	private boolean _verbose;
	private int _outputLimit;
	private boolean _debugCmdOnTimeout = false;
	private String _modeHints;
	
	/**
	 * If true, the test suite will print out the full output for each test case, regardless of whether
	 * it passed or failed
	 */
	void setVerbose( boolean verbose ) {
		_verbose = verbose;
		if (_suite != null) {
			_suite.setVerbose( _verbose );
		}
	}
	
	void setOutputLimit ( int limit ) {
		_outputLimit = limit;
		if (_suite != null) {
			_suite.setOutputLimit( _outputLimit );
		}
	}
	/**
	 * Takes the given configuration file, exclude list, and optional platforms, and generates a test suite
	 * complete with test cases and expected outputs.
	 * 
	 * @param configFile - The file from which to read the test case information. It should be formatted in
	 *                     accordance with the cmdlinetester.dtd file (see j:\j9\testing\cmdlinetester\)
	 * @param excludes - The exclude list, which specifies test cases to exclude on certain platforms. See
	 *                   j:\j9\testing\excludes\ for more information on the structure of this file. If null,
	 *                   no tests are excluded
	 * @param platforms - A comma-delimited set of strings identifying which platform-dependent output strings
	 *                    in the configuration file should be used.
	 * @param modeHints - List of hints specified for the mode being used to run this test suite. 
	 * @return The test suite object from which the tests can be run, and which will output the pass/fail stats
	 *         of the test cases.
	 */
	TestSuite runTests( String configFile, ExcludeList excludes, String platforms, boolean debugCmdOnTimeout, String modeHints ) {
		_excludes = excludes;
		_platforms = doSplit( platforms, "," );
		_debugCmdOnTimeout = debugCmdOnTimeout;
		_modeHints = modeHints;
		XMLParser xmlp = new XMLParser();
		xmlp.parse( configFile, new TestConfigDocumentHandler() );
		return _suite;
	}

	/**
	 * Basic string-splitting function, roughly equivalent to perl's split( /delim/, s ).
	 * 
	 * @param s The string to be split
	 * @param delim The delimiter between the different pieces in the string. This must be non-null
	 * @return The array of pieces in the string. This will always be non-null, although may be of size zero
	 *         if <code>s</code> is null or of length zero.
	 */
	private static String[] doSplit( String s, String delim ) {
		if (s == null || s.length() == 0) {
			return new String[0];
		}
		StringTokenizer st = new StringTokenizer( s, delim );
		String[] pieces = new String[ st.countTokens() ];
		for (int i = 0; i < pieces.length; i++) {
			pieces[i] = st.nextToken();
		}
		return pieces;
	}
	
	/**
	 * The class that is used as the document handler when SAX-ing the configuration file. This
	 * class is responsible for reading information from the configuration file, converting it to
	 * the relevant Java objects and adding them to the test suite.
	 */
	class TestConfigDocumentHandler implements IXMLDocumentHandler {
		/* Test currently being processed */
		private Test _currentTest;
		/* Output case currently being processed */
		private Output _currentOutput;
		/* exec case currently being processed */
		private CommandExecuter _currentExec; 
		/* The relevant non-element character data being spit out by the SAX parser */
		private StringBuffer _data = new StringBuffer();
		/* This is a single-iteration TestIterator which will run all the things given to it exactly once */
		private TestIterator _iterator;
		// if this is a new-style command where the executable is specified on its own, that is stored here (otherwise, this variable must be null)
		private String _commandExecutable;
		// this flag allows us to continue to use this parser implementation for the more complicated use of the command element without having to re-write it as a sort of delegating PDA
		private boolean _isInNewCommandStanza;
		// used to collect the contents of the in-order "arg" tags under the new-style command stanza (which are then passed as arguments to the underlying process)
		private Vector _commandArgs;
		// used to collect the contents of the in-order "input" tags under the new-style command stanza (which are then fed into the stdin of the underlying process as new-line terminated strings)
		private Vector _commandInputLines;
		
		/**
		 * Empty implementation
		 */
		public void xmlStartDocument() {
		}

		/**
		 * Empty implementation
		 */
		public void xmlEndDocument() {
		}

		/**
		 * @see com.oti.j9.exclude.IXMLDocumentHandler#xmlStartElement(String,Hashtable)
		 */
		public void xmlStartElement(String elementName, Hashtable attributes) {
			if (elementName.equalsIgnoreCase("suite")) {
				System.out.println("*** Starting test suite: " + attributes.get("id") + " ***");
				long timeout = getTimeout( attributes.get("timeout"), 10000 );
				_suite = new TestSuite( _excludes, timeout, _verbose, _outputLimit );
				_iterator = new TestIterator( _suite );

			} else if (elementName.equalsIgnoreCase("loop")) {
				String index = (String)attributes.get("index");
				String from = (String)attributes.get("from");
				String until = (String)attributes.get("until");
				String inc = (String)attributes.get("inc");
				_iterator.addSubIterator( new TestIterator( _suite, index, from, until, inc ) );

			} else if (elementName.equalsIgnoreCase("test")) {
				if (hasAllowedPlatform((String)attributes.get("platforms"))) {
					String id = (String)attributes.get("id");
					String modeHints = (String)attributes.get("modeHints");
					if (hasAllowedHints(modeHints)) {
						String timeout = (String)attributes.get("timeout");
						_currentTest = new Test( id, timeout, _debugCmdOnTimeout );
					} else {
						if ((_modeHints == null) || (_modeHints.matches("\\s*"))) {
							_modeHints = "empty";
						}
						System.out
								.println("Test Skipped: "
										+ id
										+ " : Hints specified for this test { "
										+ modeHints
										+ " } do not match with the hints for current mode { "
										+ _modeHints + " }\n");
						_currentTest = null;
					}
				} else {
					_currentTest = null;
				}
			} else if (elementName.equalsIgnoreCase("output")) {
				if (hasAllowedPlatform((String)attributes.get("platforms"))) {
					String regex = (String)attributes.get("regex");
					String javaUtilPattern = "no";
					if (null != attributes.get("javaUtilPattern")) {
						javaUtilPattern = (String)attributes.get("javaUtilPattern");
					}
					String showRegexMatch = "no";
					if (null != attributes.get("showMatch")) {
						showRegexMatch = (String)attributes.get("showMatch");
					}
					String caseSensitive = (String)attributes.get("caseSensitive");
					String type = (String)attributes.get("type");
					_currentOutput = new Output( regex, javaUtilPattern, showRegexMatch, caseSensitive, type );
				} else {
					_currentOutput = null;
				}

			} else if (elementName.equalsIgnoreCase("saveoutput")) {
				if (hasAllowedPlatform((String)attributes.get("platforms"))) {
					String regex = (String)attributes.get("regex");
					String javaUtilPattern = "no";
					String showRegexMatch = "no";
					if (null != attributes.get("showMatch")) {
						showRegexMatch = (String)attributes.get("showMatch");
					}
					String caseSensitive = (String)attributes.get("caseSensitive");
					String saveName = (String)attributes.get("saveName");
					String splitIndex = (String)attributes.get("splitIndex");
					String splitBy = (String)attributes.get("splitBy");
					String type = (String)attributes.get("type");
					_currentOutput = new SaveOutput( regex, javaUtilPattern, showRegexMatch, caseSensitive, type, saveName, splitIndex, splitBy);
				} else {
					_currentOutput = null;
				}

			} else if (elementName.equalsIgnoreCase("return")) {
				if ((_currentTest != null) && (hasAllowedPlatform((String)attributes.get("platforms")))) {
					String value = (String)attributes.get("value");
					String type = (String)attributes.get("type");
					_currentTest.addTestCondition( new ReturnValue( value, type ) );
				}

			} else if (elementName.equalsIgnoreCase("variable")) {
				if (hasAllowedPlatform((String)attributes.get("platforms"))) {
					String name = (String)attributes.get("name");
					String value = (String)attributes.get("value");
					_iterator.addCommand( new VariableAdder( name, value ) );
				}

			} else if (elementName.equalsIgnoreCase("exec")) {
				if (hasAllowedPlatform((String)attributes.get("platforms"))) {
					String command = (String)attributes.get("command");
					String background = (String)attributes.get("background");
					String captureVarStdout = (String)attributes.get("capture");
					String captureVarStderr = (String)attributes.get("captureStderr");
					String returnVar = (String)attributes.get("return");
					_currentExec = new CommandExecuter(command, background, captureVarStdout, captureVarStderr, returnVar);
				}
			} else if (elementName.equalsIgnoreCase("delay")) {
				_iterator.addCommand( new Delay( (String)attributes.get("length") ) );

			} else if (elementName.equalsIgnoreCase("echo")) {
				_iterator.addCommand( new Echo( (String)attributes.get("value") ) );
			} else if (elementName.equalsIgnoreCase("envvar")) {
				// to get envvars to work properly in loops, changed EnvVar class to Command type
				_iterator.addCommand( new EnvVar( (String)attributes.get("name"), (String)attributes.get("value") ) );
			} else if (elementName.equalsIgnoreCase("command")) {
				//the command tag can be structured multiple ways.  It can just be simple like <command>echo This is what I am echoing</command> or
				// it can be more complicated like:
				// <command command="someCommand"><arg>one argument</arg><arg>arg two</arg><input>one line of stdin</input><input>second line of stdin</input></command>
				// (note that the use of this "command" attribute looks odd but is to be consistent with "exec")
				// so we will determine which one is which in this start tag since we would need a "command" attribute to be using the new style
				String commandExecutable = (String)attributes.get("command");
				if (null != commandExecutable)
				{
					//this is the "new-style" command tag so get the executable name
					_commandExecutable = commandExecutable;
					//also set the flag to know that we are in a command context and create the arrays for the arguments and inputs which may appear in that stanza
					_isInNewCommandStanza = true;
					_commandArgs = new Vector();
					_commandInputLines = new Vector();
				}
			} else if (elementName.equalsIgnoreCase("if")) {
				_iterator.addCommand( new IfTest( (String)attributes.get("testVariable"), (String)attributes.get("testValue"),
					(String)attributes.get("resultVariable"), (String)attributes.get("resultValue") ) );
			}

			// clear buffer to read in stuff within this tag
			_data.setLength(0);
		}

		/**
		 * @see com.oti.j9.exclude.IXMLDocumentHandler#xmlEndElement(String)
		 */
		public void xmlEndElement(String elementName) {
			String readData = _data.toString();
			
			if (elementName.equalsIgnoreCase("loop")) {
				// </loop> - close off the most-nested loop. if _iterator doesn't have any
				// nested loops, it will return false, so we close off _iterator by setting it
				// to null
				_iterator.closeInnerLoop();

			} else if (elementName.equalsIgnoreCase("command")) {
				// </command>
				if (_currentTest != null) {
					if (_isInNewCommandStanza) {
						_currentTest.setSplitCommand(_commandExecutable, _commandArgs, _commandInputLines);
					} else {
						_currentTest.setCommand(readData);
					}
					_isInNewCommandStanza = false;
				}
			} else if (elementName.equalsIgnoreCase("output") || elementName.equalsIgnoreCase("saveoutput")) {
				// </output> - check to make sure _currentOutput is not null
				// before proceeding - this may happen if the output tag was thrown out
				// due to platform-dependency-incompatibilities
				if ((_currentTest != null) && (_currentOutput != null)) {
					_currentOutput.setOutput(readData);
					_currentTest.addTestCondition( _currentOutput );
				}

			} else if (elementName.equalsIgnoreCase("test")) {
				// </test> - pass it off to the iterator to run
				if (_currentTest != null) {
					_iterator.addTest( _currentTest );
				}
			} else if (elementName.equalsIgnoreCase("exec")) {
				if (_currentExec != null) {
					_iterator.addCommand( _currentExec );
				}
				_currentExec = null;
			} else if (elementName.equalsIgnoreCase("arg")) {
				if (_isInNewCommandStanza) {
					_commandArgs.add(readData);
				} else {
					if (_currentExec != null) {
						_currentExec.addArg(readData);
					}
				}
			} else if (elementName.equalsIgnoreCase("input")) {
				if (_isInNewCommandStanza) {
					_commandInputLines.add(readData);
				}
			}
			_data.setLength( 0 );	// any data in here has outlived it's usefulness
		}

		/**
		 * @see com.oti.j9.exclude.IXMLDocumentHandler#xmlCharacters(String)
		 */
		public void xmlCharacters(String chars) {
			_data.append( chars );
		}

		private boolean hasAllowedPlatform( String platforms ) {
			// check the platform-dependencies. if there are no
			// listed platforms or if any one of the listed platforms matches any one of
			// the platforms specified on the runtime commandline, then we return true
			String evaluatePlatforms = TestSuite.evaluateVariables(platforms);
			String[] requiredPlatforms = doSplit( evaluatePlatforms, "," );
			boolean allow = (requiredPlatforms.length == 0);
			for (int i = 0; i < _platforms.length; i++) {
				for (int j = 0; j < requiredPlatforms.length; j++) {
					if (Pattern.matches(requiredPlatforms[j], _platforms[i])) {
						allow = true;
						// like a double break
						j = requiredPlatforms.length;
						i = _platforms.length;
					}
				}
			}
			return allow;
		}
		
		private boolean hasAllowedHints(String modeHints) {
			String[] hintsSet = doSplit(modeHints, ",");
			if (hintsSet.length == 0) {
				return true;
			} else if (_modeHints == null) {
				return false;
			} else {
				for (int i = 0; i < hintsSet.length; i++) {
					boolean allHintsFound = true;
					String[] hints = doSplit(hintsSet[i], " ");
					for (int j = 0; j < hints.length; j++) {
						if (_modeHints.indexOf(hints[i]) == -1) {
							allHintsFound = false;
							break;
						}
					}
					if (allHintsFound == true) {
						return true;
					}
				}
			}
			return false;
		}
		
		private long getTimeout( Object attribute, long defaultValue ) {
			long timeout = defaultValue;
			try {
				timeout = 1000 * Integer.parseInt( ((String)attribute).trim() );
			} catch (Exception e) { }
			return timeout;
		}
	}
}
