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
package j9vm.test.ddrext.util.parser;

import java.util.regex.Pattern;

import j9vm.test.ddrext.Constants;

/**
 * This class is used to extract info from !j9pool <address> DDR extension output.
 *
 * @author fkaraman
 */
public class J9PoolOutputParser {

	private static final Pattern PUDDLELIST_SIGNATURE = Pattern.compile("J9WSRP\\((struct )?J9PoolPuddleList\\) puddleList");

	/**
	 * This method finds the address of j9poolpuddlelist from !j9pool output
	 * @param j9poolOutput
	 * @return puddleList address or null
	 */
	public static String getPuddleListAddress(String j9poolOutput) {
		return ParserUtil.getFieldAddressOrValue(PUDDLELIST_SIGNATURE, Constants.J9POOLPUDDLELIST_CMD, j9poolOutput);
	}

}
