/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package org.openj9.test.stackWalker;

import java.util.HashMap;
import java.util.List;
import java.util.stream.Collectors;
import java.lang.StackWalker.Option;
import java.lang.StackWalker.StackFrame;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.extended" })
@SuppressWarnings("nls")
public class StackWalkerTestJava10 {
	static final String TEST_MYLOADER_INSTANCE = "TestMyloaderInstance";
	protected static Logger logger = Logger.getLogger(StackWalkerTestJava10.class);

	@Test
	public void testSanity() {
		f1(3);
	}

	void f1(int i) {
		if (i > 0) f1(i-1); else f2(new Integer(42));
	}

	private void f2(Integer in) {
		f3(in, new HashMap<String, Integer>());
	}

	private int f3(Integer in, HashMap<String, Integer> hashMap) {
		printStack("Hello");
		return 42;
	}

	public void printStack(String arg) {
		List<StackFrame> frameList = (StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE))
		.walk(s -> s.collect(Collectors.toList()));
		String expectedDescriptors[];
		for (StackFrame f: frameList) {
			System.out.println(f.getDescriptor());
		}
	}

}
