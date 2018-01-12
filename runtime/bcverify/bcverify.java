/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
/* JAVACODE */

/* This Java code will generate the tables/bitmaps for start/part of java identifier unicode
   characters. */ 

package MapUnicode;

import java.io.*;
import java.util.*;

//
// @author mbottomley
//
public class mapUnicode {

	final static int VALID_START = 2;
	final static int VALID_PART = 1;
	final static int INVALID = 0;

	final static int BIT_MAP_END = 128;
	final static int RANGE_END = 65536;

	static long[] bitMapStartChars = new long[BIT_MAP_END >> 5];
	static long[] bitMapPartChars = new long[BIT_MAP_END >> 5];
	static int[] rangesStartChars = new int[1024];
	static int[] rangesPartChars = new int[512];
	static int startCharRanges, partCharRanges;

	static File cFile;
	static FileWriter cOutput;

	public static void createBitMaps() {
		int i;
		long j, k, bitPosition;
		char c;
		// Create the valid character bit maps for the first BIT_MAP_END characters
		j = 0;
		k = 0;
		bitPosition = 1;
		for (i = 0; i < BIT_MAP_END; i++) {
			c = (char) i;
			if (Character.isJavaIdentifierStart(c)) {
				j |= bitPosition;
			}
			if (Character.isJavaIdentifierPart(c)) {
				k |= bitPosition;
			}
			bitPosition <<= 1;
			if (bitPosition == 4294967296L) {
				bitMapStartChars[i >> 5] = j;
				bitMapPartChars[i >> 5] = k;
				j = 0;
				k = 0;
				bitPosition = 1;
			}
		}
	}

	public static void createStartRangeMap() {
		int i, j;
		boolean insideStartRange;
		char c;

		// Create the valid character range maps for characters BIT_MAP_END..RANGE_END - 1
		j = 0;
		insideStartRange = false;
		// Currently 0 is not a valid start character so don't expect this to execute
		if (Character.isJavaIdentifierStart((char) 0)) {
			rangesStartChars[j++] = 0;
		}
		// Add a dummy entry at the start of the table
		rangesStartChars[j++] = BIT_MAP_END - 1;
		for (i = BIT_MAP_END; i < RANGE_END; i++) {
			c = (char) i;
			// Generate the ranges for valid start characters
			// There are ~500 ranges for valid start characters
			if (Character.isJavaIdentifierStart(c)) {
				if (!insideStartRange) {
					rangesStartChars[j++] = i - 1;
					insideStartRange = true;
				}
			} else {
				if (insideStartRange) {
					rangesStartChars[j++] = i - 1;
					insideStartRange = false;
				}
			}
		}
		rangesStartChars[j++] = RANGE_END - 1;

		startCharRanges = j;
	}

	public static void createPartRangeMap() {
		int i, j;
		boolean insidePartRange;
		char c;

		// Create the valid character range maps for characters 128..65535
		j = 0;
		insidePartRange = true;
		// Since the rangesPartChars starts with a valid range, add an extra dummy entry
		// to make even numbered indexed ranges valid like the valid start character range table.
		// Expect this condition to be true
		if (Character.isJavaIdentifierPart((char) 0)) {
			rangesPartChars[j++] = 0;
		}
		rangesPartChars[j++] = BIT_MAP_END - 1;
		for (i = BIT_MAP_END; i < RANGE_END; i++) {
			c = (char) i;
			// Generate the ranges for valid part characters that are not valid start characters
			// There are fewer of these ranges than part/start ranges (~250 versus ~750)
			// This will also "glue" together part only ranges separated by start ranges
			//   (2nd condition) saving ~15 ranges
			if ((!Character.isJavaIdentifierStart(c)
				&& Character.isJavaIdentifierPart(c))
				|| (Character.isJavaIdentifierStart(c) && insidePartRange)) {
				if (!insidePartRange) {
					rangesPartChars[j++] = i - 1;
					insidePartRange = true;
				}
			} else {
				if (insidePartRange) {
					rangesPartChars[j++] = i - 1;
					insidePartRange = false;
				}
			}
		}
		rangesPartChars[j++] = RANGE_END - 1;

		partCharRanges = j;
	}

	public static void writeConstantTables() throws IOException {

		int i;

		cOutput.write("#define VALID_START " + VALID_START + "\n");
		cOutput.write("#define VALID_PART " + VALID_PART + "\n");
		cOutput.write("#define INVALID " + INVALID + "\n");
		cOutput.write("#define BIT_MAP_END " + BIT_MAP_END + "\n");
		cOutput.write("#define START_CHAR_RANGES " + startCharRanges + "\n");
		cOutput.write("#define PART_CHAR_RANGES " + partCharRanges + "\n\n");

		cOutput.write("const U_32 bitMapStartChars[] = {\n");
		for (i = 0; i < ((BIT_MAP_END >> 5) - 1); i++) {
			cOutput.write(bitMapStartChars[i] + ",\n");
		}
		// Write the last entry without a comma
		cOutput.write(bitMapStartChars[i] + "\n");
		cOutput.write("};\n\n");

		cOutput.write("const U_32 bitMapPartChars[] = {\n");
		for (i = 0; i < ((BIT_MAP_END >> 5) - 1); i++) {
			cOutput.write(bitMapPartChars[i] + ",\n");
		}
		// Write the last entry without a comma
		cOutput.write(bitMapPartChars[i] + "\n");
		cOutput.write("};\n\n");

		cOutput.write("const U_16 rangesStartChars[] = {\n");
		for (i = 0; i < (startCharRanges - 1); i++) {
			cOutput.write(rangesStartChars[i] + ",\n");
		}
		// Write the last entry without a comma
		cOutput.write(rangesStartChars[i] + "\n");
		cOutput.write("};\n\n");

		cOutput.write("const U_16 rangesPartChars[] = {\n");
		for (i = 0; i < (partCharRanges - 1); i++) {
			cOutput.write(rangesPartChars[i] + ",\n");
		}
		// Write the last entry without a comma
		cOutput.write(rangesPartChars[i] + "\n");
		cOutput.write("};\n\n");
	}

	public static int checkCharacter(int i) {

		char c;
		int searchStep, rangeIndex;
		int result = INVALID;

		c = (char) i;
		// Check the first BIT_MAP_END - optimization
		if (i < BIT_MAP_END) {
			if (((bitMapStartChars[i >> 5]) & (1 << (i & 0x1f))) != 0) {
				// GoodStartOrPart
				result = VALID_START | VALID_PART;
			} else {
				if (((bitMapPartChars[i >> 5]) & (1 << (i & 0x1f))) != 0) {
					// GoodPart
					result = VALID_PART;
				}
			}
		} else {
			// Binary search the ranges of valid start characters
			rangeIndex = startCharRanges >> 1;
			searchStep = rangeIndex;
			while (true) {
				searchStep = (searchStep + 1) >> 1;
				if (i > rangesStartChars[rangeIndex]) {
					rangeIndex += searchStep;
				} else {
					if (i <= rangesStartChars[rangeIndex - 1]) {
						rangeIndex -= searchStep;
					} else {
						if ((rangeIndex & 1) == 0) {
							// GoodStartOrPart
							result = VALID_START | VALID_PART;
						} else {
							// Not a valid start character so try the valid part characters
							rangeIndex = partCharRanges >> 1;
							searchStep = rangeIndex;
							while (true) {
								searchStep = (searchStep + 1) >> 1;
								if (i > rangesPartChars[rangeIndex]) {
									rangeIndex += searchStep;
								} else {
									if (i <= rangesPartChars[rangeIndex - 1]) {
										rangeIndex -= searchStep;
									} else {
										if ((rangeIndex & 1) == 0) {
											// GoodPart
											result = VALID_PART;
										}
										break;
									}
								}
							}
						}
						break;
					}
				}
			}
		}
		return result;
	}

	public static void main(String[] args) {
		char c;
		int i;
		int result;
		boolean fail;

		fail = false;

		createBitMaps();

		createStartRangeMap();
		createPartRangeMap();

		//Test starts
		result = 0;
		for (i = 0; i < RANGE_END; i++) {
			c = (char) i;
			result = checkCharacter(i);
			if ((result & VALID_START) == VALID_START) {
				if (!Character.isJavaIdentifierStart(c)) {
					fail = true;
					System.out.println(
						"Lookup table error? " + i + " not a valid start");
				}
			} else if ((result & VALID_PART) == VALID_PART) {
				if (!Character.isJavaIdentifierPart(c)) {
					fail = true;
					System.out.println(
						"Lookup table error? " + i + " not a valid part");
				}
			} else {
				if (Character.isJavaIdentifierPart(c)) {
					fail = true;
					System.out.println(
						"Lookup table error? "
							+ i
							+ " is a valid part and possible start");
				}
			}
		}

		if (fail == false) {
			cFile = new File("d:\\temp\\out.c");
			try {
				cOutput = new FileWriter(cFile);
				Calendar now = Calendar.getInstance();
				cOutput.write(
					"/* Autogenerated "
						+ now.get(Calendar.YEAR)
						+ "/"
						+ now.get(Calendar.MONTH)
						+ "/"
						+ now.get(Calendar.DATE)
						+ " "
						+ now.get(Calendar.HOUR)
						+ ":"
						+ now.get(Calendar.MINUTE)
						+ ":"
						+ now.get(Calendar.SECOND)
						+ " *"
						+ "/\n");
				cOutput.write(
					"/* This file is autogenerated - do not edit *");
				cOutput.write(
					"/\n\n");
				writeConstantTables();
				cOutput.close();
				System.out.println("Passed table generation");
			} catch (Exception e) {
				System.out.println("Failed table generation");
			}

		} else {
			System.out.println("Failed table generation");
		}
	}
}
