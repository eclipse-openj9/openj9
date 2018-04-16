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

import java.io.PrintStream;

public class BitStatistics {

	long sampleCount;
	long disparity[];
	long sumPopCount;

	public BitStatistics() {
		disparity = new long[32];
		sampleCount = 0;
		sumPopCount = 0;
		for (int i = 0; i < 32; ++i) {
			disparity[i] = 0;
		}
	}

	public int update(int val) {
		int popCount = 0;
		for (int i = 0; i < 32; ++i) {
			if ((val & (1 << i)) != 0) {
				++popCount;
				disparity[i] += 1;
			} else {
				disparity[i] -= 1;
			}
		}
		sumPopCount += popCount;
		++sampleCount;
		return popCount;
	}

	public void printStats(PrintStream out, String context) {
		out.print(context + " bit disparities: ");
		int wordDisparity = 0;
		int wordAbsoluteDisparity = 0;
		for (int i = 0; i < 32; ++i) {
			long bitDisparity = disparity[i];
			wordDisparity += bitDisparity;
			wordAbsoluteDisparity += (bitDisparity < 0) ? -bitDisparity
					: bitDisparity;
			out.print(bitDisparity + ", ");
		}
		out.print('\n');
		String result = String.format(
				"%1$10s disparity: %2$8d average population count %3$3d",
				context, wordAbsoluteDisparity, sumPopCount / sampleCount);
		out.println(result);
		if (false) {
			out.print("\n" + context + " word disparity: " + wordDisparity);
			out.print(" word absolute disparity: " + wordAbsoluteDisparity);
			out.println(" average population count: " + sumPopCount
					/ sampleCount);
		}
	}
}
