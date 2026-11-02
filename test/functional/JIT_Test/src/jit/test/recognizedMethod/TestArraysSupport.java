/*
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package jit.test.recognizedMethod;

import org.testng.Assert;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;

import java.util.Arrays;
import java.util.Random;
import java.util.stream.IntStream;
import jdk.internal.misc.Unsafe;
import jdk.internal.util.ArraysSupport;
import jdk.internal.vm.annotation.ForceInline;


public class TestArraysSupport {

    // Backing arrays are allocated with MAX_SKIP extra elements at the front
    // so that starting at index aSkip or bSkip is always valid.
    private static final int MAX_TEST_ARRAY_SIZE = 32;
    private static final int MAX_SKIP            = 8;  // elements
    private static final int BACKING_SIZE        = MAX_SKIP + MAX_TEST_ARRAY_SIZE;

    // Element-index skips tried for aFromIndex and bFromIndex independently.
    // Chosen to cover: zero, sub-word, word, and cross-word-boundary offsets.
    private static final int[] SKIPS = { 0, 1, 3, 4, 7 };

    // =========================================================================
    // Tests for ArraysSupport.vectorizedMismatch
    //
    // One @Test per element type, invocationCount=2. On the first invocation
    // (typically interpreted) results are collected into an instance int[].
    // On the second invocation (JIT-compiled) the same sweep is run and each
    // result is compared against the stored first-invocation value, directly
    // catching any divergence between interpreted and compiled paths.
    //
    // Each sweep covers:
    //   - aSkip, bSkip in SKIPS  (independent element-index start positions)
    //   - len in 1..MAX_TEST_ARRAY_SIZE
    //   - mismatch planted at every element position 0..len-1, plus equal case
    // Offsets are computed as BASE + fromIndex << log2Scale, matching exactly
    // what the JDK's own ArraysSupport.mismatch() overloads do.
    // =========================================================================

    // Stored results from the first (interpreted) invocation, one per @Test.
    private int[] mismatchByteResults;
    private int[] mismatchCharResults;
    private int[] mismatchShortResults;
    private int[] mismatchIntResults;
    private int[] mismatchLongResults;
    private int[] mismatchFloatResults;
    private int[] mismatchDoubleResults;

    private int[] smallResults;

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchByte() {
        byte[] a = new byte[BACKING_SIZE];
        byte[] b = new byte[BACKING_SIZE];
        Arrays.fill(a, (byte) 1);
        Arrays.fill(b, (byte) 1);
        int log2 = ArraysSupport.LOG2_ARRAY_BYTE_INDEX_SCALE;
        if (mismatchByteResults == null) {
            mismatchByteResults = collectMismatch(a, b, Unsafe.ARRAY_BYTE_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((byte[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((byte[]) arr)[pos] = 1; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_BYTE_BASE_OFFSET, log2, mismatchByteResults, "byte",
                    (arr, off, pos) -> { ((byte[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((byte[]) arr)[pos] = 1; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchChar() {
        char[] a = new char[BACKING_SIZE];
        char[] b = new char[BACKING_SIZE];
        Arrays.fill(a, 'a');
        Arrays.fill(b, 'a');
        int log2 = ArraysSupport.LOG2_ARRAY_CHAR_INDEX_SCALE;
        if (mismatchCharResults == null) {
            mismatchCharResults = collectMismatch(a, b, Unsafe.ARRAY_CHAR_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((char[]) arr)[pos] = 'z'; },
                    (arr, off, pos) -> { ((char[]) arr)[pos] = 'a'; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_CHAR_BASE_OFFSET, log2, mismatchCharResults, "char",
                    (arr, off, pos) -> { ((char[]) arr)[pos] = 'z'; },
                    (arr, off, pos) -> { ((char[]) arr)[pos] = 'a'; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchShort() {
        short[] a = new short[BACKING_SIZE];
        short[] b = new short[BACKING_SIZE];
        Arrays.fill(a, (short) 1);
        Arrays.fill(b, (short) 1);
        int log2 = ArraysSupport.LOG2_ARRAY_SHORT_INDEX_SCALE;
        if (mismatchShortResults == null) {
            mismatchShortResults = collectMismatch(a, b, Unsafe.ARRAY_SHORT_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((short[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((short[]) arr)[pos] = 1; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_SHORT_BASE_OFFSET, log2, mismatchShortResults, "short",
                    (arr, off, pos) -> { ((short[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((short[]) arr)[pos] = 1; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchInt() {
        int[] a = new int[BACKING_SIZE];
        int[] b = new int[BACKING_SIZE];
        Arrays.fill(a, 1);
        Arrays.fill(b, 1);
        int log2 = ArraysSupport.LOG2_ARRAY_INT_INDEX_SCALE;
        if (mismatchIntResults == null) {
            mismatchIntResults = collectMismatch(a, b, Unsafe.ARRAY_INT_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((int[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((int[]) arr)[pos] = 1; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_INT_BASE_OFFSET, log2, mismatchIntResults, "int",
                    (arr, off, pos) -> { ((int[]) arr)[pos] = 2; },
                    (arr, off, pos) -> { ((int[]) arr)[pos] = 1; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchLong() {
        long[] a = new long[BACKING_SIZE];
        long[] b = new long[BACKING_SIZE];
        Arrays.fill(a, 1L);
        Arrays.fill(b, 1L);
        int log2 = ArraysSupport.LOG2_ARRAY_LONG_INDEX_SCALE;
        if (mismatchLongResults == null) {
            mismatchLongResults = collectMismatch(a, b, Unsafe.ARRAY_LONG_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((long[]) arr)[pos] = 2L; },
                    (arr, off, pos) -> { ((long[]) arr)[pos] = 1L; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_LONG_BASE_OFFSET, log2, mismatchLongResults, "long",
                    (arr, off, pos) -> { ((long[]) arr)[pos] = 2L; },
                    (arr, off, pos) -> { ((long[]) arr)[pos] = 1L; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchFloat() {
        float[] a = new float[BACKING_SIZE];
        float[] b = new float[BACKING_SIZE];
        Arrays.fill(a, 1.0f);
        Arrays.fill(b, 1.0f);
        int log2 = ArraysSupport.LOG2_ARRAY_FLOAT_INDEX_SCALE;
        if (mismatchFloatResults == null) {
            mismatchFloatResults = collectMismatch(a, b, Unsafe.ARRAY_FLOAT_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((float[]) arr)[pos] = 2.0f; },
                    (arr, off, pos) -> { ((float[]) arr)[pos] = 1.0f; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_FLOAT_BASE_OFFSET, log2, mismatchFloatResults, "float",
                    (arr, off, pos) -> { ((float[]) arr)[pos] = 2.0f; },
                    (arr, off, pos) -> { ((float[]) arr)[pos] = 1.0f; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVectorizedMismatchDouble() {
        double[] a = new double[BACKING_SIZE];
        double[] b = new double[BACKING_SIZE];
        Arrays.fill(a, 1.0);
        Arrays.fill(b, 1.0);
        int log2 = ArraysSupport.LOG2_ARRAY_DOUBLE_INDEX_SCALE;
        if (mismatchDoubleResults == null) {
            mismatchDoubleResults = collectMismatch(a, b, Unsafe.ARRAY_DOUBLE_BASE_OFFSET, log2,
                    (arr, off, pos) -> { ((double[]) arr)[pos] = 2.0; },
                    (arr, off, pos) -> { ((double[]) arr)[pos] = 1.0; });
        } else {
            compareMismatch(a, b, Unsafe.ARRAY_DOUBLE_BASE_OFFSET, log2, mismatchDoubleResults, "double",
                    (arr, off, pos) -> { ((double[]) arr)[pos] = 2.0; },
                    (arr, off, pos) -> { ((double[]) arr)[pos] = 1.0; });
        }
    }

    @Test(groups = "level.sanity", invocationCount = 2)
    public void testVecSmallChar() {
        int log2 = ArraysSupport.LOG2_ARRAY_CHAR_INDEX_SCALE;
        char[] a1 = {'a','b','c','d','e','f','g','h','X'};
        char[] b1 = {'a','b','c','d','e','f','g','h','i'};

        // Case 2: tail=2 (length=6, vectorized covers [0..3], tail=[4],[5])
        // tail(2) >= wordTail(2) -> int-width handler fires, checks [4]+[5] as one int
        // internal: detects mismatch at [4] -> 4
        // public:   same -> 4
        char[] a2 = {'a','b','c','d','e','f','g','h','X','j'};
        char[] b2 = {'a','b','c','d','e','f','g','h','i','j'};

        if (smallResults == null) {
            smallResults = new int[2];
            smallResults[0] = ArraysSupport.vectorizedMismatch(a1, Unsafe.ARRAY_CHAR_BASE_OFFSET, b1, Unsafe.ARRAY_CHAR_BASE_OFFSET, 9, log2);
            smallResults[1] = ArraysSupport.vectorizedMismatch(a2, Unsafe.ARRAY_CHAR_BASE_OFFSET, b2, Unsafe.ARRAY_CHAR_BASE_OFFSET, 10, log2);
        } else {
            Assert.assertEquals(ArraysSupport.vectorizedMismatch(a1, Unsafe.ARRAY_CHAR_BASE_OFFSET, b1, Unsafe.ARRAY_CHAR_BASE_OFFSET, 9, log2), smallResults[0], "no match len 9");
            Assert.assertEquals(ArraysSupport.vectorizedMismatch(a2, Unsafe.ARRAY_CHAR_BASE_OFFSET, b2, Unsafe.ARRAY_CHAR_BASE_OFFSET, 10, log2), smallResults[1], "no match len 10");
        }
    }



    // ---- helpers ------------------------------------------------------------

    @FunctionalInterface
    private interface ArraySetter { void set(Object arr, long baseOff, int idx); }

    /** Runs the full sweep, records each result into a fresh int[] and returns it. */
    @ForceInline
    private static int[] collectMismatch(Object a, Object b, long base, int log2,
                                         ArraySetter setMismatch, ArraySetter clearMismatch) {
        int total = sweepTotal();
        int[] results = new int[total];
        int idx = 0;
        for (int aSkip : SKIPS) {
            for (int bSkip : SKIPS) {
                long aOff = base + ((long) aSkip << log2);
                long bOff = base + ((long) bSkip << log2);
                for (int len = 1; len <= MAX_TEST_ARRAY_SIZE; len++) {
                    results[idx++] = ArraysSupport.vectorizedMismatch(a, aOff, b, bOff, len, log2);
                    for (int pos = 0; pos < len; pos++) {
                        setMismatch.set(b, 0, bSkip + pos);
                        results[idx++] = ArraysSupport.vectorizedMismatch(a, aOff, b, bOff, len, log2);
                        clearMismatch.set(b, 0, bSkip + pos);
                    }
                }
            }
        }
        return results;
    }

    /** Runs the full sweep, comparing each result against the stored interpreted results. */
    @ForceInline
    private static void compareMismatch(Object a, Object b, long base, int log2,
                                        int[] stored, String type,
                                        ArraySetter setMismatch, ArraySetter clearMismatch) {
        int idx = 0;
        for (int aSkip : SKIPS) {
            for (int bSkip : SKIPS) {
                long aOff = base + ((long) aSkip << log2);
                long bOff = base + ((long) bSkip << log2);
                for (int len = 1; len <= MAX_TEST_ARRAY_SIZE; len++) {
                    int result = ArraysSupport.vectorizedMismatch(a, aOff, b, bOff, len, log2);
                    Assert.assertEquals(result, stored[idx++],
                            String.format("%s aSkip=%d bSkip=%d len=%d equal: compiled=%d interpreted=%d",
                                    type, aSkip, bSkip, len, result, stored[idx - 1]));
                    for (int pos = 0; pos < len; pos++) {
                        setMismatch.set(b, 0, bSkip + pos);
                        result = ArraysSupport.vectorizedMismatch(a, aOff, b, bOff, len, log2);
                        Assert.assertEquals(result, stored[idx++],
                                String.format("%s aSkip=%d bSkip=%d len=%d mismatch@%d: compiled=%d interpreted=%d",
                                        type, aSkip, bSkip, len, pos, result, stored[idx - 1]));
                        clearMismatch.set(b, 0, bSkip + pos);
                    }
                }
            }
        }
    }

    private static int sweepTotal() {
        int count = 0;
        for (int ignored1 : SKIPS) {
            for (int ignored2 : SKIPS) {
                for (int len = 1; len <= MAX_TEST_ARRAY_SIZE; len++) {
                    count += 1 + len;
                }
            }
        }
        return count;
    }


    @Test(groups = "level.sanity", dataProvider = "byteArrayProvider", invocationCount = 2)
    public void testVectorHashCodeByte(final byte[] arr) {
        int expectedResult = hashCode(1, arr, 0, arr.length);
        int intrinsicResult = Arrays.hashCode(arr);

        Assert.assertEquals(intrinsicResult, expectedResult, String.format("Unexpected byte hashcode result for array of length %d", arr.length));
    }

    @Test(groups = "level.sanity", dataProvider = "charArrayProvider", invocationCount = 2)
    public void testVectorHashCodeChar(final char[] arr) {
        int expectedResult = hashCode(1, arr, 0, arr.length);
        int intrinsicResult = Arrays.hashCode(arr);

        Assert.assertEquals(intrinsicResult, expectedResult, String.format("Unexpected char hashcode result for array of length %d", arr.length));
    }

    @Test(groups = "level.sanity", dataProvider = "shortArrayProvider", invocationCount = 2)
    public void testVectorHashCodeShort(final short[] arr) {
        int expectedResult = hashCode(1, arr, 0, arr.length);
        int intrinsicResult = Arrays.hashCode(arr);

        Assert.assertEquals(intrinsicResult, expectedResult, String.format("Unexpected short hashcode result for array of length %d", arr.length));
    }

    @Test(groups = "level.sanity", dataProvider = "intArrayProvider", invocationCount = 2)
    public void testVectorHashCodeInteger(final int[] arr) {
        int expectedResult = hashCode(1, arr, 0, arr.length);
        int intrinsicResult = Arrays.hashCode(arr);

        Assert.assertEquals(intrinsicResult, expectedResult, String.format("Unexpected integer hashcode result for array of length %d", arr.length));
    }

    /* Generate MAX_TEST_ARRAY_SIZE number of random arrays for each element type */

    @DataProvider(name = "byteArrayProvider")
    public static Object[][] byteArrayProvider() {
        final Random random = new Random(0);

        // Generate MAX_TEST_ARRAY_SIZE arrays
        return IntStream.range(0, MAX_TEST_ARRAY_SIZE)
                .mapToObj(i -> new Object[]{generateByteArray(random, i)})
                .toArray(Object[][]::new);
    }

    @DataProvider(name = "charArrayProvider")
    public static Object[][] charArrayProvider() {
        final Random random = new Random(0);

        // Generate MAX_TEST_ARRAY_SIZE arrays
        return IntStream.range(0, MAX_TEST_ARRAY_SIZE)
                .mapToObj(i -> new Object[]{generateCharArray(random, i)})
                .toArray(Object[][]::new);
    }

    @DataProvider(name = "shortArrayProvider")
    public static Object[][] shortArrayProvider() {
        final Random random = new Random(0);

        // Generate MAX_TEST_ARRAY_SIZE arrays
        return IntStream.range(0, MAX_TEST_ARRAY_SIZE)
                .mapToObj(i -> new Object[]{generateShortArray(random, i)})
                .toArray(Object[][]::new);
    }

    @DataProvider(name = "intArrayProvider")
    public static Object[][] intArrayProvider() {
        final Random random = new Random(0);

        // Generate MAX_TEST_ARRAY_SIZE arrays
        return IntStream.range(0, MAX_TEST_ARRAY_SIZE)
                .mapToObj(i -> new Object[]{generateIntArray(random, i)})
                .toArray(Object[][]::new);
    }

    private static byte[] generateByteArray(final Random random, final int length) {
        final byte[] result = new byte[length];
        random.nextBytes(result);
        return result;
    }

    private static char[] generateCharArray(final Random random, final int length) {
        final char[] result = new char[length];
        IntStream.range(0, length).forEach(i -> result[i] = (char) (random.nextInt(Character.MAX_VALUE)));
        return result;
    }

    private static short[] generateShortArray(final Random random, final int length) {
        final short[] result = new short[length];
        IntStream.range(0, length).forEach(i -> result[i] = (short) (random.nextInt(Short.MAX_VALUE - Short.MIN_VALUE + 1)));
        return result;
    }

    private static int[] generateIntArray(final Random random, final int length) {
        return random.ints(length).toArray();
    }

    /* Not intrinsic hashCode implementations (reference of truth) */

    private static int hashCode(int result, byte[] a, int fromIndex, int length) {
        int end = fromIndex + length;
        for (int i = fromIndex; i < end; i++) {
            result = 31 * result + a[i];
        }
        return result;
    }

    private static int hashCode(int result, char[] a, int fromIndex, int length) {
        int end = fromIndex + length;
        for (int i = fromIndex; i < end; i++) {
            result = 31 * result + a[i];
        }
        return result;
    }

    private static int hashCode(int result, short[] a, int fromIndex, int length) {
        int end = fromIndex + length;
        for (int i = fromIndex; i < end; i++) {
            result = 31 * result + a[i];
        }
        return result;
    }

    private static int hashCode(int result, int[] a, int fromIndex, int length) {
        int end = fromIndex + length;
        for (int i = fromIndex; i < end; i++) {
            result = 31 * result + a[i];
        }
        return result;
    }
}
