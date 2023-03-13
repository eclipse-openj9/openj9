/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create persistent cache, run something, check whats in there, then reset (as a standalone action) and check
 * reset works.
 */
public class TestReset01 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    

	if (isMVS())
	{
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	}
    
    createPersistentCache("Foo");
	runSimpleJavaProgramWithPersistentCache("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runResetPersistentCache("Foo");
	checkForSuccessfulPersistentCacheCreationMessage("Foo");
	checkForSuccessfulPersistentCacheDestroyMessage("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputDoesNotContain("ROMCLASS:.*SimpleApp", "Did not expect to see SimpleApp entries in the output now the cache has been reset");

	runDestroyAllCaches();
  }
}
