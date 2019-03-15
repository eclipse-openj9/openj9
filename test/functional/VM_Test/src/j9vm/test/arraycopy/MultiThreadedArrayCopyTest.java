/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
/*
 * Created on Apr 29, 2008
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.arraycopy;

import java.util.Arrays;
import java.util.HashSet;

/*
 * Test arraycopy in a multithreaded environment.
 * If arraycopy fails to copy objects atomically (e.g. it uses a byte-by-byte memcpy)
 * this test will fail or crash.
 */
public class MultiThreadedArrayCopyTest implements Runnable {

	static Object[] validValues = new Object[] {
		createValidValue(1),
		createValidValue(2),
		createValidValue(3),
		createValidValue(4),
		createValidValue(5),
		createValidValue(6),
		createValidValue(7),
		createValidValue(8),
		createValidValue(9),
		createValidValue(10)
	};
	
	static HashSet validHashSet = new HashSet(Arrays.asList(validValues));
	
	static Object[] source = (Object[])validValues.clone();

	static int count = 100000;
	
	String name;
	
	MultiThreadedArrayCopyTest(String name) {
		this.name = name;
	}
	
	public static void main(String[] args) {
		new Thread(new MultiThreadedArrayCopyTest("Thread-A")).start();
		new Thread(new MultiThreadedArrayCopyTest("Thread-B")).start();
		new Thread(new MultiThreadedArrayCopyTest("Thread-C")).start();
		new Thread(new MultiThreadedArrayCopyTest("Thread-D")).start();
	}

	public void run() {
		Object[] dest = new Object[validValues.length];
		for (int loop = 0; loop < count; loop++) {
			/* rotate the source array */
			rotateSource(loop);
			System.arraycopy(source, 0, dest, 0, dest.length);
			
			for (int i = 0; i < dest.length; i++) {
				if (!validHashSet.contains(dest[i])) {
					System.err.println("Error: Found unexpected object in array at " + i + " in " + name);
					System.err.println("dest = " + Arrays.asList(dest));
				}
			}
		}
	}
	
	/*
	 * Copy values from validValues into source.
	 * This copy is constantly rotating so that values are always being overwritten with other
	 * values.
	 */
	private static void rotateSource(int i) {
		int index = i % source.length;
		System.arraycopy(validValues, index, source, 0, source.length - index);
		System.arraycopy(validValues, 0, source, source.length - index, index);
	}
	
	/*
	 * Create a unique object to be used in the validValues table.
	 *  
	 */
	private static Object createValidValue(int i) {
		/* create some garbage to spread objects out */
		byte[] unused = new byte[17832];
		return Integer.toString(i);
	}
	
	
}
