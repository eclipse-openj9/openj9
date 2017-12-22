/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
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
package com.ibm.j9.cfdump.tests.lineardump;

import java.io.BufferedReader;
import java.io.File;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import com.ibm.j9.cfdump.tests.lineardump.utils.Field;
import com.ibm.j9.cfdump.tests.lineardump.utils.LinearDumpTestCase;
import com.ibm.j9.cfdump.tests.utils.Utils;

/**
 * Tests for Linear ROM Class Dumper (-dr option in cfdump).
 */
@SuppressWarnings({ "nls", "static-method" })
public class LinearDumpTests extends LinearDumpTestCase {

	public static final String SECTION_START = "=== Section Start:";
	public static final String SECTION_END = "=== Section End:";

	public void testPresenceOfSections() throws Exception {
		for (Class<?> clazz : classesToTest) {
			Process cfdump = Utils.invokeCfdumpOnClass(clazz, "-dr");
			List<Field> fields = parseLinearRomClassDumperOuput(cfdump);
			assertTrue(fields.size() > 0);

			Map<String, Field> map = createFieldMap(fields);

			assertHasSection(map, "romHeader");
			assertHasSection(map, "constantPool");
			assertHasSection(map, "methods");
			assertHasSection(map, "UTF8s");
		}
	}

	private static void assertHasSection(Map<String, Field> map, String sectionName) {
		Field section = map.get(sectionName);
		assertNotNull(section);
		assertEquals("(SECTION)", section.getValue());
	}

	public void testDepthParameter() throws Exception {
		int depth = 3;
		Process cfdump = Utils.invokeCfdumpOnClass(Object.class, "-dr:" + depth);
		Stack<String> stack = new Stack<>();
		int maxDepth = 0;

		try (BufferedReader in = Utils.getBufferedErrorStreamOnly(cfdump)) {
			for (String line; (line = in.readLine()) != null;) {
				if (line.startsWith(SECTION_START)) {
					String sectionName = line.substring(SECTION_START.length() + 1);
					sectionName = sectionName.substring(0, sectionName.indexOf(' '));
					assertTrue(sectionName.length() > 0);
					stack.push(sectionName);
					// The romClass itself is not output as an enclosing section,
					// so we add 1 to the depth to implicitly account for it.
					if (stack.size() + 1 > maxDepth) {
						maxDepth = stack.size() + 1;
					}
				} else if (line.startsWith(SECTION_END)) {
					String sectionName = line.substring(SECTION_END.length() + 1);
					sectionName = sectionName.substring(0, sectionName.indexOf(' '));
					assertTrue(sectionName.length() > 0);
					assertFalse(stack.isEmpty());
					String expectedName = stack.pop();
					assertEquals(sectionName, expectedName);
				}
			}
		}

		assertEquals(depth, maxDepth);

		/* Check for 0 exit code. */
		assertEquals(0, cfdump.waitFor());
	}

	public void testInvalidClassFile() throws Exception {
		/* Invoke cfdump on itself - because it's definitely not a valid Java class file! */
		Process cfdump = Utils.invokeCfdumpOnFile(new File(Utils.getCfdumpPath()), "-dr");
		try (BufferedReader in = Utils.getBufferedErrorStreamOnly(cfdump)) {
			int numFound = 0;
			String[] stringsToFind = new String[] {
					"Invalid class file:",
					"Recommended action: throw java.lang.ClassFormatError",
					"bad magic number"
			};

			for (String line; (line = in.readLine()) != null && numFound != stringsToFind.length;) {
				/* Check if this line matches any of the expected strings - if so, set it to null. */
				for (int i = 0; i < stringsToFind.length; i++) {
					if (stringsToFind[i] != null && line.indexOf(stringsToFind[i]) != -1) {
						stringsToFind[i] = null;
						numFound++;
						break;
					}
				}
			}

			/* If we nulled out all the strings, it's a pass. */
			for (String string : stringsToFind) {
				assertNull(string);
			}
		}

		/* Check for 0 exit code. */
		assertEquals(0, cfdump.waitFor());
	}

}
