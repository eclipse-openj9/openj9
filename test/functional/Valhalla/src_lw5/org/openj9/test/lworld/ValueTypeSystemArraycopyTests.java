/*
 * Copyright IBM Corp. and others 2023
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
package org.openj9.test.lworld;

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;


@Test(groups = { "level.sanity" })
public class ValueTypeSystemArraycopyTests {

	public interface SomeInterface {}

	public static class SomeIdentityClass implements SomeInterface {
		double val1;
		long val2;
		SomeIdentityClass2 val3;
		int val4;

		SomeIdentityClass(int i) {
			this.val1 = (double)i;
			this.val2 = (long)(i*2);
			this.val3 = new SomeIdentityClass2(i*3);
			this.val4 = i*4;
		}
	}

	public static class SomeIdentityClass2 implements SomeInterface {
		long val1;
		double val2;

		SomeIdentityClass2(int i) {
			this.val1 = (long)(i*5);
			this.val2 = (double)(i*6);
		}
	}

	public value static class SomeValueClass implements SomeInterface {
		double val1;
		long val2;
		SomeValueClass2 val3;
		int val4;

		SomeValueClass(int i) {
			this.val1 = (double)i;
			this.val2 = (long)(i*2);
			this.val3 = new SomeValueClass2(i*3);
			this.val4 = i*4;
		}
	}

	public value static class SomeValueClass2 implements SomeInterface {
		long val1;
		double val2;

		SomeValueClass2(int i) {
			this.val1 = (long)(i*5);
			this.val2 = (double)(i*6);
		}
	}

	public value static class SomeNullRestrictedValueClass implements SomeInterface {
		double val1;
		long val2;
		SomeNullRestrictedValueClass2! val3;
		int val4;

		public implicit SomeNullRestrictedValueClass();

		SomeNullRestrictedValueClass(int i) {
			this.val1 = (double)i;
			this.val2 = (long)(i*2);
			this.val3 = new SomeNullRestrictedValueClass2(i*3);
			this.val4 = i*4;
		}
	}

	public value static class SomeNullRestrictedValueClass2 implements SomeInterface {
		long val1;
		double val2;

		public implicit SomeNullRestrictedValueClass2();

		SomeNullRestrictedValueClass2(int i) {
			this.val1 = (long)(i*5);
			this.val2 = (double)(i*6);
		}
	}

	public static int ARRAY_SIZE = 10;

	public static SomeIdentityClass[] idArrayDst = new SomeIdentityClass[ARRAY_SIZE];
	public static SomeIdentityClass[] idArraySrc = new SomeIdentityClass[ARRAY_SIZE];
	public static SomeValueClass[] vtArrayDst = new SomeValueClass[ARRAY_SIZE];
	public static SomeValueClass[] vtArraySrc = new SomeValueClass[ARRAY_SIZE];
	public static SomeNullRestrictedValueClass![] nullRestrictedVtArrayDst = new SomeNullRestrictedValueClass![ARRAY_SIZE];
	public static SomeNullRestrictedValueClass![] nullRestrictedVtArraySrc = new SomeNullRestrictedValueClass![ARRAY_SIZE];

	public static SomeInterface[] ifIdArrayDst = new SomeIdentityClass[ARRAY_SIZE];
	public static SomeInterface[] ifIdArraySrc = new SomeIdentityClass[ARRAY_SIZE];
	public static SomeInterface[] ifVtArrayDst = new SomeValueClass[ARRAY_SIZE];
	public static SomeInterface[] ifVtArraySrc = new SomeValueClass[ARRAY_SIZE];
	public static SomeInterface[] ifNullRestrictedVtArrayDst = new SomeNullRestrictedValueClass![ARRAY_SIZE];
	public static SomeInterface[] ifNullRestrictedVtArraySrc = new SomeNullRestrictedValueClass![ARRAY_SIZE];
	public static SomeInterface[] ifArray1 = new SomeInterface[ARRAY_SIZE];
	public static SomeInterface[] ifArray2 = new SomeInterface[ARRAY_SIZE];
	public static SomeInterface[] ifArray3 = new SomeInterface[ARRAY_SIZE];

	public static SomeIdentityClass[] idArrayDstCheckForException = new SomeIdentityClass[ARRAY_SIZE];
	public static SomeNullRestrictedValueClass![] nullRestrictedVtArrayDstCheckForException = new SomeNullRestrictedValueClass![ARRAY_SIZE];

	static private void initArrays() {
		for (int i=0; i < ARRAY_SIZE; i++) {

			idArrayDst[i] = new SomeIdentityClass(i);
			idArraySrc[i] = new SomeIdentityClass(i*2);

			vtArrayDst[i] = new SomeValueClass(i*3);
			vtArraySrc[i] = new SomeValueClass(i*4);

			nullRestrictedVtArrayDst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*5);
			nullRestrictedVtArraySrc[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*6);

			ifIdArrayDst[i] = new SomeIdentityClass(i*7);
			ifIdArraySrc[i] = new SomeIdentityClass(i*8);

			ifVtArrayDst[i] = new SomeValueClass(i*9);
			ifVtArraySrc[i] = new SomeValueClass(i*10);

			ifNullRestrictedVtArrayDst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*11);
			ifNullRestrictedVtArraySrc[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*12);

			ifArray1[i] = new SomeIdentityClass(i*13);
			ifArray2[i] = new SomeValueClass(i*14);
			ifArray3[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*15);
		}
	}

	static private void initArraysForNPETest() {
		for (int i=0; i < ARRAY_SIZE; i++) {
			if (i >= ARRAY_SIZE/2) {
				ifArray3[i] = null;
			}
			else {
				ifArray3[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*30);
			}

			nullRestrictedVtArrayDst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*5);
			nullRestrictedVtArrayDstCheckForException[i] = nullRestrictedVtArrayDst[i];
		}
	}

	static private void initArraysForArrayStoreChkExceptionTest() {
		for (int i=0; i < ARRAY_SIZE; i++) {
			if (i >= ARRAY_SIZE/2) {
				ifArray1[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*13);
				ifArray3[i] = new SomeIdentityClass(i*30);
			}
			else {
				ifArray1[i] = new SomeIdentityClass(i*13);
				ifArray3[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*30);
			}

			idArrayDst[i] = new SomeIdentityClass(i);
			idArrayDstCheckForException[i] = idArrayDst[i];
			nullRestrictedVtArrayDst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*5);
			nullRestrictedVtArrayDstCheckForException[i] = nullRestrictedVtArrayDst[i];
		}
	}

	static private void checkResults(Object[] arr1, Object[] arr2) {
		for (int i=0; i < arr1.length; ++i) {
			assertEquals(arr1[i], arr2[i]);
		}
	}

	static private void checkResultsPartial(Object[] arr1, Object[] arr2, int numOfElements) {
		for (int i=0; i < numOfElements; ++i) {
			assertEquals(arr1[i], arr2[i]);
		}
	}

	static private void checkPVTArrayAfterException(int index) {
		for (int i=index; i < nullRestrictedVtArrayDst.length; ++i) {
			assertEquals(nullRestrictedVtArrayDst[i], nullRestrictedVtArrayDstCheckForException[i]);
		}
	}

	static private void checkIDArrayAfterException(int index) {
		for (int i=index; i < idArrayDst.length; ++i) {
			assertEquals(idArrayDst[i], idArrayDstCheckForException[i]);
		}
	}

	static private void testIDID(SomeIdentityClass[] src, SomeIdentityClass[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIDIF(SomeIdentityClass[] src, SomeInterface[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIFID(SomeInterface[] src, SomeIdentityClass[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testVTVT(SomeValueClass[] src, SomeValueClass[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testVTIF(SomeValueClass[] src, SomeInterface[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIFVT(SomeInterface[] src, SomeValueClass[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testPVTPVT(SomeNullRestrictedValueClass![] src, SomeNullRestrictedValueClass![] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testPVTIF(SomeNullRestrictedValueClass![] src, SomeInterface[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIFPVT(SomeInterface[] src, SomeNullRestrictedValueClass![] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIFIF(SomeInterface[] src, SomeInterface[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	static private void testIDOBJ(SomeIdentityClass[] src) {

		Object[] dst = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = new SomeIdentityClass(i*16);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testOBJID(SomeIdentityClass[] dst) {

		Object[] src = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = new SomeIdentityClass(i*17);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testVTOBJ(SomeValueClass[] src) {

		Object[] dst = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = new SomeValueClass(i*18);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testOBJVT(SomeValueClass[] dst) {

		Object[] src = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = new SomeValueClass(i*19);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testPVTOBJ(SomeNullRestrictedValueClass![] src) {

		Object[] dst = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*20);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testOBJPVT(SomeNullRestrictedValueClass![] dst) {

		Object[] src = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*21);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testIFOBJ(SomeInterface[] src) {

		Object[] dst = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*22);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testOBJIF(SomeInterface[] dst) {

		Object[] src = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*23);
		}

		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);

		checkResults(src, dst);
	}

	static private void testOBJOBJ(Object[] src, Object[] dst) {
		System.arraycopy(src, 0, dst, 0, ARRAY_SIZE);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy1() throws Throwable {

		initArrays();
		testIDID(idArraySrc, idArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIDID(idArraySrc, idArrayDst);

		checkResults(idArraySrc, idArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy2() throws Throwable {

		initArrays();
		testIDIF(idArraySrc, ifIdArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIDIF(idArraySrc, ifIdArrayDst);

		checkResults(idArraySrc, ifIdArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy3() throws Throwable {

		initArrays();
		testIFID(ifIdArraySrc, idArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIFID(ifIdArraySrc, idArrayDst);

		checkResults(ifIdArraySrc, idArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy4() throws Throwable {

		initArrays();
		testVTVT(vtArraySrc, vtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testVTVT(vtArraySrc, vtArrayDst);

		checkResults(vtArraySrc, vtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy5() throws Throwable {

		initArrays();
		testVTIF(vtArraySrc, ifVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testVTIF(vtArraySrc, ifVtArrayDst);

		checkResults(vtArraySrc, ifVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy6() throws Throwable {

		initArrays();
		testIFVT(ifVtArraySrc, vtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIFVT(ifVtArraySrc, vtArrayDst);

		checkResults(ifVtArraySrc, vtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy7() throws Throwable {

		initArrays();
		testPVTPVT(nullRestrictedVtArraySrc, nullRestrictedVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testPVTPVT(nullRestrictedVtArraySrc, nullRestrictedVtArrayDst);

		checkResults(nullRestrictedVtArraySrc, nullRestrictedVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy8() throws Throwable {

		initArrays();
		testPVTIF(nullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testPVTIF(nullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);

		checkResults(nullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy9() throws Throwable {

		initArrays();
		testIFPVT(ifNullRestrictedVtArraySrc, nullRestrictedVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIFPVT(ifNullRestrictedVtArraySrc, nullRestrictedVtArrayDst);

		checkResults(ifNullRestrictedVtArraySrc, nullRestrictedVtArrayDst);
	}


	@Test(priority=1)
	static public void testSystemArrayCopy10() throws Throwable {

		initArrays();
		testIFIF(ifIdArraySrc, ifIdArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testIFIF(ifIdArraySrc, ifIdArrayDst);
		checkResults(ifIdArraySrc, ifIdArrayDst);

		initArrays();
		testIFIF(ifVtArraySrc, ifVtArrayDst);
		checkResults(ifVtArraySrc, ifVtArrayDst);

		initArrays();
		testIFIF(ifNullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);
		checkResults(ifNullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);

		initArrays();
		testIFIF(ifArray1, ifIdArrayDst);
		checkResults(ifArray1, ifIdArrayDst);

		initArrays();
		testIFIF(ifIdArraySrc, ifArray1);
		checkResults(ifIdArraySrc, ifArray1);

		initArrays();
		testIFIF(ifArray2, ifVtArrayDst);
		checkResults(ifArray2, ifVtArrayDst);

		initArrays();
		testIFIF(ifVtArraySrc, ifArray2);
		checkResults(ifVtArraySrc, ifArray2);

		initArrays();
		testIFIF(ifArray3, ifNullRestrictedVtArrayDst);
		checkResults(ifArray3, ifNullRestrictedVtArrayDst);

		initArrays();
		testIFIF(ifNullRestrictedVtArraySrc, ifArray3);
		checkResults(ifNullRestrictedVtArraySrc, ifArray3);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy11() throws Throwable {

		initArrays();
		testIDOBJ(idArraySrc); // Fist invocation (Interpreter)

		initArrays();
		testIDOBJ(idArraySrc);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy12() throws Throwable {

		initArrays();
		testOBJID(idArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJID(idArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy13() throws Throwable {

		initArrays();
		testVTOBJ(vtArraySrc); // Fist invocation (Interpreter)

		initArrays();
		testVTOBJ(vtArraySrc);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy14() throws Throwable {

		initArrays();
		testOBJVT(vtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJVT(vtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy15() throws Throwable {

		initArrays();
		testPVTOBJ(nullRestrictedVtArraySrc); // Fist invocation (Interpreter)

		initArrays();
		testPVTOBJ(nullRestrictedVtArraySrc);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy16() throws Throwable {

		initArrays();
		testOBJPVT(nullRestrictedVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJPVT(nullRestrictedVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy17() throws Throwable {

		initArrays();
		testIFOBJ(ifNullRestrictedVtArraySrc); // Fist invocation (Interpreter)

		initArrays();
		testIFOBJ(ifNullRestrictedVtArraySrc);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy18() throws Throwable {

		initArrays();
		testOBJIF(ifNullRestrictedVtArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJIF(ifNullRestrictedVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy19() throws Throwable {

		initArrays();
		testOBJOBJ(ifIdArraySrc, ifIdArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJOBJ(ifIdArraySrc, ifIdArrayDst);
		checkResults(ifIdArraySrc, ifIdArrayDst);

		initArrays();
		testOBJOBJ(ifVtArraySrc, ifVtArrayDst);
		checkResults(ifVtArraySrc, ifVtArrayDst);

		initArrays();
		testOBJOBJ(ifNullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);
		checkResults(ifNullRestrictedVtArraySrc, ifNullRestrictedVtArrayDst);

		initArrays();
		testOBJOBJ(ifArray1, ifIdArrayDst);
		checkResults(ifArray1, ifIdArrayDst);

		initArrays();
		testOBJOBJ(ifIdArraySrc, ifArray1);
		checkResults(ifIdArraySrc, ifArray1);

		initArrays();
		testOBJOBJ(ifArray2, ifVtArrayDst);
		checkResults(ifArray2, ifVtArrayDst);

		initArrays();
		testOBJOBJ(ifVtArraySrc, ifArray2);
		checkResults(ifVtArraySrc, ifArray2);

		initArrays();
		testOBJOBJ(ifArray3, ifNullRestrictedVtArrayDst);
		checkResults(ifArray3, ifNullRestrictedVtArrayDst);

		initArrays();
		testOBJOBJ(ifNullRestrictedVtArraySrc, ifArray3);
		checkResults(ifNullRestrictedVtArraySrc, ifArray3);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy20() throws Throwable {

		Object[] src = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = new SomeIdentityClass(i*23);
		}

		testOBJOBJ(src, ifIdArrayDst); // Fist invocation (Interpreter)

		initArrays();
		testOBJOBJ(src, ifIdArrayDst);
		checkResults(src, ifIdArrayDst);

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = new SomeValueClass(i*24);
		}
		testOBJOBJ(src, ifVtArrayDst);
		checkResults(src, ifVtArrayDst);

		for (int i=0; i < ARRAY_SIZE; i++) {
			src[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*25);
		}
		testOBJOBJ(src, ifNullRestrictedVtArrayDst);
		checkResults(src, ifNullRestrictedVtArrayDst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy21() throws Throwable {

		Object[] dst = new Object[ARRAY_SIZE];

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = new SomeIdentityClass(i*23);
		}
		testOBJOBJ(ifIdArraySrc, dst); // Fist invocation (Interpreter)

		initArrays();

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = new SomeIdentityClass(i*24);
		}
		testOBJOBJ(ifIdArraySrc, dst);
		checkResults(ifIdArraySrc, dst);

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = new SomeValueClass(i*25);
		}
		testOBJOBJ(ifVtArraySrc, dst);
		checkResults(ifVtArraySrc, dst);

		for (int i=0; i < ARRAY_SIZE; i++) {
			dst[i] = (SomeNullRestrictedValueClass!)new SomeNullRestrictedValueClass(i*26);
		}
		testOBJOBJ(ifNullRestrictedVtArraySrc, dst);
		checkResults(ifNullRestrictedVtArraySrc, dst);
	}

	@Test(priority=1)
	static public void testSystemArrayCopy22() throws Throwable {

		try {
			initArraysForNPETest(); // ifArray3[ARRAY_SIZE/2] is NULL
			testIFPVT(ifArray3, nullRestrictedVtArrayDst);
		} catch (java.lang.NullPointerException npe1) {
			try {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);

				initArraysForNPETest();
				testIFPVT(ifArray3, nullRestrictedVtArrayDst);
			} catch (java.lang.NullPointerException npe2) {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				// pass
				return;
			}
		}

		Assert.fail("Expect a NullPointerException. No exception or wrong kind of exception thrown");
	}

	@Test(priority=1)
	static public void testSystemArrayCopy23() throws Throwable {

		try {
			initArraysForNPETest(); // ifArray3[ARRAY_SIZE/2] is NULL
			testIFIF(ifArray3, nullRestrictedVtArrayDst);
		} catch (java.lang.NullPointerException npe1) {
			try {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);

				initArraysForNPETest();
				testIFIF(ifArray3, nullRestrictedVtArrayDst);
			} catch (java.lang.NullPointerException npe2) {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);
				// pass
				return;
			}
		}

		Assert.fail("Expect a NullPointerException. No exception or wrong kind of exception thrown");
	}

	@Test(priority=1)
	static public void testSystemArrayCopy24() throws Throwable {

		try {
			initArraysForArrayStoreChkExceptionTest();  // ifArray3[ARRAY_SIZE/2] is SomeIdentityClass
			testIFPVT(ifArray3, nullRestrictedVtArrayDst);
		} catch (java.lang.ArrayStoreException ase1) {
			try {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);

				initArraysForArrayStoreChkExceptionTest();
				testIFPVT(ifArray3, nullRestrictedVtArrayDst);
			} catch (java.lang.ArrayStoreException ase2) {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);
				// pass
				return;
			}
		}

		Assert.fail("Expect a ArrayStoreException. No exception or wrong kind of exception thrown");
	}

	@Test(priority=1)
	static public void testSystemArrayCopy25() throws Throwable {

		try {
			initArraysForArrayStoreChkExceptionTest();  // ifArray3[ARRAY_SIZE/2] is SomeIdentityClass
			testIFIF(ifArray3, nullRestrictedVtArrayDst);
		} catch (java.lang.ArrayStoreException ase1) {
			try {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);

				initArraysForArrayStoreChkExceptionTest();
				testIFIF(ifArray3, nullRestrictedVtArrayDst);
			} catch (java.lang.ArrayStoreException ase2) {
				checkResultsPartial(ifArray3, nullRestrictedVtArrayDst, ARRAY_SIZE/2);
				checkPVTArrayAfterException(ARRAY_SIZE/2);
				// pass
				return;
			}
		}

		Assert.fail("Expect a ArrayStoreException. No exception or wrong kind of exception thrown");
	}

	@Test(priority=1)
	static public void testSystemArrayCopy26() throws Throwable {

		try {
			initArraysForArrayStoreChkExceptionTest();  // ifArray1[ARRAY_SIZE/2] is SomeNullRestrictedValueClass
			testIFIF(ifArray1, idArrayDst);
		} catch (java.lang.ArrayStoreException ase1) {
			try {
				checkResultsPartial(ifArray1, idArrayDst, ARRAY_SIZE/2);
				checkIDArrayAfterException(ARRAY_SIZE/2);

				initArraysForArrayStoreChkExceptionTest();
				testIFIF(ifArray1, idArrayDst);
			} catch (java.lang.ArrayStoreException ase2) {
				checkResultsPartial(ifArray1, idArrayDst, ARRAY_SIZE/2);
				checkIDArrayAfterException(ARRAY_SIZE/2);
				// pass
				return;
			}
		}

		Assert.fail("Expect a ArrayStoreException. No exception or wrong kind of exception thrown");
	}
}
