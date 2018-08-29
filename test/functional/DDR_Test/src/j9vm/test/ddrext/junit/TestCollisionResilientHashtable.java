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
package j9vm.test.ddrext.junit;

import j9vm.test.ddrext.DDRExtTesterBase;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This test should run on a java core that was generated with hashtable backed by list and tree.
 * i.e 27+ => -XX:stringTableListToTreeThreshold=64
 * i.e 23, 24, 26 => -XXgc:stringTableListToTreeThreshold=64
 */
public class TestCollisionResilientHashtable extends DDRExtTesterBase
{

	public void testCollisionResilientHashtable() {
		String dumpStringTableOutput = exec("dumpstringtable", new String[] {});
		
		Pattern dumpStringTablePattern = Pattern.compile("!j9object (0x[0-9a-f]+) value = <(.*)>");
		Pattern searchStringTablePattern = Pattern.compile("!j9object (0x[0-9a-f]+) <(.*)>");
		Pattern dumpStringTableSummaryPattern = Pattern.compile("Table Size = (\\d+)");

		Matcher dumpStringTableMatcher = dumpStringTablePattern.matcher(dumpStringTableOutput);
		Matcher dumpStringTableSummaryMatcher = dumpStringTableSummaryPattern.matcher(dumpStringTableOutput);
		
		assertTrue("Summary string not found in the output", dumpStringTableSummaryMatcher.find());
		int stringTableSize = 0;
		
		while (dumpStringTableMatcher.find()) {
			stringTableSize += 1;
						
			String stringAddress = dumpStringTableMatcher.group(1);
			String stringValue = dumpStringTableMatcher.group(2);
			
			String searchStringTableResult = exec("searchstringtable", new String[] {stringAddress});
			
			if(searchStringTableResult.equals("Search not supported for HASH_TABLE_VERSION == 0")) {
				break; // ignore old core files, without CollisionResilientHashtable
			}
			
			Matcher searchStringTableMatcher = searchStringTablePattern.matcher(searchStringTableResult);
			assertTrue("Matching string not found: " + searchStringTableResult, searchStringTableMatcher.find());
			
			String searchStringTableAddress = searchStringTableMatcher.group(1);
			String searchStringTableValue = searchStringTableMatcher.group(2);
			
			assertEquals(stringAddress, searchStringTableAddress);			
			assertEquals(stringValue, searchStringTableValue);
		}
		assertTrue(stringTableSize > 0);
	}
}
