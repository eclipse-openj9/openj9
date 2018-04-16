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
package j9vm.test.jnichk;

import java.math.BigInteger;

public class CriticalAlwaysCopy extends Test {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		boolean result = true;
		Object[] array = new Object[]{ new CriticalAlwaysCopy(), new String("Foo"), new BigInteger("10000") };
		String str = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. " +
				"Duis ut est a nunc cursus adipiscing. Curabitur tincidunt, eros quis hendrerit " +
				"auctor, sapien diam pretium sapien, a hendrerit leo diam ut nisi. " +
				"Praesent facilisis, odio sit amet interdum tincidunt, elit mi porttitor " +
				"tortor, ut rutrum neque odio eu tortor. Aenean quis neque id odio tincidunt euismod. " +
				"Suspendisse rhoncus interdum eros. Nulla vel massa. Aenean ultricies ligula at dui. " +
				"Sed mollis purus sit amet dolor. Integer sodales quam a arcu. Integer porta felis vel diam. " +
				"Proin sit amet elit sed dolor condimentum molestie. Aliquam pellentesque egestas nisi. " +
				"Vestibulum volutpat tempor arcu. Donec lobortis, tortor vitae rhoncus vulputate, lacus " +
				"neque facilisis augue, nec interdum erat velit quis neque. Nam et pede dignissim velit " +
				"dignissim consectetuer. Cras elit metus, sagittis id, accumsan luctus, vestibulum ut, velit. " +
				"Phasellus nec neque nec quam mollis tincidunt. Nunc et orci ut nibh semper molestie.";

		if (!testArray(array)) {
			result = false;
			System.err.println("Array test failed.");
		}
		if (!testString(str)) {
			result = false;
			System.err.println("String test failed.");
		}

		System.out.println(result ? "TEST PASSED" : "TEST FAILED");
	}

	private static native boolean testArray(Object[] array);
	private static native boolean testString(String str);
}
