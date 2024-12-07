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

public class TestArraysSupport {

    private static final int MAX_TEST_ARRAY_SIZE = 140;

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
