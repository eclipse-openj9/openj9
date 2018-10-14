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

package org.openj9.test.modularity.tests;

import java.lang.module.Configuration;
import java.lang.module.ModuleFinder;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Optional;
import java.util.ServiceLoader;
import java.util.Set;

import org.openj9.test.modularity.common.VisibilityTester;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import static org.testng.Assert.fail;
import static org.testng.Assert.assertTrue;


@Test(groups = { "level.extended" })

public class MethodVisibilityTests {

	private static final String ORG_OPENJ9TEST_MODULARITY = "org.openj9test.modularity";
	/* Note: module name components should avoid terminal digits. */
	private static final String MODULE_TEST = ORG_OPENJ9TEST_MODULARITY + ".testerModule";
	public static final String MODULE_A = ORG_OPENJ9TEST_MODULARITY + ".moduleA";
	public static final String MODULE_B = ORG_OPENJ9TEST_MODULARITY + ".moduleB";
	public static final String MODULE_C = ORG_OPENJ9TEST_MODULARITY + ".moduleC";
	private static final String visibilityTesterFullyQualifiedName = 
			"org.openj9.test.modularity.tests.VisibilityTesterImpl";
	private static final String MODULARITY_CODE = 
			System.getProperty("org.openj9.test.module.path");
	static Logger logger = Logger.getLogger(MethodVisibilityTests.class);


	/*
	 * Test access to types in a target module from a consumer module via:
	 * 	- fromMethodDescriptorString()
	 * 	- Lookup.findVirtual()
	 * 	- ldc method handle with test types as arguments or return type
	 * for various combinations of module visibility:
	 * 	- target exports, opens, or grants no access to a package.
	 *  - consumer requires, requires transitively via another module, or does not require the package
	 *  
	 *  testerModule requires moduleA and moduleB.
	 *  moduleA exports pkgAe, opens pkgAo, and does not export pkgAr.
	 *  moduleA requires moduleB transitively.
	 *  moduleB exports pkgBe, opens pkgBo, and does not export pkgBr. 
	 *  moduleB requires moduleC non-transitively.
	 *  moduleC exports pkgCe, opens pkgCr, and does not export pkgCr.
	 *  moduleD exports pkgDe, opens pkgDr, and does not export pkgDr. No packages require Module D.
	 */

	public static void main(String[] args) {
		MethodVisibilityTests testObject = new MethodVisibilityTests();
		try {
			testObject.testClassForNameAndFindVirtual();
			testObject.testFindStatic();
			testObject.testFromMethodDescriptorString();
			testObject.testFindClass();
			testObject.testMethodHandle();
		} catch (Throwable e) {
			e.printStackTrace();
		}
	}

	@Test(groups = { "level.extended" })
	public void testClassForNameAndFindVirtual()  {
		VisibilityTester myTester = setUp();
		myTester.testClassForNameAndFindVirtual();
	}

	@Test(groups = { "level.extended" })
	public void testFindStatic()  {
		VisibilityTester myTester = setUp();
		myTester.testFindStatic();
	}

	@Test(groups = { "level.extended" })
	public void testFromMethodDescriptorString() {
		VisibilityTester myTester = setUp();
		myTester.testFromMethodDesc();
	}

	@Test(groups = { "level.extended" })
	public void testFindClass() {
		VisibilityTester myTester = setUp();
		myTester.testFindClass();
	}

	@Test(groups = { "level.extended" })
	public void testMethodHandle() {
		VisibilityTester myTester = setUp();
		myTester.testLoadMethodHandle();
	}

	VisibilityTester setUp()  {
		Set<String> moduleSet = Set.of(MODULE_TEST, MODULE_A, MODULE_B);
		ArrayList<Path> modulePaths = new ArrayList<>();
		for (String moduleDirectory: moduleSet) {
			String moduleDirectoryString = MODULARITY_CODE + "/" + moduleDirectory;
			Path path = Paths.get(moduleDirectoryString);
			modulePaths.add(path);
		}
		Path modulePathArray[] = new Path[modulePaths.size()];
		modulePaths.toArray(modulePathArray);
		ModuleFinder finder = ModuleFinder.of(modulePathArray);

		ModuleLayer parent = ModuleLayer.boot();

		Configuration cf = parent.configuration().resolve(finder, ModuleFinder.of(), 
				moduleSet);

		ClassLoader systemLoader = ClassLoader.getSystemClassLoader();
		ModuleLayer layer = parent.defineModulesWithOneLoader(cf, systemLoader);
		try {
		Class<VisibilityTester> c;
			ClassLoader layerLoader = layer.findLoader(MODULE_TEST);
			c = (Class<VisibilityTester>) layerLoader.
					loadClass(visibilityTesterFullyQualifiedName);
			Constructor<VisibilityTester> declaredConstructor = c.getDeclaredConstructor();
			VisibilityTester myTester = declaredConstructor.newInstance();
			return myTester;
		} catch (ClassNotFoundException | NoSuchMethodException | SecurityException | InstantiationException | IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
			fail("Cannot create tester class", e);
			return null;
		}
	}

}
