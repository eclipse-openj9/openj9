/*
 * Copyright IBM Corp. and others 2010
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

package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create non-persistent cache, run something, check whats in there, then reset (as a standalone action) and check
 * reset works.
 */
public class TestReset02 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
	createNonPersistentCache("Foo");
	runSimpleJavaProgramWithNonPersistentCache("Foo");
	
	runPrintAllStats("Foo",false);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runResetNonPersistentCache("Foo");
	
	checkOutputContains("JVMSHRC159I","Expected message about the cache being opened: JVMSHRC159I");
	checkOutputContains("is destroyed","Expected message about the cache being destroyed: is destroyed");
	checkOutputContains("JVMSHRC158I","Expected message about the cache being created: JVMSHRC158I");
	
	
	runPrintAllStats("Foo",false);
	checkOutputDoesNotContain("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runDestroyAllCaches();
  }
}
