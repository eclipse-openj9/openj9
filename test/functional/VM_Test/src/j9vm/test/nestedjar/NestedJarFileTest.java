/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package j9vm.test.nestedJar;

import java.net.URL;
import j9vm.test.invalidclasspath.SetClasspathTest.CustomClassLoader;

public class NestedJarFileTest {
	private static final String MSG_PREFIX = NestedJarFileTest.class.getSimpleName() + "> ";
	
	public static void main(String args[]) throws Exception {
		URL nestedURLs[] = new URL[] {
			/* The urls are jar:file:/Path_To_VM_Test/VM_Test.jar!/InvalidClasspathResource1.jar
			 *				jar:file:/Path_To_VM_Test/VM_Test.jar!/InvalidClasspathResource2.jar
			 *				jar:file:/Path_To_VM_Test/VM_Test.jar!/InvalidClasspathResource3.jar
			 */
			NestedJarFileTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
			NestedJarFileTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
			NestedJarFileTest.class.getClassLoader().getResource("InvalidClasspathResource3.jar"),
		};

		CustomClassLoader loader = new CustomClassLoader(nestedURLs);
		
		if (args[0].equals("load3Classes")) {
			/* TestA1 is from VM_Test.jar!/InvalidClasspathResource1.jar */
			Class.forName("TestA1", true, loader);
			System.err.println("\n" + MSG_PREFIX + "loadded TestA1");
			
			/* TestA2 is from VM_Test.jar!/InvalidClasspathResource1.jar */
			Class.forName("TestA2", true, loader);
			System.err.println("\n" + MSG_PREFIX + "loadded TestA2");
		
			/* TestC1 is from VM_Test.jar!/InvalidClasspathResource3.jar */
			Class.forName("TestC1", true, loader);
			System.err.println("\n" + MSG_PREFIX + "loadded TestC1");
		}
		if (args[0].equals("load1Class")) {
			/* TestC1 is from VM_Test.jar!/InvalidClasspathResource2.jar */
			Class.forName("TestB1", true, loader);
			System.err.println("\n" + MSG_PREFIX + "loadded TestB1");
		}
	}
}
