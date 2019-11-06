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

package tests.sharedclasses.options.junit;

import tests.sharedclasses.options.*;

/**
 * Gather together all the tests into one testcase - see the subclasses for more info.
 */
public abstract class TestOptionsBaseRealtime extends TestOptionsNonpersistentRealtime {

	public void testCacheDir06() { TestCacheDir06.main(null);}

	public void testControlDirCacheDir01() { TestControlDirCacheDir01.main(null);}

	public void testCorruptedCaches03() { TestCorruptedCaches03.main(null);}

	public void testDestroyNonExistentCaches() { TestDestroyNonExistentCaches.main(null);}

	public void testListAllWhenNoCaches() { TestListAllWhenNoCaches.main(null);}
	
	public void testOptionNone() { TestOptionNone.main(null); }

	public void testPersistentCacheMoving01() { TestPersistentCacheMoving01.main(null);}

	public void testPersistentCacheMoving02() { TestPersistentCacheMoving02.main(null);}

	public void testPersistentCacheMoving03() { TestPersistentCacheMoving03.main(null);}

	public void testReset01() { TestReset01.main(null); }

	public void testReset04() { TestReset04.main(null); }

	public void testReset06() { TestReset06.main(null); }
	
	public void testVerboseIO() { TestVerboseIO.main(null); }
	public void testVerboseData() { TestVerboseData.main(null); }
	public void testVerboseHelper() { TestVerboseHelper.main(null); }
	public void testVerboseAOT() { TestVerboseAOT.main(null); }
	public void testSharedCacheJvmtiAPI() { TestSharedCacheJvmtiAPI.main(null); }
	public void testSharedCacheJavaAPI() { TestSharedCacheJavaAPI.main(null); }
	public void testDestroyCache() { TestDestroyCache.main(null); }
}
