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

class IfTest implements Command {
	private String _testVariableName;
	private String _testVariableValue;
	private String _resultVariableName;
	private String _resultVariableValue;
	
	IfTest( String testVariableName, String testVariableValue, String resultVariableName, String resultVariableValue ) {
		_testVariableName = testVariableName;
		_testVariableValue = testVariableValue;
		_resultVariableName = resultVariableName;
		_resultVariableValue = resultVariableValue;
	}
	
	public void executeSelf() {
		String actualVariableValue = TestSuite.getVariable( _testVariableName );
	
		if (actualVariableValue ==null) {
			// Consider null to be the same as ""
			actualVariableValue = "";
		}

		/* If the variable value (actualVariableValue) is equal to the one we are testing again (_testVariableValue)
		   If it is, then replace the variable (_resultVariableName) with the value passed into the constructor (_resultVariableValue).
		   If they are not equal, then we do nothing, as the if test fails
		 */

		if ( actualVariableValue.equals(_testVariableValue)	 )
		{
			TestSuite.putVariable( _resultVariableName, TestSuite.evaluateVariables(_resultVariableValue) );
		}

	}
}
