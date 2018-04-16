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
package j9vm.test.hashCode.generator;

import j9vm.test.hashCode.HashTest;

/**
 * Superclass for hashcode generators
 *
 */
public abstract class HashCodeGenerator {

	/**
	 * Salt value to add entropy to the hashcode.
	 */
	protected int seed;
	public static final String PROPERTY_LFSR_CYCLES = HashTest.PROPERTY_HASHTEST_PREFIX
			+ "lfsrcycles";
	public static final String PROPERTY_LINCON_CYCLES = HashTest.PROPERTY_HASHTEST_PREFIX
			+ "linconcycles";
	public static final String PROPERTY_PRESQUARE = HashTest.PROPERTY_HASHTEST_PREFIX
			+ "presquare";
	public static int LFSR_MASK_CYCLES = Integer.getInteger(
			HashTest.PROPERTY_HASHTEST_PREFIX + "lfsrmaskcycles", 1);
	public static int LINCON_CYCLES = Integer.getInteger(
			HashCodeGenerator.PROPERTY_LINCON_CYCLES, 1);
	public static int LFSR_CYCLES = Integer.getInteger(
			HashCodeGenerator.PROPERTY_LFSR_CYCLES, 8);
	public static boolean HASH_PRESQUARE = Boolean
			.getBoolean(HashCodeGenerator.PROPERTY_PRESQUARE);

	public abstract int processHashCode(int hashCode);

	public abstract boolean seedChanged();

	public int getSeed() {
		return seed;
	}

	public void setSeed(int newSeed) {
		seed = newSeed;
	}

	public static int doPresquare(int initial) {
		int result;
		if (HASH_PRESQUARE) {
			int sq = initial & 0xffff;
			result = initial + (sq * sq);
			return result;
		} else {
			return initial;
		}
	}

}
