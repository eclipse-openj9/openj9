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
package j9vm.test.hashCode;

import j9vm.test.hashCode.generator.HashCodeGenerator;

public class RunHashGen {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		HashCodeGenerator gen;
		if (args.length == 0) {
			gen = selectGenerator(HashedObject.STRATEGY_LFSR1);
		} else {
			gen = selectGenerator(args[0]);

		}
		for (int i = 1; i > 0; i = i << 1) {
			int hashVal = gen.processHashCode(i);
			String outString = String.format("raw: %1$8x hash: %2$8x\n", i,
					hashVal);
			System.out.print(outString);
		}
		int oldHashVal = 0;
		for (int i = 0; i < 128; ++i) {
			int hashVal = gen.processHashCode(i);
			String outString = String
					.format("raw: %1$6d hash: %2$8x differences: raw: %3$6d hash: %4$8x\n",
							i, hashVal, i ^ (i - 1), hashVal ^ oldHashVal);
			System.out.print(outString);
			oldHashVal = hashVal;
		}
	}

	public static HashCodeGenerator selectGenerator(String hashStrategy) {
		HashCodeGenerator hashGen = null;
		if (hashStrategy.equals(HashedObject.STRATEGY_LFSR1)) {
			hashGen = new LfsrMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(HashedObject.STRATEGY_LFSRA1)) {
			hashGen = new LfsrAddingMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(HashedObject.STRATEGY_LFSR2)) {
			hashGen = new ResettingLfsrMaskGenerator(0x5e8c2a09);
		} else if (hashStrategy.equals(HashedObject.STRATEGY_RAND)) {
			hashGen = new RandHashCodeGenerator();
		} else if (hashStrategy.equals(HashedObject.STRATEGY_LINCON)) {
			hashGen = new LinConHashCodeGenerator(27182835);
		} else if (hashStrategy.equals(HashedObject.STRATEGY_LNCLFSR)) {
			hashGen = new LinConLfsrHashGenerator(27182835);
		}
		return hashGen;
	}

}
