/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package org.openj9.test.lworld;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" })
public class ValhallaAttributeTests {

	class MultiPreloadTest {}

	/* A class may have no more than one Preload attribute. */
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultiplePreloadAttributes() throws Throwable {
		String className = MultiPreloadTest.class.getName();
		ValhallaAttributeGenerator.generateClassWithTwoPreloadAttributes("MultiPreloadAttributes",
			new String[]{className}, new String[]{className});
	}

	class PreloadClass {}

	@Test
	static public void testPreloadBehavior() throws Throwable {
		String className = PreloadClass.class.getName();
		/* Verify PreloadClass is not loaded */
		Assert.assertNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
		/* Generate and load class that preloads PreloadClass */
		ValhallaAttributeGenerator.generateClassWithPreloadAttribute("PreloadBehavior", new String[]{className});
		/* Verify that PreloadClass is loaded */
		Assert.assertNotNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
	}

	value class PreloadValueClass {}

	@Test
	static public void testPreloadValueClassBehavior() throws Throwable {
		String className = PreloadValueClass.class.getName();
		/* Verify PreloadValueClass is not loaded */
		Assert.assertNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
		/* Generate and load class that preloads PreloadClass */
		ValhallaAttributeGenerator.generateClassWithPreloadAttribute("PreloadValueClassBehavior", new String[]{className});
		/* Verify that PreloadValueClass is loaded */
		Assert.assertNotNull(ValhallaAttributeGenerator.findLoadedTestClass(className));
	}
}
