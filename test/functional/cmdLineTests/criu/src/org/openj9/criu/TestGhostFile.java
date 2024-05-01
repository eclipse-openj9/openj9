/*
 * Copyright IBM Corp. and others 2024
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
 */
package org.openj9.criu;

import org.eclipse.openj9.criu.*;

public class TestGhostFile {
	private static final long ABOVE_LIMIT = (0xFFFFFFFFL) + 1L;
	private static final long AT_LIMIT = 0xFFFFFFFFL;
	private static final long BELOW_LIMIT = 0xFFFFFFFFL - 1L;
	private static final long ZERO = 0L;
	private static final long NEGATIVE = -1L;

	public static void main(String[] args) {
		String test = args[0];

		switch (test) {
		case "aboveLimit":
			aboveLimit();
			break;
		case "atLimit":
			atLimit();
			break;
		case "belowLimit":
			belowLimit();
			break;
		case "negative":
			negative();
			break;
		case "zero":
			zero();
			break;
		default:
			throw new RuntimeException("incorrect parameters");
		}
	}

	private static void aboveLimit() {
		CRIUTestUtils.showThreadCurrentTime("Test aboveLimit() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		try {
			criu.setGhostFileLimit(ABOVE_LIMIT);
			System.out.println("TEST FAILED ");
		} catch (Throwable t) {
			if (t instanceof UnsupportedOperationException) {
				System.out.println("TEST PASSED ");
			} else {
				System.out.println("TEST FAILED ");
			}
		}
	}

	private static void atLimit() {
		CRIUTestUtils.showThreadCurrentTime("Test atLimit() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.setGhostFileLimit(AT_LIMIT);
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		System.out.println("TEST PASSED ");
		CRIUTestUtils.showThreadCurrentTime("Test atLimit() after doCheckpoint()");
	}

	private static void belowLimit() {
		CRIUTestUtils.showThreadCurrentTime("Test belowLimit() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.setGhostFileLimit(BELOW_LIMIT);
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		System.out.println("TEST PASSED ");
		CRIUTestUtils.showThreadCurrentTime("Test belowLimit() after doCheckpoint()");
	}

	private static void negative() {
		CRIUTestUtils.showThreadCurrentTime("Test negative() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		try {
			criu.setGhostFileLimit(NEGATIVE);
			System.out.println("TEST FAILED ");
		} catch (Throwable t) {
			if (t instanceof UnsupportedOperationException) {
				System.out.println("TEST PASSED ");
			} else {
				System.out.println("TEST FAILED ");
			}
		}
	}

	private static void zero() {
		CRIUTestUtils.showThreadCurrentTime("Test zero() starts ..");
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		criu.setGhostFileLimit(ZERO);
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		System.out.println("TEST PASSED ");
		CRIUTestUtils.showThreadCurrentTime("Test zero() after doCheckpoint()");
	}

}
