/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package org.openj9.test.varhandle;

public class ArrayHelper {
	static byte[] reset(byte[] array) {
		if (null == array) {
			array = new byte[3];
		}
		array[0] = 1;
		array[1] = 2;
		array[2] = 3;
		return array;
	}
	
	static char[] reset(char[] array) {
		if (null == array) {
			array = new char[3];
		}
		array[0] = '1';
		array[1] = '2';
		array[2] = '3';
		return array;
	}
	
	static double[] reset(double[] array) {
		if (null == array) {
			array = new double[3];
		}
		array[0] = 1.1;
		array[1] = 2.2;
		array[2] = 3.3;
		return array;
	}
	
	static float[] reset(float[] array) {
		if (null == array) {
			array = new float[3];
		}
		array[0] = 1.1f;
		array[1] = 2.2f;
		array[2] = 3.3f;
		return array;
	}
	
	static int[] reset(int[] array) {
		if (null == array) {
			array = new int[3];
		}
		array[0] = 1;
		array[1] = 2;
		array[2] = 3;
		return array;
	}
	
	static long[] reset(long[] array) {
		if (null == array) {
			array = new long[3];
		}
		array[0] = 1;
		array[1] = 2;
		array[2] = 3;
		return array;
	}
	
	static String[] reset(String[] array) {
		if (null == array) {
			array = new String[3];
		}
		array[0] = "1";
		array[1] = "2";
		array[2] = "3";
		return array;
	}
	
	static Class<?>[] reset(Class<?>[] array) {
		if (null == array) {
			array = new Class<?>[3];
		}
		array[0] = Object.class;
		array[1] = String.class;
		array[2] = ClassLoader.class;
		return array;
	}
	
	static short[] reset(short[] array) {
		if (null == array) {
			array = new short[3];
		}
		array[0] = 1;
		array[1] = 2;
		array[2] = 3;
		return array;
	}
	
	static boolean[] reset(boolean[] array) {
		if (null == array) {
			array = new boolean[4];
		}
		array[0] = true;
		array[1] = false;
		array[2] = true;
		array[3] = false;
		return array;
	}
}
