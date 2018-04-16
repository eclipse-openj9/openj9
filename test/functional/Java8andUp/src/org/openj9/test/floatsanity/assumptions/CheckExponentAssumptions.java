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
package org.openj9.test.floatsanity.assumptions;

import java.util.ArrayList;

import org.openj9.test.floatsanity.D;
import org.openj9.test.floatsanity.ED;
import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckExponentAssumptions {

	public static Logger logger = Logger.getLogger(CheckExponentAssumptions.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check exponent double assumptions");
	}

	private ArrayList<Object[]> generator() {
		ArrayList<Object[]> tests = new ArrayList<>();

		for (int i = 0; i < ED.values.length; i++) {
			tests.add(new Object[] { "bases: ED.p" + (i + 1), ED.values[i], valuebits[i] });
			tests.add(new Object[] { "bases: ED.n" + (i + 1), ED.nvalues[i], nvaluebits[i] });
		}

		for (int i = 0; i < ED.squares.length; i++) {
			tests.add(new Object[] { "squares: ED.p" + (i + 1) + "P2", ED.squares[i], squarebits[i] });
		}

		for (int i = 0; i < ED.cubes.length; i++) {
			tests.add(new Object[] { "cubes: ED.p" + (i + 1) + "P3", ED.cubes[i], cubebits[i] });
			tests.add(new Object[] { "cubes: ED.n" + (i + 1) + "P3", ED.ncubes[i], ncubebits[i] });
		}
		for (int i = 0; i < ED.squareroots.length; i++) {
			tests.add(new Object[] { "sqrt: ED.p" + (i + 1) + "R2", ED.squareroots[i], squarerootbits[i] });
		}
		for (int i = 0; i < ED.logvalues.length; i++) {
			tests.add(new Object[] { "logs: ED.plog" + (i + 1), ED.logvalues[i], logvaluebits[i] });
		}
		for (int i = 0; i < ED.expvalues.length; i++) {
			tests.add(new Object[] { "exp: ED.pEP" + (i + 1), ED.expvalues[i], expvaluebits[i] });
		}

		return tests;
	}

	public void exponent_double_assumptions() {
		ArrayList<Object[]> testCases = generator();
		for (Object[] args : testCases) {
			String name = (String)args[0];
			double assumed = (double)args[1];
			long expectedLong = (long)args[2];
			double expected = D.L2D(expectedLong);
			String operation = "checking assumption " + name + " is equal to 0x" + Long.toHexString(expectedLong);
			logger.debug(operation);
			if (Double.isNaN(expected)) {
				Assert.assertTrue(Double.isNaN(assumed), operation);
			} else {
				Assert.assertEquals(assumed, expected, 0, operation);
			}
		}
	}

	static final long[] valuebits = {
		0x3FF0000000000000L, 0x4000000000000000L, 0x4008000000000000L, 0x4010000000000000L,
		0x4014000000000000L, 0x4018000000000000L, 0x401C000000000000L, 0x4020000000000000L,
		0x4022000000000000L, 0x4024000000000000L, 0x4026000000000000L, 0x4028000000000000L,
		0x402A000000000000L, 0x402C000000000000L, 0x402E000000000000L, 0x4030000000000000L
	};
	static final long[] nvaluebits = {
		0xBFF0000000000000L, 0xC000000000000000L, 0xC008000000000000L, 0xC010000000000000L,
		0xC014000000000000L, 0xC018000000000000L, 0xC01C000000000000L, 0xC020000000000000L,
		0xC022000000000000L, 0xC024000000000000L, 0xC026000000000000L, 0xC028000000000000L,
		0xC02A000000000000L, 0xC02C000000000000L, 0xC02E000000000000L, 0xC030000000000000L
	};
	static final long[] squarebits = {
		0x3FF0000000000000L, 0x4010000000000000L, 0x4022000000000000L, 0x4030000000000000L,
		0x4039000000000000L, 0x4042000000000000L, 0x4048800000000000L, 0x4050000000000000L,
		0x4054400000000000L, 0x4059000000000000L, 0x405E400000000000L, 0x4062000000000000L,
		0x4065200000000000L, 0x4068800000000000L, 0x406C200000000000L, 0x4070000000000000L
	};
	static final long[] cubebits = {
		0x3FF0000000000000L, 0x4020000000000000L, 0x403B000000000000L, 0x4050000000000000L,
		0x405F400000000000L, 0x406B000000000000L, 0x4075700000000000L, 0x4080000000000000L,
		0x4086C80000000000L, 0x408F400000000000L, 0x4094CC0000000000L, 0x409B000000000000L,
		0x40A12A0000000000L, 0x40A5700000000000L, 0x40AA5E0000000000L, 0x40B0000000000000L
	};
	static final long[] ncubebits = {
		0xBFF0000000000000L, 0xC020000000000000L, 0xC03B000000000000L, 0xC050000000000000L,
		0xC05F400000000000L, 0xC06B000000000000L, 0xC075700000000000L, 0xC080000000000000L,
		0xC086C80000000000L, 0xC08F400000000000L, 0xC094CC0000000000L, 0xC09B000000000000L,
		0xC0A12A0000000000L, 0xC0A5700000000000L, 0xC0AA5E0000000000L, 0xC0B0000000000000L
	};
	static final long[] squarerootbits = {
		0x3FF0000000000000L, 0x3FF6A09E667F3BCDL, 0x3FFBB67AE8584CAAL, 0x4000000000000000L,
		0x4001E3779B97F4A8L, 0x4003988E1409212EL, 0x40052A7FA9D2F8EAL, 0x4006A09E667F3BCDL,
		0x4008000000000000L, 0x40094C583ADA5B53L, 0x400A887293FD6F34L, 0x400BB67AE8584CAAL,
		0x400CD82B446159F3L, 0x400DEEEA11683F49L, 0x400EFBDEB14f4EDAL, 0x4010000000000000L
	};
	static final long[] logvaluebits = {
		0x0000000000000000L, 0x3FE62E42FEFA39EFL, 0x3FF193EA7AAD030BL, 0x3FF62E42FEFA39EFL,
		0x3FF9C041F7ED8D33L, 0x3FFCAB0BFA2A2002L, 0x3FFF2272AE325A57L, 0x4000A2B23F3BAB73L,
		0x400193EA7AAD030BL, 0x40026BB1BBB55516L, 0x40032EE3B77F374CL, 0x4003E116BCD39E7DL,
		0x400485042B318C51L, 0x40051CCA16D7BBA7L, 0x4005AA16394D481FL, 0x40062E42FEFA39EFL
	};
	static final long[] expvaluebits = {
		0x4005BF0A8B145769L, 0x401D8E64B8D4DDAEL, 0x403415E5BF6FB106L, 0x404B4C902E273A58L,
		0x40628D389970338FL, 0x407936DC5690C08FL, 0x409122885AAEDDAAL, 0x40A749EA7D470C6EL,
		0x40BFA7157C470F82L, 0x40D5829DCF950560L, 0x40ED3C4488EE4F7FL, 0x4103DE1654D37C9AL,
		0x411B00B5916AC955L, 0x413259AC48BF05D7L, 0x4148F0CCAFAD2A87L, 0x4160F2EBD0A80020L
	};
}
