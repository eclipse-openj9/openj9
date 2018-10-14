/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.testclasses;

/**
 * A test class which has StackMapTable attribute with primitive arrays in verification_type array in the frames.
 * 
 * @author ashutosh
 */
public class StackMapTableTest {
	public void stackMapTableTest(String args[]) {
		if (args.length == 1) {
			if (args[0].charAt(0) == 'B') {
				byte mat1[] = new byte[] {1, 2, 3};
				byte mat2[] = new byte[] {4, 5, 6};
				byte mat3[] = new byte[mat1.length];
				for (int i = 0; i < mat3.length; i++) {
					mat3[i] = (byte)(mat1[i] + mat2[i]);
				}
			} else if (args[0].charAt(0) == 'I') {
				int mat1[] = new int[] {1, 2, 3};
				int mat2[] = new int[] {4, 5, 6};
				int mat3[] = new int[mat1.length];
				for (int i = 0; i < mat3.length; i++) {
					mat3[i] = mat1[i] + mat2[i];
				}
			} else if (args[0].charAt(1) == 'J') {
				long mat1[] = new long[] {1, 2, 3};
				long mat2[] = new long[] {4, 5, 6};
				long mat3[] = new long[mat1.length];
				for (int i = 0; i < mat3.length; i++) {
					mat3[i] = mat1[i] + mat2[i];
				}
			}
		}
	}
}
