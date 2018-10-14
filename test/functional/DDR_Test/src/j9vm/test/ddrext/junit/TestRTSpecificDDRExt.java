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

public class TestRTSpecificDDRExt extends DDRExtTesterBase {
	public void testShrcCLStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CLSTATS });
		assertTrue(validate(statsOutput,
				Constants.SHRC_CLSTATS_SUCCESS_KEY_RTP,
				Constants.SHRC_CLSTATS_FAILURE_KEY_RTP, true));
	}

	public void testShrcCacheletExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CLSTATS });
		String lines[] = statsOutput.split(Constants.NL);
		String address = null;
		for (String line : lines) {
			if (line.contains("CACHELET !shrc")) {
				address = line.split("!shrc cachelet")[1];
			}
		}
		if (address == null) {
			fail("Can not obtain cachelet address");
		} else {
			String cacheletOutput = exec(Constants.SHRC_CMD, new String[] {
					Constants.SHRC_CACHELET, address });
			assertTrue(validate(cacheletOutput,
					Constants.SHRC_CACHELET_SUCCESS_KEY_RTP,
					Constants.SHRC_CACHELET_FAILURE_KEY_RTP, true));
		}
	}
}
