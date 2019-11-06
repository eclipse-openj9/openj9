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
 * then destroy them one at a time, check the right files disappear as we go.
 */
public class TestCacheCreationListingDestroying02 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  
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
		if (isMVS())
		{
			checkOutputContains("Foo.*(no|non-persistent)","Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(no|non-persistent)","Did not find a non-persistent cache called Bar");
		}
		else
		{
			checkOutputContains("Foo.*(yes|persistent)","Did not find a persistent cache called Foo");
			checkOutputContains("Foo.*(no|non-persistent)","Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(yes|persistent)","Did not find a persistent cache called Bar");
			checkOutputContains("Bar.*(no|non-persistent)","Did not find a non-persistent cache called Bar");
		}
		if (isMVS())
		{
			destroyNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
		}
		else
		{
			destroyNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkFileExistsForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");
	
			destroyPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");
			
			destroyPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
			
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
		}
  }
}
