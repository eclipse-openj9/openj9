/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.fastclasshashtable;

public class FastClassHashTableTest {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		ClassLoader loader = new ClassLoader() {};
		
		try {
			/* Sleep to allow the JIT to change phase and call jvmPhaseChange */
			Thread.sleep(10000);
		} catch (InterruptedException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}
		
		try {
			/* Load enough classes to cause the native hashtable to grow, which will leave a previous hashtable to be freed later */
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy1");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy2");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy3");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy4");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy5");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy6");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy7");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy8");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy9");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy10");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy11");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy12");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy13");
			loader.loadClass("j9vm.test.fastclasshashtable.Dummy14");
			System.gc();
		} catch (ClassNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		/* On exit the JVM will call releaseExclusiveVMAccess(), which will free any previous hashtables */
	}

}
