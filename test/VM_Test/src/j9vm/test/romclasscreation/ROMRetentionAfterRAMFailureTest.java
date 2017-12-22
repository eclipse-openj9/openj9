package j9vm.test.romclasscreation;

/*******************************************************************************
 * Copyright (c) 2010, 2012 IBM Corp. and others
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

/**
 * This test checks that we do not leak ROM classes when RAM
 * class creation fails. Instead, the next request to load
 * this class should result in a re-use of the previously
 * created ROM class.
 * 
 * @author Alexei Svitkine
 *
 */
public class ROMRetentionAfterRAMFailureTest {
	/**
	 * ParentlessChild is a class that is expected to be on the classpath
	 * when the test is run. The class should be a subclass of another
	 * class that is not on the classpath. There are no other requirements
     * on the class.
	 * 
	 * Currently, the compiled class is at VM/j9vm/data/ParentlessChild.class
     * This was easier than changing the ant script to remove the parent class
     * before jarring.
	 */
	public static final String CLASS_TO_LOAD = "ParentlessChild";
	public static final int ATTEMPTS = 10;

	public static void main(String[] args) {
		new ROMRetentionAfterRAMFailureTest().test();
	}

	private void test() {
		for (int i = 0; i < ATTEMPTS; i++) {
			try {
				Class.forName(CLASS_TO_LOAD);
				System.err.println("Error: Class.forName(" + CLASS_TO_LOAD + ") should not have succeeded!");
			} catch (java.lang.ClassNotFoundException exc) {
			} catch (java.lang.NoClassDefFoundError err) {
			}
		}
	}
}
