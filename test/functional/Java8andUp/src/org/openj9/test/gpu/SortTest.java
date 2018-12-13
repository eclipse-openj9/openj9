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
import java.util.LinkedHashSet;
import java.util.Random;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import com.ibm.cuda.CudaDevice;
import com.ibm.cuda.CudaException;
import com.ibm.gpu.GPUConfigurationException;
import com.ibm.gpu.GPUSortException;
import com.ibm.gpu.Maths;

/**
 * Test sorting using com.ibm.gpu.Maths.
 */
@SuppressWarnings({ "nls", "static-method" })
public final class SortTest {

	private static enum ElementType {

		DOUBLE("double", Double.BYTES),

		FLOAT("float", Float.BYTES),

		INT("int", Integer.BYTES),

		LONG("long", Long.BYTES);

		final String name;

		final int size;

		private ElementType(String name, int size) {
			this.name = name;
			this.size = size;
		}

	}

	private static final boolean DisableGC = Boolean.getBoolean("disable.system.gc");

	private static final boolean DisableJavaTiming = Boolean.getBoolean("disable.java.timing");

	private static final Logger logger = Logger.getLogger(SortTest.class);

	private static final Pattern SeedPattern = makePattern("-srand", 6, "=(-?\\d+)");

	private static final Pattern TypesPattern = makePattern("-types", 5, "=(\\S+)");

	private static void addGeometric(Set<Integer> lengths, int min, int max, int count) {
		checkArgs(min, max, count);

		double ratio = Math.pow(max / (double) min, 1.0 / (count - 1));

		for (double length = min; length < max; length *= ratio) {
			lengths.add(Integer.valueOf((int) (length + 0.5)));
		}

		lengths.add(Integer.valueOf(max));
	}

	private static void addLinear(Set<Integer> lengths, int min, int max, int count) {
		checkArgs(min, max, count);

		double step = (max - (double) min) / (count - 1);

		for (double length = min; length < max; length += step) {
			lengths.add(Integer.valueOf((int) (length + 0.5)));
		}

		lengths.add(Integer.valueOf(max));
	}

	private static void addTypes(Set<ElementType> types, String typeList) {
		for (String type : typeList.split(",")) {
			switch (type) {
			case "all":
				types.addAll(Arrays.asList(ElementType.values()));
				break;
			case "double":
				types.add(ElementType.DOUBLE);
				break;
			case "float":
				types.add(ElementType.FLOAT);
				break;
			case "int":
				types.add(ElementType.INT);
				break;
			case "long":
				types.add(ElementType.LONG);
				break;
			default:
				throw new IllegalArgumentException("unsupported type: " + type);
			}
		}
	}

	private static void checkArgs(int min, int max, int count) {
		if (min <= 0) {
			throw new IllegalArgumentException("min <= 0");
		}

		if (min > max) {
			throw new IllegalArgumentException("min > max");
		}

		if (count < 2) {
			throw new IllegalArgumentException("count < 2");
		}
	}

	private static Double elementsPerSecond(int elements, long timeNs) {
		return Double.valueOf(timeNs == 0 ? 0 : (elements * 1e9 / timeNs));
	}

	private static int getDeviceCount() {
		try {
			return com.ibm.cuda.Cuda.getDeviceCount();
		} catch (CudaException e) {
			// getDeviceCount() declares but never throws CudaException
			unexpected(e);
			return 0;
		}
	}

	private static int getLengthLimit(int deviceId, int elementSize) {
		try {
			CudaDevice device = new CudaDevice(deviceId);
			long freeMemory = device.getFreeMemory();

			return (int) Math.min(freeMemory / elementSize, Integer.MAX_VALUE);
		} catch (CudaException e) {
			// If deviceId is not valid or accessible, we'll get a CudaException.
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

				if (SeedPattern.matcher(arg).matches() || TypesPattern.matcher(arg).matches()) {
					// ignore non-length options here
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

	private static boolean isSorted(double[] data, int fromIndex, int toIndex) {
		for (int i = fromIndex; ++i < toIndex;) {
			if (data[i - 1] > data[i]) {
				return false;
			}
		}

		return true;
	}

	private static boolean isSorted(float[] data, int fromIndex, int toIndex) {
		for (int i = fromIndex; ++i < toIndex;) {
			if (data[i - 1] > data[i]) {
				return false;
			}
		}

		return true;
	}

	private static boolean isSorted(int[] data, int fromIndex, int toIndex) {
		for (int i = fromIndex; ++i < toIndex;) {
			if (data[i - 1] > data[i]) {
				return false;
			}
		}

		return true;
	}

	private static boolean isSorted(long[] data, int fromIndex, int toIndex) {
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

	private final Set<ElementType> types;

	public SortTest() {
		this(new String[0]);
	}

	public SortTest(String... args) {
		super();
		this.lengths = getLengths(args);
		this.types = new LinkedHashSet<>();

		Long seed = null;

		for (String arg : args) {
			Matcher matcher;

			matcher = SeedPattern.matcher(arg);
			if (matcher.matches()) {
				seed = Long.valueOf(matcher.group(1));
			}

			matcher = TypesPattern.matcher(arg);
			if (matcher.matches()) {
				addTypes(types, matcher.group(1));
			}
		}

		if (seed == null) {
			seed = Long.valueOf(new Random().nextLong());

			logger.info(String.format(
					"Random seed value: %1$d. Add -srand=%1$d to the command line to reproduce this test manually.",
					seed));
		}

		this.random = new Random(seed.longValue());

		if (types.isEmpty()) {
			types.addAll(Arrays.asList(ElementType.values()));
		}
	}

	private void run() {
		int deviceCount = getDeviceCount();

		if (deviceCount == 0) {
			logger.info("No devices found or unsupported platform.");
		}

		for (int deviceId = 0; deviceId < deviceCount; ++deviceId) {
			for (ElementType type : types) {
				run(deviceId, type);
			}
		}
	}

	private void run(int deviceId, ElementType type) {
		logger.info(String.format("Running sort(%s[]) tests on device %d",
				type.name, Integer.valueOf(deviceId)));

		// warm up
		test(deviceId, type, 0x1_0000, false);

		logger.info(String.format("%13s %16s %16s", "Length", "Rate(GPU)", "Rate(Java)"));

		int limit = getLengthLimit(deviceId, type.size);

		for (int length : lengths) {
			if (length > limit) {
				logger.warn(String.format("Length %d exceeds limit of %d elements.",
						Integer.valueOf(length), Integer.valueOf(limit)));
				break;
			}

			test(deviceId, type, length, true);
		}
	}

	/**
	 * Sorts an array of the specified element type and length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param type  the element type
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void test(int deviceId, ElementType type, int size, boolean logResult) {
		switch (type) {
		case DOUBLE:
			testDouble(deviceId, size, logResult);
			break;
		case FLOAT:
			testFloat(deviceId, size, logResult);
			break;
		case INT:
			testInt(deviceId, size, logResult);
			break;
		case LONG:
			testLong(deviceId, size, logResult);
			break;
		default:
			throw new IllegalArgumentException();
		}
	}

	/**
	 * Sorts an array of doubles with the given length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void testDouble(int deviceId, int size, boolean logResult) {
		double[] gpuData = new double[size];

		// populate the array with random values
		for (int i = 0; i < size; ++i) {
			// Use this instead of random.nextDouble() to include NaNs, etc.
			gpuData[i] = Double.longBitsToDouble(random.nextLong());
		}

		if (size > 10) {
			// include special values, out of order
			gpuData[0] = Double.POSITIVE_INFINITY;
			gpuData[1] = Double.MAX_VALUE;
			gpuData[2] = Double.MIN_NORMAL;
			gpuData[3] = Double.MIN_VALUE;
			gpuData[4] = +0.0f;
			gpuData[5] = Double.NaN;
			gpuData[6] = -0.0f;
			gpuData[7] = -Double.MIN_VALUE;
			gpuData[8] = -Double.MIN_NORMAL;
			gpuData[9] = -Double.MAX_VALUE;
			gpuData[10] = Double.NEGATIVE_INFINITY;
		}

		double[] javaData = DisableJavaTiming ? null : gpuData.clone();
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
		if (javaData != null) {
			if (!DisableGC) {
				System.gc();
			}

			long javaStart = System.nanoTime();

			Arrays.sort(javaData);

			javaDurationNs = System.nanoTime() - javaStart;

			if (!Arrays.equals(gpuData, javaData)) {
				Assert.fail(String.format("sort failure (size=%d)", Integer.valueOf(size)));
			}
		}

		if (logResult) {
			String result = String.format("%13d %,16.3f %,16.3f", Integer.valueOf(size),
					elementsPerSecond(size, gpuDurationNs), elementsPerSecond(size, javaDurationNs));

			logger.info(result);
		}
	}

	/**
	 * Sorts an array of floats with the given length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void testFloat(int deviceId, int size, boolean logResult) {
		float[] gpuData = new float[size];

		// populate the array with random values
		for (int i = 0; i < size; ++i) {
			// Use this instead of random.nextFloat() to include NaNs, etc.
			gpuData[i] = Float.intBitsToFloat(random.nextInt());
		}

		if (size > 10) {
			// include special values, out of order
			gpuData[0] = Float.POSITIVE_INFINITY;
			gpuData[1] = Float.MAX_VALUE;
			gpuData[2] = Float.MIN_NORMAL;
			gpuData[3] = Float.MIN_VALUE;
			gpuData[4] = +0.0f;
			gpuData[5] = Float.NaN;
			gpuData[6] = -0.0f;
			gpuData[7] = -Float.MIN_VALUE;
			gpuData[8] = -Float.MIN_NORMAL;
			gpuData[9] = -Float.MAX_VALUE;
			gpuData[10] = Float.NEGATIVE_INFINITY;
		}

		float[] javaData = DisableJavaTiming ? null : gpuData.clone();
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
		if (javaData != null) {
			if (!DisableGC) {
				System.gc();
			}

			long javaStart = System.nanoTime();

			Arrays.sort(javaData);

			javaDurationNs = System.nanoTime() - javaStart;

			if (!Arrays.equals(gpuData, javaData)) {
				Assert.fail(String.format("sort failure (size=%d)", Integer.valueOf(size)));
			}
		}

		if (logResult) {
			String result = String.format("%13d %,16.3f %,16.3f", Integer.valueOf(size),
					elementsPerSecond(size, gpuDurationNs), elementsPerSecond(size, javaDurationNs));

			logger.info(result);
		}
	}

	/**
	 * Sorts an array of ints with the given length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void testInt(int deviceId, int size, boolean logResult) {
		int[] gpuData = new int[size];

		// populate the array with random values
		for (int i = 0; i < size; ++i) {
			gpuData[i] = random.nextInt();
		}

		int[] javaData = DisableJavaTiming ? null : gpuData.clone();
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
		if (javaData != null) {
			if (!DisableGC) {
				System.gc();
			}

			long javaStart = System.nanoTime();

			Arrays.sort(javaData);

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

	/**
	 * Sorts an array of longs with the given length, then checks to see if array is in order.
	 *
	 * @param deviceId  the device to use
	 * @param size  size of array to be tested
	 * @param logResult  should performance measurements be logged?
	 */
	private void testLong(int deviceId, int size, boolean logResult) {
		long[] gpuData = new long[size];

		// populate the array with random values
		for (int i = 0; i < size; ++i) {
			gpuData[i] = random.nextLong();
		}

		long[] javaData = DisableJavaTiming ? null : gpuData.clone();
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
		if (javaData != null) {
			if (!DisableGC) {
				System.gc();
			}

			long javaStart = System.nanoTime();

			Arrays.sort(javaData);

			javaDurationNs = System.nanoTime() - javaStart;

			if (!Arrays.equals(gpuData, javaData)) {
				Assert.fail(String.format("sort failure (size=%d)", Integer.valueOf(size)));
			}
		}

		if (logResult) {
			String result = String.format("%13d %,16.3f %,16.3f", Integer.valueOf(size),
					elementsPerSecond(size, gpuDurationNs), elementsPerSecond(size, javaDurationNs));

			logger.info(result);
		}
	}

	@Test(groups = { "level.sanity" })
	public void testBasic() {
		main(new String[] { "-geometric=10,10000000,31" });
	}

	@Test(groups = { "level.sanity" })
	public void testPowersOf2() {
		main(new String[] { "-geometric=1,16777216,25" });
	}

}
