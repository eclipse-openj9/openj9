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
 *		Create a parent classloader with two children.  Let each classloader find a class.  Retain only
 *	a reference to one child.  The other child and the class it found are unloaded.
 *
 */
public class DiscardOneOfTwoSiblingsTest extends ClassUnloadingTestParent {
public static void main(String[] args) throws Exception {
	new DiscardOneOfTwoSiblingsTest().runTest();
}

protected String[] unloadableItems() { 
	return new String[] {"parent", "sibling1", "sibling2", "j9vm.test.classunloading.classestoload.ClassToLoad1", "j9vm.test.classunloading.classestoload.ClassToLoad2", "j9vm.test.classunloading.classestoload.ClassToLoad3"};
}
protected String[] itemsToBeUnloaded() { 
	return new String[] {"sibling2", "j9vm.test.classunloading.classestoload.ClassToLoad2"};
}

ClassLoader sibling1;

public void createScenario() throws Exception {
	ClassLoader parent = new SelectiveJarClassLoader("parent", jarFileName, "j9vm.test.classunloading.classestoload.ClassToLoad1");
	sibling1 = new SelectiveJarClassLoader("sibling1", jarFileName, parent, "j9vm.test.classunloading.classestoload.ClassToLoad3");
	ClassLoader sibling2 = new SelectiveJarClassLoader("sibling2", jarFileName, parent, "j9vm.test.classunloading.classestoload.ClassToLoad2");
	Class classLoadedBySibling1 = sibling1.loadClass("j9vm.test.classunloading.classestoload.ClassToLoad3");
	Class classLoadedBySibling2 = sibling2.loadClass("j9vm.test.classunloading.classestoload.ClassToLoad2");
	Class classLoadedByParent = classLoadedBySibling2.getSuperclass();
	//newInstance() forces clinit:
	classLoadedBySibling1.newInstance();
	classLoadedBySibling2.newInstance();
	classLoadedByParent.newInstance();
}
}
