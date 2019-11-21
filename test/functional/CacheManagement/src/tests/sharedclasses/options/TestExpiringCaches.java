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
 * Create the four caches (2 persistent, 2 non-persistent) and check they list OK
 * then expire them, check they all go.
 */
public class TestExpiringCaches extends TestUtils {
  public static void main(String[] args) {
	  
		if (isMVS()) {

			destroyNonPersistentCache("Foo");
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			
			createNonPersistentCache("Bar");
			createNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			
			listAllCaches();
			checkOutputContains("Foo.*(no|non-persistent)",
					"Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(no|non-persistent)",
					"Did not find a non-persistent cache called Bar");
			try {
				Thread.sleep(1000);
			} catch (Exception e) {
			} // required???!?!?
			expireAllCaches();
			
			checkForSuccessfulNonPersistentCacheDestoryMessage("Bar");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
		} else {
			destroyPersistentCache("Foo");
			destroyPersistentCache("Bar");
			destroyNonPersistentCache("Foo");
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");

			createNonPersistentCache("Bar");
			createNonPersistentCache("Foo");
			createPersistentCache("Bar");
			createPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkFileExistsForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");

			listAllCaches();
			checkOutputContains("Foo.*(yes|persistent)",
					"Did not find a persistent cache called Foo");
			checkOutputContains("Foo.*(no|non-persistent)",
					"Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(yes|persistent)",
					"Did not find a persistent cache called Bar");
			checkOutputContains("Bar.*(no|non-persistent)",
					"Did not find a non-persistent cache called Bar");

			try {
				Thread.sleep(1000);
			} catch (Exception e) {
			} // required???!?!?
			expireAllCaches();

			checkForSuccessfulPersistentCacheDestroyMessage("Bar");
			checkForSuccessfulPersistentCacheDestroyMessage("Foo");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Bar");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");

			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
		}
  }
}
