/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and others
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
package jit.test.tr.BNDCHKSimplify;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class BNDCHKSimplifyTest{
	int limit;
	int dummy;
	public int checkIndexV1(int index) {
		if (index < 0 || index >= limit)
			throw new IndexOutOfBoundsException();
		return index;
	}

	public int checkIndexV2(int index) {
		if (index >= 0 && index < limit)
			return index;
		throw new NullPointerException();
	}

	public int checkIndexV3(int index) {
		if (index < 0)
			throw new IllegalStateException();
		if (index >= limit)
			throw new IllegalArgumentException();
		return index;
	}

	public int checkIndexV4(int index) {
		if (index < 0 || index >= limit)
			throw new StringIndexOutOfBoundsException();
		dummy += index;
		return index;
	}

	void setLimit(int limit) {
		this.limit = limit;
		this.dummy = 0;
	}

	@Test
	public void whenBoundsAreInRangeThenReturnWithoutAssert(){
		for (int i = 0; i < 20; ++i){
			BNDCHKSimplifyTest obj = new BNDCHKSimplifyTest();
			obj.setLimit(10);
			for (int j=0; j < 10; ++j) {
				obj.checkIndexV1(j);
				obj.checkIndexV2(j);
				obj.checkIndexV3(j);
				obj.checkIndexV4(j);
			}
		}
		int maxIndex = 10000;
		BNDCHKSimplifyTest objUnderTest = new BNDCHKSimplifyTest();
		objUnderTest.setLimit(maxIndex);
		for(int i = 0; i < maxIndex; ++i) {
			int actualIdx = objUnderTest.checkIndexV1(i);
			AssertJUnit.assertEquals("Mismatched index value", i, actualIdx);
		}

		objUnderTest.setLimit(maxIndex);
		for(int i = 0; i < maxIndex; ++i) {
			int actualIdx = objUnderTest.checkIndexV2(i);
			AssertJUnit.assertEquals("Mismatched index value", i, actualIdx);
		}

		objUnderTest.setLimit(maxIndex);
		for(int i = 0; i < maxIndex; ++i) {
			int actualIdx = objUnderTest.checkIndexV3(i);
			AssertJUnit.assertEquals("Mismatched index value", i, actualIdx);
		}

		objUnderTest.setLimit(maxIndex);
		for(int i = 0; i < maxIndex; ++i) {
			int actualIdx = objUnderTest.checkIndexV4(i);
			AssertJUnit.assertEquals("Mismatched index value", i, actualIdx);
		}
	}


	@Test
	public void whenBoundsAreOutOfRangeThenThrowCorrectException(){
		for (int i = 0; i < 20; ++i){
			BNDCHKSimplifyTest obj = new BNDCHKSimplifyTest();
			obj.setLimit(10);
			for (int j=0; j < 10; ++j) {
				obj.checkIndexV1(j);
				obj.checkIndexV2(j);
				obj.checkIndexV3(j);
				obj.checkIndexV4(j);
			}
		}
		int maxIndex = 10000;
		BNDCHKSimplifyTest objUnderTest = new BNDCHKSimplifyTest();
		objUnderTest.setLimit(maxIndex);
		try {
			int actualIdx = objUnderTest.checkIndexV1(maxIndex + 1);
			AssertJUnit.fail("failed to throw correct exception");
		} catch (IndexOutOfBoundsException outOfBound) {
		}

		objUnderTest.setLimit(maxIndex);
		try {
			int actualIdx = objUnderTest.checkIndexV2(maxIndex + 1);
			AssertJUnit.fail("failed to throw correct exception");
		}catch(NullPointerException outOfBound) {
		}

		objUnderTest.setLimit(maxIndex);
		try {
			int actualIdx = objUnderTest.checkIndexV3(-1);
			AssertJUnit.fail("failed to throw correct exception");
		} catch (IllegalStateException outOfBound) {
		}
		try {
			int actualIdx = objUnderTest.checkIndexV3(maxIndex + 1);
			AssertJUnit.fail("failed to throw correct exception");
		} catch (IllegalArgumentException outOfBound) {
		}

		objUnderTest.setLimit(maxIndex);
		try {
			int actualIdx = objUnderTest.checkIndexV4(maxIndex + 1);
			AssertJUnit.fail("failed to throw correct exception");
		} catch (StringIndexOutOfBoundsException outOfBound) {
		}
	}
}