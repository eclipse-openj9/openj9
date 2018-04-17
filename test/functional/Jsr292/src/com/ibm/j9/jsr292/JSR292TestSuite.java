/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import java.util.Enumeration;
import junit.framework.TestResult;

/**
 * @author mesbaha
 * This class extends the default junit.framework.TestSuite class and redefines the run() method 
 * in order to run only the test cases that are not in a given exclude list for this test suite.
 */
public class JSR292TestSuite extends junit.framework.TestSuite {

	private String excludeList;
	
	/**
	 * Default constructor
	 * @param name - Name of the test suite 
	 * @param ignoreTestCaseList - A comma separated list of test cases to ignore
	 */
	public JSR292TestSuite( String name, String excludeList ) {
		super( name );
		this.excludeList = excludeList;
	}
	
	/* ( non-Javadoc )
	 * @see junit.framework.TestSuite#run( junit.framework.TestResult )
	 * Overridden version of TestSuite.run() method which invokes the new run( TestSuite, TestResult ) method 
	 * in order to ensure we use test filtering.
	 * @param result - This TestResult object to use for result reporting 
	 */
	public void run( TestResult result ) {
		run( this, result );
	}
	
	/**
	 * This method runs all test cases from this test suite but the ones found in the given ignore list  
	 * @param suite - This TestSuite object 
	 * @param result - The TestResult object to use
	 */
	public void run( junit.framework.TestSuite suite, TestResult result ) {
		@SuppressWarnings( "rawtypes" )
		Enumeration en = suite.tests();
		while ( en.hasMoreElements() ) {
			junit.framework.Test test = ( junit.framework.Test )en.nextElement();
			if ( test instanceof junit.framework.TestSuite ) {
				run( ( junit.framework.TestSuite )test, result );
			} else {
				junit.framework.TestCase testCase = ( junit.framework.TestCase )test;
				//Ignore test cases from the exclude list
				if ( excludeList == null || excludeList.contains( testCase.getName() ) == false ) {
					super.runTest( testCase, result );
				}
			}
		}
	}
}
