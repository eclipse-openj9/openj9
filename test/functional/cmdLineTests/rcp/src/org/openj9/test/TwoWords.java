/*
 * Copyright IBM Corp. and others 2026
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assem bly-exception-1.0
 */
package org.openj9.test;

public class TwoWords {
	String word1;
	String word2;
	static int n = 1000;

	TwoWords(String greeting1, String greeting2)
	{
		word1 = greeting1;
		word2 = greeting2;
	}
	void verify(String greeting1, String greeting2) {
		if (!word1.equals(greeting1))
			throw new RuntimeException(greeting1 + "!=" + word1);
		if (!word2.equals(greeting2))
			throw new RuntimeException(greeting2 + "!=" + word2);
    }
	public static void main(String[] args) {
		TwoWords[] arr = new TwoWords[n];
		for (int i = 0; i < n; i++) {
			arr[i] = new TwoWords("Hello", "World");
		}
		for (int i = 0; i < n; i++) {
			arr[i].verify("Hello", "World");
		}

		System.out.println("Success");
	}
}
