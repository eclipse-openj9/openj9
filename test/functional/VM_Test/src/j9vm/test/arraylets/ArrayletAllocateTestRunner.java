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
package j9vm.test.arraylets;

import j9vm.runner.Runner;

public class ArrayletAllocateTestRunner extends Runner {

	private final String customizedHeapOptions = "-Xms65m -Xmx65m";
	
	public ArrayletAllocateTestRunner(String className, String exeName, String bootClassPath, String userClassPath, String javaVersion) {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* See CMVC 172289 -- this test triggers excessive GC in gencon. Disable excessive until the defect is resolved. */
	@Override
	public String getCustomCommandLineOptions() {
		return super.getCustomCommandLineOptions() + " -Xdisableexcessivegc";
	}
	
	/* 
	 * Override getCommandLine() to change heapOptions
	 */
	@Override
	public String getCommandLine() {
		return super.exeName + " " + customizedHeapOptions + " " + getCustomCommandLineOptions() + " "
			+ super.getJ9VMSystemPropertiesString() + " " + super.getBootClassPathOption() + " "
			+ super.getUserClassPathOption() + " ";
	}

}
