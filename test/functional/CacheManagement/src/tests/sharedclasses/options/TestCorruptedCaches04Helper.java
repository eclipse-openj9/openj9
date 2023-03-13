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


import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Complex corruption.
 * This Helper class is running in a VM instance attached to a cache - the name of the
 * cache is passed in as args[0].  It then starts a second VM running attached to the
 * same cache (which should work fine).  When that completes it loads some classes itself
 * which will affect the cache, then it starts another VM attached to this same cache.
 * This reveals if there is a problem with CRC checking - it used to be that the CRC
 * was updated at the end of a VMs lifetime and checked at the start of any VMs lifetime.
 * If that were the case then this test would fail because the intermediate forName() calls
 * change the contents of the cache and would render the CRC incorrect.  The fix is to
 * mark the CRC invalid when any write occurs on the cache - thus the forname() calls will
 * end up marking the CRC invalid and the final started task wont try and check it.
 */
public class TestCorruptedCaches04Helper extends TestUtils {
  public static void main(String[] args) throws Exception {
	  RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,args[0]));

	  Class.forName("tests.sharedclasses.options.TestIncompatibleCaches01");
	  Class.forName("tests.sharedclasses.options.TestIncompatibleCaches02");
	  Class.forName("tests.sharedclasses.options.TestIncompatibleCaches03");
	  Class.forName("tests.sharedclasses.options.TestIncompatibleCaches01");
	  
	  RunCommand.execute(getCommand(RunSimpleJavaProgramWithPersistentCache,args[0]));
  }
}
