/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.string;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.Spliterator;
import java.util.Spliterator.OfInt;
import java.util.function.IntConsumer;

import org.openj9.test.util.VersionCheck;
import org.testng.annotations.Test;

@Test(groups = { "level.sanity" })
public class StringStreams {
	static final String DIGITS = "0123456789"; //$NON-NLS-1$
	static final String SINGLECHAR = "A"; //$NON-NLS-1$
	static final String EMPTY = ""; //$NON-NLS-1$
	
	@Test
	public void testStringCharsSanity() {
		ArrayList<Integer> actualIntegers = new ArrayList<>();
		DIGITS.chars().forEachOrdered(i -> actualIntegers.add(Integer.valueOf(i)));
		checkDigitsOrdered(DIGITS, actualIntegers);
	}

	@Test
	public void testStringCharsCharacteristics() {
		Spliterator<?> s = DIGITS.chars().spliterator();
		int expectedCharacteristics = Spliterator.ORDERED | Spliterator.SIZED | Spliterator.SUBSIZED;
		if (VersionCheck.major() >= 11 ) {  /* IMMUTABLE not set in reference implementation Java 8 */
			expectedCharacteristics |= Spliterator.IMMUTABLE; 
		}
		int characteristics = s.characteristics();
		assertTrue((characteristics & expectedCharacteristics) == expectedCharacteristics,
				"chars() spliterator characteristics 0x" + Integer.toHexString(characteristics)  //$NON-NLS-1$
				+ "\ndid not have expected characteristics 0x" + Integer.toHexString(expectedCharacteristics)); //$NON-NLS-1$
	}

	@Test
	public void testStringCodePointsCharacteristics() {
		Spliterator<?> s = DIGITS.codePoints().spliterator();
		int expectedCharacteristics = Spliterator.ORDERED;
		if (VersionCheck.major() >= 11 ) {  /* IMMUTABLE not set in reference implementation Java 8 */
			expectedCharacteristics |= Spliterator.IMMUTABLE; 
		}
		int characteristics = s.characteristics();
		assertTrue((characteristics & expectedCharacteristics) == expectedCharacteristics,
				"codePoints() spliterator characteristics 0x" + Integer.toHexString(characteristics)  //$NON-NLS-1$
				+ "\ndid not have expected characteristics 0x" + Integer.toHexString(expectedCharacteristics)); //$NON-NLS-1$
	}

	@Test
	public void testStringCharsSpliterator() {
		Spliterator.OfInt s = DIGITS.codePoints().spliterator();
		final ArrayList<Integer> actualIntegers = new ArrayList<>();
		s.forEachRemaining((IntConsumer) i -> actualIntegers.add(Integer.valueOf(i)));
		for (int c = 0; c < DIGITS.length(); ++c) {
			assertEquals(actualIntegers.get(c), Integer.valueOf(DIGITS.codePointAt(c)), 
					"Wrong value at position " + c); //$NON-NLS-1$
		}
	}

	@Test
	public void testStringCodePointsSpliterator() {
		Spliterator.OfInt s = DIGITS.chars().spliterator();
		final ArrayList<Integer> actualIntegers = new ArrayList<>();
		s.forEachRemaining((IntConsumer) i -> actualIntegers.add(Integer.valueOf(i)));
		checkDigitsOrdered(DIGITS, actualIntegers);
	}

	@Test
	public void testStringCharsSpliteratorSingleChar() {
		Spliterator.OfInt s = SINGLECHAR.chars().spliterator();
		final ArrayList<Integer> actualIntegers = new ArrayList<>();
		s.forEachRemaining((IntConsumer) i -> actualIntegers.add(Integer.valueOf(i)));
		checkDigitsOrdered(SINGLECHAR, actualIntegers);
	}

	@Test
	public void testStringCharsSpliteratorEmpty() {
		Spliterator.OfInt s = EMPTY.chars().spliterator();
		final ArrayList<Integer> actualIntegers = new ArrayList<>();
		s.forEachRemaining((IntConsumer) i -> actualIntegers.add(Integer.valueOf(i)));
		checkDigitsOrdered(EMPTY, actualIntegers);
	}

	@Test
	public void testStringCharsSpliteratorSplitting() {
		OfInt s = DIGITS.chars().spliterator();
		ArrayList<OfInt> sArray = new ArrayList<>();
		while (Objects.nonNull(s)) {
			sArray.add(s);
			s = s.trySplit();
		}
		final HashSet<Integer> actualIntegers = new HashSet<>();
		for (OfInt e: sArray) {
			e.forEachRemaining((IntConsumer) i -> actualIntegers.add(Integer.valueOf(i)));
		}
		checkDigits(actualIntegers);
	}

	/**
	 * Consume a bit of the spliterator, then split it.
	 */
	@Test
	public void testStringCharsSpliteratorSplittingAfterIterating() {
		OfInt s = DIGITS.chars().spliterator();
		ArrayList<OfInt> sArray = new ArrayList<>();
		final HashSet<Integer> actualIntegers = new HashSet<>();
		IntConsumer consumer = (IntConsumer) i -> actualIntegers.add(Integer.valueOf(i));
		while (Objects.nonNull(s)) {
			s.tryAdvance(consumer);
			sArray.add(s);
			s = s.trySplit();
		}
		for (OfInt e: sArray) {
			e.forEachRemaining(consumer);
		}
		checkDigits(actualIntegers);
	}

	private static void checkDigitsOrdered(String expectedString, List<Integer> actualIntegers) {
		for (int i = 0; i < expectedString.length(); ++i) {
			assertEquals(actualIntegers.get(i), Integer.valueOf(expectedString.charAt(i)), 
					"Wrong value at position " + i); //$NON-NLS-1$
		}
	}
	
	private static void checkDigits(Set<Integer> actualIntegers) {
		assertEquals(actualIntegers.size(), DIGITS.length(), "wrong number of elements"); //$NON-NLS-1$
		for (int i = 0; i < DIGITS.length(); ++i) {
			assertTrue(actualIntegers.contains(Integer.valueOf(DIGITS.charAt(i))), i + " missing"); //$NON-NLS-1$
		}
	}

}
