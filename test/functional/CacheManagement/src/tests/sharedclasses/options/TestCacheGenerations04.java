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

import java.util.Iterator;
import java.util.List;

import tests.sharedclasses.TestUtils;

/*
 * Create cache generations (we will have generations 1 and 3 around), run destroy on each in turn and check the right caches vanish
 */
public class TestCacheGenerations04 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();  
    
    if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
    
    createGenerations01();
    // Now have two generations of Foo and Bar (in both persistent and non-persistent forms)
    
    // Lets check that
    listAllCaches();
    List /*String*/ compatibleCaches = processOutputForCompatibleCacheList();
    // Should be at most two entries for Foo and Bar (persistent and non-persistent variants of each)
    // If there are 4 of each then generations are not being hidden
    int fooCount = 0;
    int barCount = 0;
    for (Iterator iterator = compatibleCaches.iterator(); iterator.hasNext();) {
		String name = (String) iterator.next();
		if (name.equals("Foo")) fooCount++;
		if (name.equals("Bar")) barCount++;
	}

    if (fooCount==4) fail("The generations are not being hidden, there are 6 entries for Foo in the list");
    if (barCount==4) fail("The generations are not being hidden, there are 6 entries for Bar in the list");
    if (fooCount!=2) fail("Expected to find two entries for cache 'Foo' (one persistent, one non-persistent)");
    if (barCount!=2) fail("Expected to find two entries for cache 'Bar' (one persistent, one non-persistent)");

    // Now let's start deletions
    destroyNonPersistentCache("Foo");
    listAllCaches();
    checkOutputForCompatibleCache("Foo", false, false);
    
    destroyPersistentCache("Bar");
    listAllCaches();
    checkOutputForCompatibleCache("Bar", true, false);

    destroyPersistentCache("Foo");
    listAllCaches();
    checkOutputForCompatibleCache("Foo", true, false);

    destroyNonPersistentCache("Bar");
    listAllCaches();
    checkOutputForCompatibleCache("Bar", false, false);
    
  }
}
