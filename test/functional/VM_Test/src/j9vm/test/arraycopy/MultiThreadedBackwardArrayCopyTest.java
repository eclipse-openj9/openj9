/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

package j9vm.test.arraycopy;
import java.util.Arrays;
import java.util.HashSet;

/*
 * Test atomicity of backward arraycopy in a multithreaded environment.
 */

public class MultiThreadedBackwardArrayCopyTest implements Runnable {
	static Object[] source = new Object[] {
		createValidValue(1),
		createValidValue(2),
		createValidValue(3),
		createValidValue(4),
		createValidValue(5),
		createValidValue(6),
		createValidValue(7),
		createValidValue(8),
		createValidValue(9),
		createValidValue(10),
		createValidValue(11),
		createValidValue(12),
		createValidValue(13),
		createValidValue(14),
		createValidValue(15),
		createValidValue(16),
		createValidValue(17),
		createValidValue(18),
		createValidValue(19),
		createValidValue(20),
		createValidValue(21),
		createValidValue(22),
		createValidValue(23),
		createValidValue(24),
		createValidValue(25),
		createValidValue(26),
		createValidValue(27),
		createValidValue(28),
		createValidValue(29),
		createValidValue(30),
		createValidValue(31),
		createValidValue(32),
		createValidValue(33),
		createValidValue(34),
	};

	static HashSet<Object> validHashSet = new HashSet<Object>(Arrays.asList(source));
	String name;
	static int count = 1000000;
	MultiThreadedBackwardArrayCopyTest(String name) {
		this.name = name;
	}

	public static void main(String[] args) throws InterruptedException {
		new Thread(new MultiThreadedBackwardArrayCopyTest("Thread-1")).start();
		new Thread(new MultiThreadedBackwardArrayCopyTest("Thread-2")).start();
		new Thread(new MultiThreadedBackwardArrayCopyTest("Thread-3")).start();
		new Thread(new MultiThreadedBackwardArrayCopyTest("Thread-4")).start();
	}

	public void run() {
		Object[] dest = new Object[source.length];
		for (int i=0; i < count; i++) {
			rotateSource();
			System.arraycopy(source, 0, dest, 0, dest.length);
			for (int j=0; j < dest.length; j++){
				if (!validHashSet.contains(dest[j])){
					System.out.println("Error: Found unexpected object in array at " + j + " in " + name);
				}
			}
		}
	}


	// Create a unique String object
	private static Object createValidValue(int i) {
		// Create array of bytes to spread out the objects
		byte [] unused = new byte[17832];
		return Integer.toString(i);
	}

	// Rotate source right forcing it to do array copy in backward direction
	public static void rotateSource() {
		Object temp = source[source.length - 1]; // Getting last element in temp
		System.arraycopy(source, 1, source, 2, source.length - 2);
		source[1] = source[0];
		source[0] = temp;
	}
}
