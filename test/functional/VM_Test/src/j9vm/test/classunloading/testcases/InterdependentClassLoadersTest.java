/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.classunloading.testcases;

import j9vm.test.classunloading.*;

/**
 *	Create two classloaders.  The classloaders are interdependent, in that each has 
 *  a superclass in the other.
 * 
 *  Hierarchy is:
 *  	Object (System)
 * 			DependentClassToLoad1 (CL1) implements InterfaceToLoad1 (CL2)
 * 				DependentClassToLoad2 (CL2) implements InterfaceToLoad2 (CL1)
 * 					DependentClassToLoad3 (CL1) implements InterfaceToLoad3 (CL2)
 * 
 * 			InterfaceToLoad1 (CL2)
 * 			InterfaceToLoad2 (CL1)
 * 			InterfaceToLoad3 (CL2)
 * 
 * See https://bugs.ottawa.ibm.com/show_bug.cgi?id=104799
 */
public class InterdependentClassLoadersTest extends ClassUnloadingTestParent {
public static void main(String[] args) throws Exception {
	new InterdependentClassLoadersTest().runTest();
}

protected String[] unloadableItems() { 
	return new String[] {
			"CL1", 
			"CL2", 
			"j9vm.test.classunloading.classestoload.DependentClassToLoad1",
			"j9vm.test.classunloading.classestoload.DependentClassToLoad2",
			"j9vm.test.classunloading.classestoload.DependentClassToLoad3",
			"j9vm.test.classunloading.classestoload.InterfaceToLoad1",
			"j9vm.test.classunloading.classestoload.InterfaceToLoad2",
			"j9vm.test.classunloading.classestoload.InterfaceToLoad3"
			};
}
protected String[] itemsToBeUnloaded() { 
	return unloadableItems();
}

protected void createScenario() throws Exception {
	FriendlyJarClassLoader cl1 = new FriendlyJarClassLoader("CL1", jarFileName, 
			new String[] {
				"j9vm.test.classunloading.classestoload.DependentClassToLoad1",
				"j9vm.test.classunloading.classestoload.DependentClassToLoad3",
				"j9vm.test.classunloading.classestoload.InterfaceToLoad2"
			});
	FriendlyJarClassLoader cl2 = new FriendlyJarClassLoader("CL2", jarFileName, 
			new String[] { 
			"j9vm.test.classunloading.classestoload.DependentClassToLoad2",
			"j9vm.test.classunloading.classestoload.InterfaceToLoad1",
			"j9vm.test.classunloading.classestoload.InterfaceToLoad3"
			});

	cl1.setFriend(cl2);
	cl2.setFriend(cl1);
	
	Class class3 = cl2.loadClass("j9vm.test.classunloading.classestoload.DependentClassToLoad3");
	//newInstance() forces clinit:
	class3.newInstance();
}

protected void afterFinalization() throws Exception {

}


}
