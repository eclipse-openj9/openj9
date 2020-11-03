/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package jit.test.jitt.array;

import org.testng.annotations.Test;
import org.testng.Assert;	
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" }, invocationCount=2)
public class multianewarray {
	class Vector {
		long x;
		long y;
	}

	private Integer[][] createMultiArray(int firstDimLength, int secondDimLength) {
		Integer[][] multiarray = new Integer[firstDimLength][secondDimLength];
		for (int i = 0; i < firstDimLength; i++) {
			for (int j = 0; j < secondDimLength; j++) {
				multiarray[i][j] = i * j;
			}
		}
		return multiarray;
	}

	private Vector[][] createMultiVector(int firstDimLength, int secondDimLength) {
		Vector[][] multiVector = new Vector[firstDimLength][secondDimLength];
		for (int i = 0; i < firstDimLength; i++) {
			for (int j = 0; j < secondDimLength; j++) {
				multiVector[i][j] = new Vector();
				multiVector[i][j].x = i * j;
				multiVector[i][j].y = i * i + j * j;
			}
		}
		return multiVector;
	}

	private void testMultiVector(Vector[][] inputVector, int dim1Length, int dim2Length) {
		AssertJUnit.assertEquals(dim1Length, inputVector.length);
		if (inputVector.length > 0)
			AssertJUnit.assertEquals(dim2Length, inputVector[0].length);

		for (int i = 0; i < inputVector.length; i++) {
			for (int j = 0; j < inputVector[0].length; j++) {
				AssertJUnit.assertEquals(i*j, (inputVector[i][j]).x);
				AssertJUnit.assertEquals(i*i + j*j, (inputVector[i][j]).y);
			}
		}
	}
	private void testMultiArray(Integer[][] inputArray, int dim1Length, int dim2Length) {
		AssertJUnit.assertEquals(dim1Length, inputArray.length);
		if (inputArray.length > 0)
			AssertJUnit.assertEquals(dim2Length, inputArray[0].length);
		for (int i = 0; i < inputArray.length; i++) {
			for (int j = 0; j < inputArray[0].length; j++) {
				AssertJUnit.assertEquals(i*j, (inputArray[i][j]).intValue());
			}
		}
	}
	@Test
	public void testmultianewarray() {
		Integer[][] inputArray;
		Vector[][] inputVector;
		for (int i = 0; i <= 64; i++) {
			inputVector = createMultiVector(0, 0);
			testMultiVector(inputVector, 0, 0);
			inputArray = createMultiArray(0, 0);
			testMultiArray(inputArray, 0, 0);
		}
		for (int i = 0; i <= 64; i++) {
			inputVector = createMultiVector(i, 0);
			testMultiVector(inputVector, i, 0);
			inputArray = createMultiArray(i, 0);
			testMultiArray(inputArray, i, 0);
		}
		for (int i = 0; i <= 64; i++) {
			inputArray = createMultiArray(i, 10);
			testMultiArray(inputArray, i, 10);
			inputVector = createMultiVector(i, 10);
			testMultiVector(inputVector, i, 10);
		}
	}   
}
