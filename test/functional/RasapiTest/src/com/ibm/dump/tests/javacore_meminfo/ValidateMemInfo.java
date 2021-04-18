/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.javacore_meminfo;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

/**
 * Test application that reads a javacore and validates the MEMINFO section against !dumpallsegments
 * debug extension output from a core file read by jdmpview.
 * 
 * Usage:
 * 		java com.ibm.memorycategories.ValidateMeminfo <javacore> <jdmpview !dumpallsegments output>
 * 
 * @author Richard Chamberlain
 * 
 */
public class ValidateMemInfo {

	/**
	 * main()
	 * 
	 * @param args - javacore and jdmpview !dumpallsegments output
	 */
	public static void main(String[] args) {

		if (args.length < 2) {
			System.err.println("Input javacore and jdmpview !dumpallsegments files are required.");
			System.exit(1);
		}
		String javacoreFile = args[0];
		String jdmpviewFile = args[1];

		BufferedReader in1 = null;
		BufferedReader in2 = null;
		try {
			in1 = new BufferedReader(new FileReader(javacoreFile));
			in2 = new BufferedReader(new FileReader(jdmpviewFile));
		} catch (FileNotFoundException e) {
			System.err.println("File not found");
			e.printStackTrace();
			System.exit(1);
		}
		if (in1 == null) {
			System.err.println("Failed to open javacore file");
			System.exit(1);
		}
		if (in2 == null) {
			System.err.println("Failed to open jdmpview !dumpallsegments file");
			System.exit(1);
		}

		boolean rc = compareMemInfo(in1, in2);

		if (rc == true) {
			System.err.println("Test passed.");
			System.exit(0);
		} else {
			System.err.println("Test failed.");
			System.exit(1);
		}
	}
	

	/*
	 * compareMemInfo()
	 * 
	 * @param in1 javacore
	 * @param in2 jdmpview !dumpallsegments output
	 * @return true/false for pass/fail
	 */
	private static boolean compareMemInfo(BufferedReader in1, BufferedReader in2) {
		String line;
		
		// First parse and validate the code cache memory segments in the javacore
		long jcCodeCacheTotal = 0;
		long jcCodeCacheAlloc = 0;
		long jcCodeCacheFree = 0;
		
		/* 2.6 VM and later:
		 * 1STSEGTYPE     JIT Code Cache
		 * NULL           segment            start              alloc              end                type       size
		 * 1STSEGMENT     0x0000000002C9FCA8 0x000007FFDEE80000 0x000007FFDEE80000 0x000007FFDF080000 0x00000068 0x0000000000200000
		 * 1STSEGMENT     0x0000000002CDBD58 0x000007FFDF080000 0x000007FFDF080000 0x000007FFDF280000 0x00000068 0x0000000000200000
		 * 1STSEGMENT     0x0000000002CDBCA0 0x000007FFDF280000 0x000007FFDF280000 0x000007FFDF480000 0x00000068 0x0000000000200000
		 * 1STSEGMENT     0x0000000002CDBBE8 0x000007FFDF480000 0x000007FFDF480000 0x000007FFDF680000 0x00000068 0x0000000000200000
		 * NULL
		 * 1STSEGTOTAL    Total memory:           8388608 (0x0000000000800000)
		 * 1STSEGINUSE    Total memory in use:          0 (0x0000000000000000)
		 * 1STSEGFREE     Total memory free:      8388608 (0x0000000000800000)
		 * 
		 * 2.4 VM:
		 * 1STSEGTYPE     JIT Code Cache
		 * NULL           segment          start            alloc            end               type     bytes
		 * 1STSEGMENT     000000000029E4C8 000007FFFF7A0000 000007FFFF80D9C0 000007FFFFFA0000  00000068 800000
		 */
		try {
			System.out.println("Parsing code cache memory segments in javacore:");
			line = in1.readLine();
			// Find the JIT Code Cache
			while (line != null && !line.startsWith("1STSEGTYPE     JIT Code Cache")) {
				line = in1.readLine();
			}
			
			while ((line = in1.readLine()) != null ) {
				if (line.startsWith("1STSEGMENT")) {
					String[] values = line.split("[ ]+");
					// Get the start, alloc and end values for this segment. The .substring(2) removes the 0x prfix
					long start = Long.parseLong(values[2].substring(2), 16);
					long alloc = Long.parseLong(values[3].substring(2), 16);
					long end = Long.parseLong(values[4].substring(2), 16);
					// System.out.println("seg values " + start + " " + alloc + " " + end + "\n");
					jcCodeCacheTotal += end - start; 
					jcCodeCacheAlloc += alloc - start;
					jcCodeCacheFree += end - alloc;
					continue; // Processed this line.
				} else if (line.startsWith("1STSEGTOTAL")) {
					String[] values = line.split("[ ]+");
					if (jcCodeCacheTotal == Long.parseLong(values[3], 10)) {
						System.out.println("\tcorrect total");
					} else {
						System.out.println("\tincorrect total: " + jcCodeCacheTotal + " " + Long.parseLong(values[3], 10));
					}
					continue;
				} else if (line.startsWith("1STSEGINUSE")) {
					String[] values = line.split("[ ]+");
					if (jcCodeCacheAlloc == Long.parseLong(values[5], 10)) {
						System.out.println("\tcorrect in use total");
					} else {
						System.out.println("\tincorrect in use total: " + jcCodeCacheAlloc + " " + Long.parseLong(values[5], 10));
					}
					continue;
				} else if (line.startsWith("1STSEGFREE")) {
					String[] values = line.split("[ ]+");
					if (jcCodeCacheFree == Long.parseLong(values[4], 10)) {
						System.out.println("\tcorrect free total");
					} else {
						System.out.println("\tincorrect free total: " + jcCodeCacheFree + " " + Long.parseLong(values[4], 10));
					}
					break; // we are done
				} else if (line.startsWith("NULL")) {
					// ignore
				} else {
					System.err.println("Unexpected javacore line");
					break;
				}
 			}
		} catch (IOException e) {
			System.err.println("Error reading javacore file.");
			return false;
		}

		// Now parse and validate the code cache segment information in the jdmpview output
		long jdCodeCacheTotal = 0;
		long jdCodeCacheAlloc = 0;
		long jdCodeCacheFree = 0;
		/*
		 * jit code segments - !j9memorysegmentlist 0x293e670
		 * +----------------+----------------+----------------+----------------+----------------+--------+
		 * |    segment     |     start      |    warmAlloc   |    coldAlloc   |      end       |  size  |
		 * +----------------+----------------+----------------+----------------+----------------+--------+
		 *  00000000029b9968 000007ffdee80000 000007ffdee83280 000007ffdf0602c0 000007ffdf080000   200000
		 *  0000000002946f18 000007ffdf080000 000007ffdf080020 000007ffdf260318 000007ffdf280000   200000
		 *  0000000002946e60 000007ffdf280000 000007ffdf280020 000007ffdf460318 000007ffdf480000   200000
		 *  0000000002946da8 000007ffdf480000 000007ffdf480020 000007ffdf660318 000007ffdf680000   200000
		 * +----------------+----------------+----------------+----------------+----------------+--------+
		 * Total memory:           0000000008388608 (0000000000800000)
		 * Total memory in use:    0000000000534232 (00000000000826d8)
		 * Total memory free:      0000000007854376 (000000000077d928)
		 */
		try {
			System.out.println("Parsing code cache memory segments in jdmpview output:");
			line = in2.readLine();
			// Find the JIT Code Cache
			while (line != null && !line.startsWith("jit code segments")) {
				line = in2.readLine();
			}
			
			while ((line = in2.readLine()) != null ) {
				if (line.startsWith("Total memory:")) {
					String[] values = line.split("[ ]+");
					if (jdCodeCacheTotal == Long.parseLong(values[2], 10)) {
						System.out.println("\tcorrect total");
					} else {
						System.out.println("\tincorrect total: " + jdCodeCacheTotal + " " + Long.parseLong(values[2], 10));
					}
					continue;
				} else if (line.startsWith("Total memory in use:")) {
					String[] values = line.split("[ ]+");
					if (jdCodeCacheAlloc == Long.parseLong(values[4], 10)) {
						System.out.println("\tcorrect in use total");
					} else {
						System.out.println("\tincorrect in use total: " + jdCodeCacheAlloc + " " + Long.parseLong(values[4], 10));
					}
					continue;
				} else if (line.startsWith("Total memory free:")) {
					String[] values = line.split("[ ]+");
					if (jdCodeCacheFree == Long.parseLong(values[3], 10)) {
						System.out.println("\tcorrect free total");
					} else {
						System.out.println("\tincorrect free total: " + jdCodeCacheFree + " " + Long.parseLong(values[3], 10));
					}
					break; // we are done
				} else if (line.startsWith("+")) {
					// header line, ignore
				} else if (line.startsWith("|")) {
					// header line, ignore
				} else {
					// Should be a data line. Note that the data lines have a leading space, so 'start' is at index=2 
					String[] values = line.split("[ ]+");
					long start = Long.parseLong(values[2], 16);
					long warm = Long.parseLong(values[3], 16);
					long cold = Long.parseLong(values[4], 16);
					long end = Long.parseLong(values[5], 16);
					System.out.println("\tsegment values " + start + " " + warm + " " + cold + " " + end);
					jdCodeCacheTotal += end - start; 
					jdCodeCacheAlloc += (warm - start) + (end - cold);
					jdCodeCacheFree += cold - warm;
					continue; // Processed this line.
				}
 			}
		} catch (IOException e) {
			System.err.println("Error reading jdmpview output file.");
			return false;
		}
		
		// Finally, compare the code cache segment totals from the javacore with those from the jdmpview output.
		System.out.println("Code cache (javacore) total: " + jcCodeCacheTotal + " alloc: " + jcCodeCacheAlloc + " free: " + jcCodeCacheFree);
		System.out.println("Code cache (coredump) total: " + jdCodeCacheTotal + " alloc: " + jdCodeCacheAlloc + " free: " + jdCodeCacheFree);
		if (jcCodeCacheTotal != jdCodeCacheTotal) {
			System.err.println("Test failed: code cache total memory validation failed");
			return false;
		}
		// The JIT may have allocated or freed memory in the time interval between the javacore and
		// system dumps. So allow for a +/- 10% variation between the corresponding code cache values.
		if (Math.abs(jcCodeCacheAlloc - jdCodeCacheAlloc) > jcCodeCacheTotal/10) {
			System.err.println("Test failed: code cache allocated memory validation failed");
			return false;
		}
		if (Math.abs(jcCodeCacheFree - jdCodeCacheFree) > jcCodeCacheTotal/10) {
			System.err.println("Test failed: code cache free memory validation failed");
			return false;
		}
		return true;
	}

}
