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
package org.openj9.test.jep401;

/**
 * Simple test used to verify whether an AOT-compiled method is loaded successfully, based
 * on compatible or incompatible settings of various value types-related features.
 */
public class Sample {
	public static final void sub(Object[] arr, boolean print) {
		arr[0] = null;
		if (print) {
			System.out.println("In Sample.sub");
		}
	}

	public static final void main(String[] args) {
		final int maxIters = 1000000;
		System.out.println("In Sample.main");
		for (int i = 1; i <= maxIters; i++) {
			sub(new Object[1], i == 1 || i == maxIters);
		}
		System.out.println("Leaving Sample.main");
	}
}
