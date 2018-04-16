/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

@Test(groups = { "level.sanity","component.jit" })
public class byteArrayLoadStore extends jit.test.jitt.Test {



static byte byteArry[]= {0,1,2,3,127,-128};



private boolean check(byte arrayP[]){

	int len = arrayP.length;

	for (int i=0;i<len;i++) {

		if (arrayP[i] != byteArry[i]){

			return false;

		}

	}

	return true;

}



private byte[] tstFoo(byte arrayP[],int zero) {

	int len = arrayP.length;

	byte byteArryLocal[] = new byte [len];

	for (int i=0;i<len;i++) {

		byteArryLocal[i] = arrayP[i]; 

	}

	return byteArryLocal;

}


@Test
public void testbyteArrayLoadStore() {

	int zero = 0;

	byteArrayLoadStore x = new byteArrayLoadStore();

	byte arrayReturned[];



	for (int j = 0; j < sJitThreshold+1; j++) {

		arrayReturned = x.tstFoo(byteArry,0);

		if (!check(arrayReturned)){

	         Assert.fail("byte Array value bad in interpreted");

		}

	}

}



}

