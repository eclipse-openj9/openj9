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
import j9vm.test.hashCode.generator.Lfsr;
import j9vm.test.hashCode.generator.LinCon;

public class LinConLfsrHashGenerator extends HashCodeGenerator {

	Lfsr lfsrGen;
	private LinCon linconGen;
	private boolean changedSeed = false;
	private int previousHashCode;

	public boolean seedChanged() {
		return changedSeed;
	}

	public LinConLfsrHashGenerator(int initialSeed) {
		seed = initialSeed;
		lfsrGen = new Lfsr();
		linconGen = new LinCon();
	}

	@Override
	public int processHashCode(int hashCode) {
		changedSeed = hashCode < previousHashCode;
		if (changedSeed) {
			seed = lfsrGen.scrambleSeed(linconGen.scrambleSeed(seed));
		}
		previousHashCode = hashCode;

		int lfsrHash = HashCodeGenerator.doPresquare(hashCode);
		int linConHash = hashCode;
		for (int i = 0; i < HashCodeGenerator.LINCON_CYCLES; ++i) {
			linConHash = linconGen.runLinCon(linConHash + seed);
		}

		lfsrHash = lfsrGen.runLfsr(lfsrHash, LFSR_CYCLES);
		return lfsrHash ^ linConHash;
	}

}
