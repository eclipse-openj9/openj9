/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.javahome;


public class JavaHomeTestRunner extends j9vm.runner.Runner {

public JavaHomeTestRunner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion) {
	super(className, exeName, bootClassPath, userClassPath, javaVersion);
}

public String getBootClassPathOption()  { return ""; }

public boolean run()  {
	boolean passed = true;
	int rc = runCommandLine(getCommandLine() + "-Djava.home=" +
			System.getProperty("java.home") + " " + className);
	System.out.println("rc = " + rc + "\n");
	if (0 != rc) {
		passed = false;
	}
	
	System.out.println("////// Failure starting VM expected here //////");
	rc = runCommandLine(getCommandLine() + "-Djava.home=FooFooFoo! " + className);
	System.out.println("rc = " + rc + "\n");
	if (1 != rc) {
		passed = false;
	}
	
	System.out.println("////// Failure starting VM expected here //////");
	rc = runCommandLine(getCommandLine() + "-Djava.home= " + className);
	System.out.println("rc = " + rc + "\n");
	if (1 != rc) {
		passed = false;
	}
	
	return passed;
}

}
