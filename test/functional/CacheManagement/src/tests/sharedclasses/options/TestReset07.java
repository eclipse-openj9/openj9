/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create an incompatible cache and a non-persistent cache, check reset warns us that we need to
 * specify nonpersistent and not that it reports the incompatible one.
 */
public class TestReset07 extends TestUtils {
  public static void main(String[] args) {
	  try {
	    runDestroyAllCaches();    
	    createIncompatibleCache("Foo");
	    createNonPersistentCache("Foo");
	    runResetPersistentCache("Foo");
	   // checkOutputContains("JVMSHRC278I.*Foo","Did not get expected message JVMSHRC273E about the incompatible form of Foo");
	    
	    showOutput();
	    fail("What is the expected message here?  I think I expect the message 'did you mean the non-persistent cache Foo because you forgot to specify nonpersistent'");
	  } finally {
		  runDestroyAllCaches();
	  }
  }
}
