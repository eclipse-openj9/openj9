/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.nogc;

import java.util.HashMap;
import java.util.Random;
import java.util.Date;
import java.util.List;
import java.util.ArrayList;

class Record {

	public Record(int sI, int size, long tS) {
		timeStamp = tS;
		data = new byte[size];
	}

	public long timeStamp;
	public byte[] data;
}

public class Allocator extends Thread {

	static int largeAllocSize[] =      {     20,     40,  1000, 10000,  2000, 40000 };
	static int largeAllocFrequency[] = { 250000, 125000, 25000,  2500, 12500,   625 };
	static int largeAllocKeyCount[] =  { 125000,  62500, 12500,  1250,  6250,   312 };

	static int largeAllocReplacement[] = { 5, 5, 5, 5, 5, 5 };
	static int largeAllocStart[] = { 10, 10,  0,  0, 10, 10 };
	static int largeAllocStop[] = {  20, 20, 10, 10, 20, 20 };

	public static boolean bQuit = false;

	public Allocator() {
	}

	public void run() {
		double largeAllocAgeFastAvg[] = new double[largeAllocSize.length];
		double largeAllocAgeSlowAvg[] = new double[largeAllocSize.length];
		long largeAllocCount[] = new long[largeAllocSize.length];
		int cycleCount = 20;

		System.gc();

		int sumOfFrequencies = 0;
		int keyCount = 0;
		List<HashMap<Integer, Record>> container = new ArrayList<HashMap<Integer, Record>>();
		for (int i = 0; i < largeAllocFrequency.length; i++) {
			sumOfFrequencies += largeAllocFrequency[i];
			keyCount += largeAllocKeyCount[i];
			container.add(i, new HashMap<Integer, Record>());
		}

		Random randomKeyGenerator = new Random();
		Random randomSizeGenerator = new Random();
		Random randomReplacementGenerator = new Random();

		for (long i = 0; i < keyCount * cycleCount; i++) {

			int prob = randomSizeGenerator.nextInt(sumOfFrequencies);
			int sizeIndex = 0;
			int sum = 0;
			if (bQuit) {
				break;
			}

			for (; sizeIndex < largeAllocFrequency.length; sizeIndex++) {
				sum += largeAllocFrequency[sizeIndex];
				if (prob < sum)
					break;
			}

			/* process old entry first */
			int key = randomKeyGenerator.nextInt(largeAllocKeyCount[sizeIndex]);
			Record oldRecord = container.get(sizeIndex).get(key);
			Date date = new Date();

			/*
			 * create some transient objects, to make the workload more realistic (otherwise
			 * tilt ratio will be rather low and significant portion of long lived objects
			 * will be in Nursery)
			 */

			if (null != oldRecord) {
				/* find if we are replacing this entry */
				int replacementRatio = largeAllocReplacement[sizeIndex];
				if (randomReplacementGenerator.nextInt(100) >= replacementRatio) {
					continue;
				}

				/* yes, we are. find how long it lived */
				long oldRecordTS = oldRecord.timeStamp;
				long age = date.getTime() - oldRecordTS;

				largeAllocAgeFastAvg[sizeIndex] = largeAllocAgeFastAvg[sizeIndex] * 0.0 + age * 1.0;
				largeAllocAgeSlowAvg[sizeIndex] = largeAllocAgeSlowAvg[sizeIndex] * 0.9999 + age * 0.0001;

				if (0 == (i % sumOfFrequencies)) {
					long totalBytes = 0;
					long totalCount = 0;
					long containterTotalSize = 0;
					for (int j = 0; j < largeAllocSize.length; j++) {
						totalBytes += largeAllocSize[j] * largeAllocCount[j];
						totalCount += largeAllocCount[j];
						containterTotalSize += container.get(j).size();
					}
				}

				largeAllocCount[sizeIndex] -= 1;
			}

			/* adding new entry. find sizeIndex */

			int size = largeAllocSize[sizeIndex];
			Record record = new Record(sizeIndex, size, date.getTime());

			if ((i < (long) largeAllocStart[sizeIndex] * keyCount)
					|| (i > (long) largeAllocStop[sizeIndex] * keyCount)) {
				// this is ugly: we are not replacing old Index; revert back that counter
				if (null != oldRecord) {
					largeAllocCount[sizeIndex] += 1;
				}
				continue;
			}

			container.get(sizeIndex).put(key, record);
			largeAllocCount[sizeIndex] += 1;
		}
	}
}
