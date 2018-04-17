/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package org.openj9.test.gpu;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Random;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import com.ibm.cuda.CudaDevice;
import com.ibm.gpu.GPUConfigurationException;
import com.ibm.gpu.GPUSortException;
import com.ibm.gpu.Maths;

/**
 * Test sorting using com.ibm.gpu.Maths.
 */
@SuppressWarnings({ "nls", "restriction" })
public final class SortTest {

	private static final boolean DisableGC = Boolean.getBoolean("disable.system.gc");

	private static final Logger logger = Logger.getLogger(SortTest.class);

	private static final Pattern SeedPattern = makePattern("-srand", 6, "=(\\d+)");

	private static final boolean TimeJava = !Boolean.getBoolean("disable.java.timing");

	private static void addGeometric(Set<Integer> lengths, int min, int max, int count) {
		if (min <= 0) {
			throw new IllegalArgumentException("min <= 0");
		}

		if (min > max) {
			throw new IllegalArgumentException("min > max");
		}

		if (count < 2) {
			throw new IllegalArgumentException("count < 2");
		}

		double ratio = Math.pow(max / (double) min, 1.0 / (count - 1));

		for (double length = min; length < max; length *= ratio) {
			lengths.add(Integer.valueOf((int) (length + 0.5)));
		}

		lengths.add(Integer.valueOf(max));
	}

	private static void addLinear(Set<Integer> lengths, int min, int max, int count) {
		if (min > max) {
			throw new IllegalArgumentException("min > max");
		}

		if (count < 2) {
			throw new IllegalArgumentException("count < 2");
		}

		double step = (max - (double) min) / (count - 1);

		for (double length = min; length < max; length += step) {
			lengths.add(Integer.valueOf((int) (length + 0.5)));
		}

		lengths.add(Integer.valueOf(max));
	}

	private static Double elementsPerSecond(int elements, long timeNs) {
		return Double.valueOf(timeNs == 0 ? 0 : (elements * 1e9 / timeNs));
	}

	private static int getDeviceCount() {
		try {
			return com.ibm.cuda.Cuda.getDeviceCount();
		} catch (Exception e) {
			// getDeviceCount() declares but never throws CudaException
			unexpected(e);
		} catch (NoClassDefFoundError e) {
			// CUDA4J is not supported on this platform.
		}
		return 0;
	}

	private static int getLengthLimit(int deviceId, int elementSize) {
		try {
			CudaDevice device = new CudaDevice(deviceId);
			long freeMemory = device.getFreeMemory();

			return (int) Math.min(freeMemory / elementSize, Integer.MAX_VALUE);
		} catch (Exception e) {
			// If deviceId is not valid or accessible, we'll get a CudaException.
			// We don't explicitly catch CudaException because then this class
			// would fail to load.
			unexpected(e);
			return 0;
		}
	}

	private static int[] getLengths(String[] args) {
		Pattern geometric = makePattern("-geometric", 4, "=(\\d+),(\\d+),(\\d+)");
		Pattern linear = makePattern("-linear", 4, "=(\\d+),(\\d+),(\\d+)");
		Set<Integer> lengths = new HashSet<>();

		for (String arg : args) {
			try {
				Matcher matcher;

				matcher = geometric.matcher(arg);

				if (matcher.matches()) {
					addGeometric(
							lengths,
							Integer.parseInt(matcher.group(1)),
							Integer.parseInt(matcher.group(2)),
							Integer.parseInt(matcher.group(3)));
					continue;
				}

				matcher = linear.matcher(arg);

				if (matcher.matches()) {
					addLinear(
							lengths,
							Integer.parseInt(matcher.group(1)),
							Integer.parseInt(matcher.group(2)),
							Integer.parseInt(matcher.group(3)));
					continue;
				}

				if (SeedPattern.matcher(arg).matches()) {
					// ignore random seed here
					continue;
				}

				int length = Integer.parseInt(arg);

				if (length >= 0) {
					lengths.add(Integer.valueOf(length));
					continue;
				}
			} catch (NumberFormatException e) {
				// ignore
			}

			logger.warn(String.format("Argument '%s' ignored", arg));
		}

		if (lengths.isEmpty()) {
			addGeometric(lengths, 1, 1000, 16);
		}

		int[] sorted = new int[lengths.size()];
		int index = 0;

		for (Integer length : lengths) {
			sorted[index++] = length.intValue();
		}

		Arrays.sort(sorted);

		return sorted;
	}

	private static boolean isSorted(int[] data, int fromIndex, int toIndex) {
		for (int i = fromIndex; ++i < toIndex;) {
			if (data[i - 1] > data[i]) {
				return false;
			}
		}

		return true;
	}

	/**
	 * Main method for testing Cuda sort. Takes a list of length
	 * specifications. Specifications may be
	 *   -geo[metric]=first,last,count
	 *     count geometrically spaced lengths from first to last, inclusive
	 *   -lin[ear]=first,last,count
	 *     count linearly spaced lengths from first to last, inclusive
	 *   -srand=N
	 *     use N to seed random number generation
	 *   length
	 *     an individual length
	 * The combined set of lengths is sorted and tests are run in order
	 * of increasing lengths.
	 *
	 * If no args are provided, a default test will run:
	 *   -geometric=1,1000,16
	 *
	 * @param args  list of options or lengths of arrays to be sorted
	 */
	public static void main(String[] args) {
		new SortTest(args).run();
	}

	private static Pattern makePattern(String prefix, int minPrefixLength, String suffix) {
		int prefixLength = prefix.length();

		if (minPrefixLength > prefixLength) {
			throw new IllegalArgumentException("minPrefixLength");
		}

		StringBuilder pattern = new StringBuilder();

		pattern.append(prefix.substring(0, minPrefixLength));

		if (minPrefixLength < prefixLength) {
			pattern.append("(?:");

			for (int len = minPrefixLength; len <= prefixLength; ++len) {
				if (len != minPrefixLength) {
					pattern.append('|');
				}

				pattern.append(prefix.substring(minPrefixLength, len));
			}

			pattern.append(')');
		}

		pattern.append(suffix);

		return Pattern.compile(pattern.toString(), Pattern.CASE_INSENSITIVE);
	}

	private static void unexpected(Exception e) {
		Assert.fail("unexpected exception: " + e.getLocalizedMessage(), e);
	}

	private final int[] lengths;

	private final Random random;

	private final Long randomSeed;

	public SortTest() {
		this(new String[0]);
	}

	public SortTest(String[] args) {
		super();

		Long seed = null;

		for (String arg : args) {
			Matcher matcher = SeedPattern.matcher(arg);

			if (matcher.matches()) {
				seed = Long.valueOf(matcher.group(1));
			}
		}

		if (seed == null) {
			seed = Long.valueOf(new Random().nextLong());
		}

		this.lengths = getLengths(args);
		this.random = new Random(seed.longValue());
		this.randomSeed = seed;
	}

	private void run() {
		int deviceCount = getDeviceCount();

		if (deviceCount == 0) {
			logger.info("No devices found or unsupported platform.");
			return;
		}

		logger.info(String.format(
				"Random seed value: %1$d. Add -srand=%1$d to the command line to reproduce this test manually.",
				randomSeed));

		for (int deviceId = 0; deviceId < deviceCount; ++deviceId) {
			run(deviceId);
		}
	}

	private void run(int deviceId) {
		logger.info(String.format("Running sort(int[]) tests on device %d", Integer.valueOf(deviceId)));

		// warm up
		test(deviceId, 0x1_0000, false);

		logger.info(String.format("%13s %16s %16s", "Length", "Rate(GPU)", "Rate(Java)"));

		int limit = getLengthLimit(deviceId, Integer.BYTES);

		for (int length : lengths) {
			if (length > limit) {
				logger.warn(String.format("Length %d exceeds limit of %d elements.",
						Integer.valueOf(length), Integer.valueOf(limit)));
				break;
			}

			test(deviceId, length, true);
		}
	}

	/**
	 * Sorts an int array of given length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void test(int deviceId, int size, boolean logResult) {
		int[] gpuData = new int[size];

		// populate the array with random integer values
		for (int i = 0; i < size; ++i) {
			gpuData[i] = random.nextInt();
		}

		int[] javaData = TimeJava ? gpuData.clone() : null;
		long gpuDurationNs = 0;
		long javaDurationNs = 0;

		// GPU sort
		{
			if (!DisableGC) {
				System.gc();
			}

			long gpuStart = System.nanoTime();

			try {
				Maths.sortArray(deviceId, gpuData);
			} catch (GPUConfigurationException | GPUSortException e) {
				unexpected(e);
			}

			gpuDurationNs = System.nanoTime() - gpuStart;

			if (!isSorted(gpuData, 0, size)) {
				Assert.fail(String.format("sort failure (size=%d)", Integer.valueOf(size)));
			}
		}

		// Java sort
		if (TimeJava) {
			if (!DisableGC) {
				System.gc();
			}

			long javaStart = System.nanoTime();

			java.util.Arrays.sort(javaData);

			javaDurationNs = System.nanoTime() - javaStart;

			if (!Arrays.equals(gpuData, javaData)) {
				Assert.fail(String.format("sort failure (size=%d)", Integer.valueOf(size)));
			}
		}

		if (logResult) {
			String result = String.format("%13d %,16.3f %,16.3f",
					Integer.valueOf(size),
					elementsPerSecond(size, gpuDurationNs),
					elementsPerSecond(size, javaDurationNs));

			logger.info(result);
		}
	}

	@Test(groups = { "level.sanity", "level.extended" })
	public void testBasic() {
		main(new String[] { "-geometric=10,10000000,31" });
	}

	@Test(groups = { "level.sanity", "level.extended" })
	public void testPowersOf2() {
		main(new String[] { "-geometric=1,8388608,24" });
	}

}

