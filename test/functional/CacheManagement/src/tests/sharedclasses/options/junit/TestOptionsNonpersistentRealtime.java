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
import junit.framework.TestCase;

/**
 * A subset of tests that don't rely on any persistent cache support
 */
public class TestOptionsNonpersistentRealtime extends TestCase {

	public void testCommandLineOptionName02() { TestCommandLineOptionName02.main(null); }

	public void testCommandLineOptionName04() { TestCommandLineOptionName04.main(null); }

	public void testCommandLineOptionSilent01() { TestCommandLineOptionSilent01.main(null); }

	public void testCommandLineOptionSilent02() { TestCommandLineOptionSilent02.main(null); }

	public void testDestroyAllWhenNoCaches() { TestDestroyAllWhenNoCaches.main(null);}

	public void testPrintStats04() { TestPrintStats04.main(null); }

	public void testPrintStats05() { TestPrintStats05.main(null); }

}
