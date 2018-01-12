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
package com.ibm.j9.cfdump.tests.lineardump.utils;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9.cfdump.tests.utils.Utils;

import junit.framework.TestCase;

/**
 * Parsed field from cfdump Linear ROM Class Dumper output.
 */
@SuppressWarnings("nls")
public abstract class LinearDumpTestCase extends TestCase {

	/** Regex pattern to match fields in cfdump output. */
	private static final Pattern FIELD_PATTERN = Pattern.compile("(\\w+)-(\\w+)\\s+\\[\\s+(\\S+)\\s+(\\S+)\\s+\\](.*)");

	protected static final Class<?>[] classesToTest = new Class[] { Object.class, String.class, Class.class, ClassLoader.class };

	/**
	 * Parses cfdump output and collects a list of printed fields.
	 *
	 * @param cfdump
	 * @return
	 * @throws IOException
	 * @throws InterruptedException
	 */
	public static List<Field> parseLinearRomClassDumperOuput(Process cfdump) throws IOException, InterruptedException {
		List<Field> fields = new ArrayList<>();

		try (BufferedReader in = Utils.getBufferedErrorStreamOnly(cfdump)) {
			for (String line; (line = in.readLine()) != null;) {
				Matcher matcher = FIELD_PATTERN.matcher(line);
				if (!matcher.matches()) {
					System.out.printf("Linear dump: No match for '%s'\n", line);
				} else {
					Field field = new Field();
					field.setAddressStart(matcher.group(1));
					field.setAddressEnd(matcher.group(2));
					field.setValue(matcher.group(3));
					field.setName(matcher.group(4));
					field.setExtraDescription(matcher.group(5).trim());
					fields.add(field);
				}
			}
		}

		/* Check for 0 exit code. */
		assertEquals(0, cfdump.waitFor());

		return fields;
	}

	/**
	 * Creates a Map<String, Field> from a list of fields.
	 * Maps field names to fields.
	 *
	 * @param fields
	 * @return
	 */
	public static Map<String, Field> createFieldMap(List<Field> fields) {
		Map<String, Field> map = new HashMap<>();

		for (Field field : fields) {
			map.put(field.getName(), field);
		}

		return map;
	}

}
