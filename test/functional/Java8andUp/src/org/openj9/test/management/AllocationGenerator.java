/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
package org.openj9.test.management;

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
	public byte [] data;
}

public class AllocationGenerator extends Thread {

	static int largeAllocSize [] =        {       20,      40,  1000, 10000,  2000, 40000, 200000, 4000000 };
    static int largeAllocFrequency [] =   {    500000, 250000, 50000,  5000, 25000,  1250,    750,      37 };
    static int largeAllocKeyCount [] =    {    250000, 125000, 25000,  2500, 12500,   625,    375,      18 };

    static int largeAllocReplacement [] = {       10,      10,    10,    10,    10,    10,     10,      10 };
    static int largeAllocStart [] =       {        50,     50,     0,     0,    50,    50,     50,      50 };
    static int largeAllocStop  [] =       {     1000,    1000,    50,    50,  1000,  1000,   1000,    1000 };

    public static boolean bQuit = false;
    long _heapSize = 512*1024*1024;

	
	public AllocationGenerator() {
	}

	public AllocationGenerator(long heapSize) {
		_heapSize = heapSize;
	}
	
    public void run() {
        double largeAllocAgeFastAvg[] = new double [largeAllocSize.length];
        double largeAllocAgeSlowAvg[] = new double [largeAllocSize.length];
        long largeAllocCount [] = new long[largeAllocSize.length];

        try {
             for (int i=0; i<largeAllocSize.length; i++) {
               	largeAllocFrequency[i] = (int)(_heapSize/10/largeAllocSize[i]);
               	if ((i == 0) || (i == 1)) {
               		largeAllocFrequency[i] /= 10;
               	} else if (i>=2 && i<=5) {
               		largeAllocFrequency[i] /= 2;
               	} else {
               		int ratio = (int)(_heapSize/1000/1000/1000/2);
               		if (ratio < 1) {
               			ratio = 1;
               		}
               		largeAllocSize[i] *= ratio;
               		largeAllocFrequency[i] /= ratio;
               		largeAllocFrequency[i] = largeAllocFrequency[i] * 3 / 2;
               	}
               	largeAllocKeyCount[i] = largeAllocFrequency[i]/2;
            }
       	} catch (NumberFormatException nfe) {}
       	System.gc();
	
       	int sumOfFrequencies = 0;
       	int keyCount = 0;
       	List<HashMap<Integer, Record>> container = new ArrayList<HashMap<Integer, Record>>();
       	for (int i = 0; i < largeAllocFrequency.length; i++ ) {
       		sumOfFrequencies += largeAllocFrequency[i];
       		keyCount += largeAllocKeyCount[i];
       		container.add(i, new HashMap<Integer, Record> ());
       	}
	
       	Random randomKeyGenerator = new Random();
       	Random randomSizeGenerator = new Random();
       	Random randomReplacementGenerator = new Random();
	
       	for (long i = 0; i < keyCount * 1000L; i++) {

       		int prob = randomSizeGenerator.nextInt(sumOfFrequencies);
       		int sizeIndex = 0;
       		int sum = 0;
       		if (bQuit) {
       			break;
       		}
       		
       		for (; sizeIndex < largeAllocFrequency.length;  sizeIndex++ ) {
       			sum += largeAllocFrequency[sizeIndex];
       			if (prob < sum)
       				break;
       		}

       		/* process old entry first */
       		int key = randomKeyGenerator.nextInt(largeAllocKeyCount[sizeIndex]);
       		Record oldRecord = container.get(sizeIndex).get(key);
       		Date date = new Date();

       		/* create some transient objects, to make the workload more realistic
		   		(otherwise tilt ratio will be rather low and significant portion of long lived objects will be in Nursery) */
		
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

       		if ((i < (long)largeAllocStart[sizeIndex] * keyCount)
       				|| (i > (long)largeAllocStop[sizeIndex] * keyCount)) {
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
