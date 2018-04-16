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
package org.openj9.test.jsr335.defendersupersends;

import java.io.IOException;
import java.util.ArrayList;

import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestListener;
import junit.framework.TestResult;
import junit.framework.TestSuite;
import junit.textui.*;
import org.openj9.test.jsr335.defendersupersends.asm.AsmUtils;

public class TestDefenderSupersends {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		Class<?> testClass = AsmUtils.getDefenderSupersendTestcase();
		Test suite =  new TestSuite(testClass);
		TestResult result = new TestResult();
		result.addListener( new TestListener() {
			public void addError( Test test, Throwable t ) {
				t.printStackTrace( System.err );
			}

			public void addFailure( Test test, AssertionFailedError t ) {
				t.printStackTrace( System.err );
			}
			public void endTest( Test test ) {
				System.out.println( "Finished " + test );
			}

			public void startTest( Test test ) {
				System.out.println( "Starting " + test );
			}
		} );
		suite.run(result);
		System.out.println("================Test Result ==================");
		System.out.println("Failures:   " + result.failureCount() + " out of "
				+ result.runCount() + " test cases." );
		System.out.println("Errors:   " + result.errorCount() + " out of "
				+ result.runCount() + " test cases." );
		//return value
	}

}
