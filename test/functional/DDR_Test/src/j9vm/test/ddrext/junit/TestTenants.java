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

public class TestTenants extends DDRExtTesterBase {

	/**
	 * Test !monitors tables
	 */
	public void testMonitorsTables() {
		String output = exec(Constants.MONITORS_CMD, new String[] { "tables" });
		
		if (output == null) {
			fail("\"!monitors tables\" output is null. Can not proceed with test");
			return;
		}
		
		assertTrue(validate(output, "Tenant 0,Tenant 1,Tenant 2", null, false));
		
		/* Now test that the string objects we expect are being waited on. */
		
		String lines[] = output.split(Constants.NL);
		
		for(int i = 0; i < lines.length; i++) {
			
			String currentLine = lines[i];
			assertTrue(validate(currentLine,"j9monitortablelistentry", "exception,error", false));
			
			if (currentLine.contains("Tenant")) {
				
				// Grab the the details for that table.
				String tableAddr = getAddressFor(currentLine, null, "j9monitortablelistentry", "\t");
				tableAddr = tableAddr.substring(0, tableAddr.indexOf('>')); // getAddressFor(...) has some issues.
				String tableOutput = exec(Constants.MONITORS_CMD, new String[] { "table", tableAddr });
				assertTrue((null != tableOutput) && (false == tableOutput.isEmpty()));
				
				// Adjust the string so it can work with getAddressFor.
				String eyecatcher = "Object monitor for ";
				tableOutput = tableOutput.substring(tableOutput.indexOf(eyecatcher) + eyecatcher.length());
				
				// Grab the object details for that table.
				String objectAddr = getAddressFor(tableOutput, null, "j9object", "\t");
				String objectOutput = exec(Constants.J9OBJECT_CMD, new String[] { objectAddr });
				
				assertTrue(validate(objectOutput,"foo",1));
				
			}
		}
	}
	
	/**
	 * Test !tenantsregions
	 */
	public void testTenantsRegions() {
		String output = exec(Constants.TENANTREGIONS_CMD, new String[] {});
		
		if (output == null) {
			fail("\"!tenantsregions\" output is null. Can not proceed with test");
			return;
		}
		
		assertTrue(validate(output, "_ROOT_TENANT_,Tenant 0,Tenant 1,Tenant 2,total", null, false));
	}
	
	/**
	 * Test !tenantcheck
	 */
	public void testTenantscheck() {
		String output = exec(Constants.TENANTCHECK_CMD, new String[] {});
		
		if (output == null) {
			fail("\"!tenantcheck\" output is null. Can not proceed with test");
			return;
		}
		
		assertTrue(validate(output, "checking class->tenant mappings,ok", null, false));
	}
}
