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

import java.util.List;

import com.ibm.j9.cfdump.tests.lineardump.utils.Field;
import com.ibm.j9.cfdump.tests.lineardump.utils.LinearDumpTestCase;
import com.ibm.j9.cfdump.tests.utils.Utils;

/**
 * Tests for Linear ROM Class Dumper queries (-drq option in cfdump).
 */
@SuppressWarnings({ "nls", "static-method" })
public class QueryTests extends LinearDumpTestCase {

	public void testQueryForRomSize() throws Exception {
		for (Class<?> clazz : classesToTest) {
			Process cfdump = Utils.invokeCfdumpOnClass(clazz, "-drq:/romHeader/romSize");
			List<Field> fields = parseLinearRomClassDumperOuput(cfdump);
			assertEquals(1, fields.size());
			Field field = fields.get(0);
			assertEquals("romSize", field.getName());
			assertTrue(Integer.parseInt(field.getValue()) > 0);
		}
	}

	public void testMultipleQueries() throws Exception {
		for (Class<?> clazz : classesToTest) {
			Process cfdump = Utils.invokeCfdumpOnClass(clazz, "-drq:/romHeader/romSize,/romHeader/className");
			List<Field> fields = parseLinearRomClassDumperOuput(cfdump);
			assertEquals(2, fields.size());
			Field field = fields.get(0);
			assertEquals("romSize", field.getName());
			assertTrue(Integer.parseInt(field.getValue()) > 0);
			field = fields.get(1);
			assertEquals("className", field.getName());
			assertEquals("-> " + clazz.getName().replaceAll("\\.", "/"), field.getExtraDescription());
		}
	}

	public void testQueryArrayNotation() throws Exception {
		for (Class<?> clazz : classesToTest) {
			Process cfdump = Utils.invokeCfdumpOnClass(clazz, "-drq:/methods/method[0]/name,/methods/method[1]/name");
			List<Field> fields = parseLinearRomClassDumperOuput(cfdump);
			assertEquals(2, fields.size());
			assertEquals("name", fields.get(0).getName());
			assertEquals("name", fields.get(1).getName());
			assertTrue(fields.get(0).getExtraDescription().length() > 0);
			assertNotSame(fields.get(0).getExtraDescription(), fields.get(1).getExtraDescription());
		}
	}

	public void testQueryForNonExistentField() throws Exception {
		Process cfdump = Utils.invokeCfdumpOnClass(Object.class, "-drq:/romHeaderFooBar");
		String line = "Warning: Query '/romHeaderFooBar' matched nothing!";
		assertTrue(Utils.findLineInStream(line, Utils.getBufferedErrorStreamOnly(cfdump)));

		/* Check for 0 exit code. */
		assertEquals(0, cfdump.waitFor());
	}

	public void testInvalidQueries() throws Exception {
		assertQueryIsInvalid("blah");
		assertQueryIsInvalid("/!dsfkl");
		assertQueryIsInvalid("//");
		assertQueryIsInvalid("/[0]");
		assertQueryIsInvalid("/methods/method[x]");
		assertQueryIsInvalid("/methods/method[");
	}

	private void assertQueryIsInvalid(String query) throws Exception {
		Process cfdump = Utils.invokeCfdumpOnClass(Object.class, "-drq:" + query);
		String line = "Syntax error in query '" + query + "'";
		assertTrue(Utils.findLineInStream(line, Utils.getBufferedErrorStreamOnly(cfdump)));

		/* Check for 0 exit code. */
		assertEquals(0, cfdump.waitFor());
	}

}
