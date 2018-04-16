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

/**
 * Calculates metrics on the hashcode:
 * ascending and descending run counts, maximum and minimum, differences between consecutive values,
 * bitwise statistics etc.
 *
 */
public class HashStats {
	private static final int BUFFERSIZE = 10000;
	private static final int HISTOGRAM_SIZE = 64;
	private static final int HISTOGRAM_DIVISOR = 26;
	boolean ascending = false, descending = false;
	long ascendingRunCount = 0;
	long sumAscendingRunLength = 0;
	long descendingRunCount = 0;
	long sumDescendingRunLength = 0;
	long currentRunLength;
	int previousHash = 0;
	private int hashMax = Integer.MIN_VALUE;
	private int hashMin = Integer.MAX_VALUE;
	long sumPositiveDiffs = 0;
	long numPositiveDiffs = 0;
	long sumNegativeDiffs = 0;
	long numNegativeDiffs = 0;
	int hashList[] = new int[BUFFERSIZE];
	long histogram[] = new long[HISTOGRAM_SIZE];
	int hashCount = 0;
	private BitStatistics bitStats;
	private BitStatistics deltaBitStats;

	public HashStats() {
		bitStats = new BitStatistics();
		deltaBitStats = new BitStatistics();
	}

	/**
	 * Fold the new hashcode into the stats.
	 * @param hash new hashcode
	 */
	public void update(int hash) {
		bitStats.update(hash);
		deltaBitStats.update(hash ^ previousHash);
		if (hashMin > hash) {
			hashMin = hash;
		}
		if (hashMax < hash) {
			hashMax = hash;
		}

		if (hash > previousHash) {
			if (ascending) {
				++numPositiveDiffs;
				sumPositiveDiffs += hash - previousHash;
			}
			if (descending) {
				currentRunLength = 0;
			}
			++sumAscendingRunLength;
			if (!ascending) {
				++ascendingRunCount;
			}
			ascending = true;
			descending = false;
		} else if (hash < previousHash) {
			if (descending) {
				++numNegativeDiffs;
				sumNegativeDiffs += hash - previousHash;
			}
			if (ascending) {
				currentRunLength = 0;
			}
			if (!descending) {
				++descendingRunCount;
			}
			++sumDescendingRunLength;
			++currentRunLength;
			descending = true;
			ascending = false;

		}
		if (hashCount < BUFFERSIZE) {
			hashList[hashCount++] = hash;
		}
		int bucket = (hash >> HISTOGRAM_DIVISOR) & (HISTOGRAM_SIZE - 1);
		++histogram[bucket];
		previousHash = hash;
	}

	public long getAverageRunLength(boolean ascending) {
		long len, count;
		if (ascending) {
			len = sumAscendingRunLength;
			count = ascendingRunCount;
		} else {
			len = sumDescendingRunLength;
			count = descendingRunCount;
		}
		if (count > 0) {
			return len / count;
		} else {
			return 0;
		}
	}

	public int getHashMax() {
		return hashMax;
	}

	public int getHashMin() {
		return hashMin;
	}

	public long getAscendingRunCount() {
		return ascendingRunCount;
	}

	public long getDescendingRunCount() {
		return descendingRunCount;
	}

	public void printStats(PrintStream out) {
		out.println("Maximum hash: " + Integer.toHexString(hashMax));
		out.println("Minimum hash: " + Integer.toHexString(hashMin));
		out.println(ascendingRunCount + " ascending runs, average length "
				+ getAverageRunLength(true));
		out.println(descendingRunCount + " decending runs, average length "
				+ getAverageRunLength(false));
		if (numPositiveDiffs != 0) {
			out.println(numPositiveDiffs + " positive differences, average ="
					+ (sumPositiveDiffs / numPositiveDiffs));
		}
		if (numNegativeDiffs != 0) {
			out.println(numNegativeDiffs + " negative differences, average="
					+ (sumNegativeDiffs / numNegativeDiffs));
		}
		bitStats.printStats(out, "absolute");
		deltaBitStats.printStats(out, "relative");
		// printHistogram(out);
	}

	public void printHashValuesAndDifferences(PrintStream out) {
		for (int i = 1; i < hashCount; ++i) {
			out.println("hash value: " + hashList[i]
					+ " arithmetic difference: "
					+ (hashList[i] - hashList[i - 1])
					+ ", bitwise difference: "
					+ (Integer.toHexString(hashList[i - 1] ^ hashList[i])));
		}
	}

	public void printHistogram(PrintStream out) {
		for (int i = 1; i < HISTOGRAM_SIZE; ++i) {
			out.println("bucket: "
					+ (Integer.toHexString(i << HISTOGRAM_DIVISOR))
					+ ", count: " + histogram[i]);
		}
	}

}
