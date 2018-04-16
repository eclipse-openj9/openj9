/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
import java.util.*;

enum ParseResult {
	OPTION_OK ("ok"),
	OPTION_MISSING_VALUE ("missing value"),
	OPTION_UNRECOGNIZED ("unrecognized");

	private String message;
	ParseResult(String msg) {
		message = msg;
	}

	public String toString() {
		return message;
	}
}

public class ActiveIdleTest {
	// list of valid options
	final static String BUSY_PERIOD = "--busy-period";
	final static String IDLE_PERIOD = "--idle-period";

	// create an array of all valid options
	final static String options[] = { BUSY_PERIOD, IDLE_PERIOD };

	// variables to store option value
	static long busyPeriod = 30; // in secs
	static long idlePeriod = 300; // in secs

	static int idleToActiveCount = 1;
	static int activeToIdleCount = 1;

	public static void printUsage() {
		System.out.println("Usage: java ActiveIdleTest [" + BUSY_PERIOD + "=<secs>] [" + IDLE_PERIOD + "=<secs>]");
	}

	public static ParseResult parseArgs(String args[]) {
		String name = null;
		String value = null;
		ParseResult parseResult = ParseResult.OPTION_OK;

		for (int i = 0; i < args.length; i++) {
			int j = 0;

			name = args[i];
			value = null;
			if (args[i].indexOf('=') != -1) {
				String optionParts[] = args[i].split("=");

				name = optionParts[0];
				value = optionParts[1];
			}
			for (j = 0; j < options.length; j++) {
				if (name.equals(options[j])) {
					switch (name) {
						case BUSY_PERIOD:
							if (value != null) {
								busyPeriod = Long.valueOf(value).longValue();
							} else {
								parseResult = ParseResult.OPTION_MISSING_VALUE;
							}
							break;
						case IDLE_PERIOD:
							if (value != null) {
								idlePeriod = Long.valueOf(value).longValue();
							} else {
								parseResult = ParseResult.OPTION_MISSING_VALUE;
							}
							break;
					}
					break;
				}
			}
			if (j == options.length) {
				parseResult = ParseResult.OPTION_UNRECOGNIZED;
			}

			if (parseResult != ParseResult.OPTION_OK) {
				System.out.println("Error in parsing option " + args[i] + " : " + parseResult);
				break;
			}
		}
		return parseResult;
	}

	public static void main(String args[]) {
		ParseResult result = parseArgs(args);
		if (result != ParseResult.OPTION_OK) {
			printUsage();
			return;
		}
		busyLoop(busyPeriod);
		idleLoop(idlePeriod);
		busyLoop(busyPeriod);
	}

	public static void busyLoop(long busyPeriod) {
		System.out.println("Busy looping ... ");
		long startTime = System.nanoTime();
		do {
			ActiveWork.doWork();
		} while ((System.nanoTime() - startTime) / 1000000L < (busyPeriod * 1000));
		System.out.println("Busy loop done");
	}

	public static void idleLoop(long idlePeriod) {
		System.out.println("Idling ... ");
		long startTime = System.nanoTime();
		long sleepTime = idlePeriod * 1000;
		do {
			try {
				Thread.currentThread().sleep(sleepTime);
			} catch (Exception e) {
				/* ignored any interrupts */
			} finally {
				long elapsed = (System.nanoTime() - startTime) / 1000000L; // in msecs
				sleepTime = sleepTime - elapsed;
			}
		} while (sleepTime > 0);
		System.out.println("Idling done");
	}

	static class ActiveWork {
		static List<Long> list;

		public static void doWork() {
			int primeCount = 0;

			list = new ArrayList<>();

			// create a list of numbers
			for (int i = 0; i < 1000; i++) {
				long number = System.nanoTime() % 1000000;
				list.add(new Long(number));
			}

			// find the prime numbers in the list
			for (Long value: list) {
				long counter = 2;
				long val = value.longValue();
				boolean isPrime = true;

				for (counter = 2; counter < val; counter++) {
					if (val % counter == 0) {
						isPrime = false;
					}
				}
				if (isPrime == true) {
					primeCount += 1;
				}
			}
			System.out.println("Prime numbers in the list: " + primeCount);
		}
	}
}

