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
package org.openj9.test.stackWalker;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;

import java.lang.StackWalker.Option;
import java.lang.StackWalker.StackFrame;
import java.lang.invoke.MethodType;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This test StackWalker features added in Java 10 and is supplementary to the Java 9 tests.
 *
 */
public class StackWalkerTestJava10 {
	static final String TEST_MYLOADER_INSTANCE = "TestMyloaderInstance"; //$NON-NLS-1$
	protected static Logger logger = Logger.getLogger(StackWalkerTestJava10.class);

	@Test(groups = { "level.extended" })
	public void testSanity() {
		sanityMethod_1(3);
	}

	void sanityMethod_1(int i) {
		if (i > 0) sanityMethod_1(i-1); else sanityMethod_2(new Integer(42));
	}

	private void sanityMethod_2(Integer in) {
		sanityMethod_3(in, new HashMap<String, Integer>());
	}

	private int sanityMethod_3(Integer in, HashMap<String, Integer> hashMap) {
		printStack("Hello"); //$NON-NLS-1$
		return 42;
	}

	public void printStack(String arg) {
		final ClassLoader myClassLoader = this.getClass().getClassLoader();
		List<StackFrame> frameList = 
				StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE)
				.walk(s -> s.collect(Collectors.toList()));
		String expectedDescriptors[] = {
				"(Ljava/lang/String;)V", //$NON-NLS-1$
				"(Ljava/lang/Integer;Ljava/util/HashMap;)I", //$NON-NLS-1$
				"(Ljava/lang/Integer;)V", //$NON-NLS-1$
				"(I)V", //$NON-NLS-1$
				"(I)V", //$NON-NLS-1$
				"(I)V", //$NON-NLS-1$
				"(I)V", //$NON-NLS-1$
				"()V" //$NON-NLS-1$
		};
		Iterator<StackFrame> frameIterator = frameList.iterator();
		for (String s: expectedDescriptors) {
			assertTrue(frameIterator.hasNext(), "too few frames"); //$NON-NLS-1$
			StackFrame f = frameIterator.next();
			final String actualDescriptor = f.getDescriptor();
			assertEquals(actualDescriptor, s, "wrong descriptor"); //$NON-NLS-1$
			assertEquals(MethodType.fromMethodDescriptorString(s,myClassLoader), f.getMethodType(), 
					"wrong descriptor"); //$NON-NLS-1$
		}
	}

	@Test(groups = { "level.extended" })
	public void testRetainOption() {
		StackWalker walker = StackWalker.getInstance();
		getMyMethodType(walker);
	}

	String getMyMethodType(StackWalker walker) {
		Optional<StackFrame> callerFrameOption = walker.walk(s->s.findFirst());
		assertTrue(callerFrameOption.isPresent(), "missing stack frame"); //$NON-NLS-1$
		StackFrame callerFrame = callerFrameOption.get();
		assertEquals(callerFrame.getDescriptor(), 
				"(Ljava/lang/StackWalker;)Ljava/lang/String;",  //$NON-NLS-1$
				"wrong descriptor when RETAIN_CLASS_REFERENCE not configured"); //$NON-NLS-1$
		try {
			callerFrame.getMethodType();
			fail("UnsupportedOperationException not thrown"); //$NON-NLS-1$
		} catch (UnsupportedOperationException e) {
			logger.info("expected exception thrown", e); //$NON-NLS-1$
		}
		return "hello"; //$NON-NLS-1$
	}
}