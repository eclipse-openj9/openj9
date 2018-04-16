package org.openj9.test.runlast;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.net.*;

/*******************************************************************************
 * Copyright (c) 2012, 2018 IBM Corp. and others
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
 * These tests have to run last because they have side affects that cannot be reverted.
 */
@Test(groups = { "level.sanity" })
public class Test_JavaNetWithSideEffectsShouldRunLast {

/**
 * @tests java.lang.ClassLoader#getResource(java.lang.String)
 */
@Test
public void test_getResource() {
	// Once the URLStreamHandlerFactory is set, it cannot be reverted
	URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory() {
		public URLStreamHandler createURLStreamHandler(String protocol) {
			if (protocol.equals("jar")) {
				// When the jar URL for the bootstrap resource is created, it should not have to create
				// an URLStreamHandler as it should be using an existing context
				throw new Error("Testing. Don't create any " + protocol + " handlers");
			}
			try {
				Class handlerClass = Class.forName("sun.net.www.protocol." + protocol + ".Handler");
				return (URLStreamHandler)handlerClass.newInstance();
			} catch (Exception e) {
				throw new Error(e);
			}
		}
	});
	try {
		URL url = ClassLoader.getSystemClassLoader().getResource("java/lang/Object.class");
		AssertJUnit.assertTrue("URL should not be null", url != null);
	} catch (Throwable e) {
		e.printStackTrace();
		AssertJUnit.fail("should have created URL: " + e);
	}
}

}
