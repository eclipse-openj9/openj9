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

import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;
import j9vm.test.ddrext.util.parser.ParserUtil;

public class TestTypeResolution extends DDRExtTesterBase
{
	
	public void testSubtypeDetection()
	{
		String output = exec(Constants.DUMP_ALL_REGIONS_CMD, new String[] {});
		
		if (output == null) {
			fail("\"!dumpallregions\" output is null. Cannot proceed with test!");
			return;
		}
		
		String regionAddr = getFirstRegionAddress(output);
		
		if (regionAddr == null) {
			fail("\"!dumpallregions\" output is malformed. Cannot proceed with test!");
			return;
		}
		
		// Test a subtype of MM_BaseVirtual:
		output = exec(Constants.DUMP_HRD_CMD, new String[] {regionAddr});
		assertTrue(validate(output, Constants.TYPERES_HRD_SUCCESS_KEYS, Constants.TYPERES_HRD_FAILURE_KEYS, false));
		
		String subspaceCmd  = ParserUtil.getFieldAddressOrValue(Constants.TYPERES_SUBSPACE_STRUCT_CMD, null, output);
		String subspaceAddr = ParserUtil.getFieldAddressOrValue(Constants.TYPERES_SUBSPACE_STRUCT_CMD, Constants.TYPERES_SUBSPACE_STRUCT_CMD, output);
		
		// Check that we actually got a subtype (i.e. more specific type name, or same for older cores)
		assertTrue(subspaceCmd.toLowerCase().indexOf(Constants.TYPERES_SUBSPACE_STRUCT_CMD) >= 0);
	
		output = exec(subspaceCmd, new String[] { subspaceAddr });
		
		// Test a subtype of MM_BaseNonVirtual:
		String lockCmd = ParserUtil.getFieldAddressOrValue(Constants.TYEPERES_LOCK_STRUCT_CMD, null, output);
		String lockAddr = ParserUtil.getFieldAddressOrValue(Constants.TYEPERES_LOCK_STRUCT_CMD, Constants.TYEPERES_LOCK_STRUCT_CMD, output);
		
		output = exec(lockCmd, new String[] { lockAddr });
		
		assertTrue(validate(output, Constants.TYPERES_LOCK_SUCCESS_KEYS, Constants.TYPERES_LOCK_FAILURE_KEYS, false));
		
	}
	
	
	private String getFirstRegionAddress(String output)
	{
		String[] lines = output.split("\n");
		if (lines.length > 2) {
			String firstRegionLine = lines[3].trim();
			int endIdx = firstRegionLine.indexOf(' ');
			if (endIdx <= 0) {
				return null;
			}
			String firstRegionAddr = firstRegionLine.substring(0,endIdx);
			return "0x" + firstRegionAddr;
		}
		return null;
	}
	
}
