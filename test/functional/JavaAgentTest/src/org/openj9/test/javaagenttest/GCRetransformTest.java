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
package org.openj9.test.javaagenttest;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.instrument.IllegalClassFormatException;
import org.openj9.test.javaagenttest.util.Source;
import org.openj9.test.javaagenttest.util.Transformer;

import com.ibm.oti.vm.VM;

/*
 * This test case is only designed for testing gc handles static references of hot swap class in gencon,
 * please add -Xgcpolicy:gencon as VMArg for the testing
 */
@Test(groups = { "level.extended" })
public class GCRetransformTest {

	private static final Logger logger = Logger.getLogger(GCRetransformTest.class);

	@Test
	public void testGCRetransform(){
		logger.info("Starting GCRetransformTest...");
		Source obj = new Source();
		retransform(obj.getClass());
		obj.getArraySize();
		logger.debug("Forcing Local GC...");
		VM.localGC();
		logger.debug("Forcing Global GC...");
		System.gc();
		logger.debug("Test passed");
	}

	/**
	 * The following method performs a re-transformation of theClass after add new static field in the bytecode.
	 */
	private static void retransform(Class<?> theClass) {
		Transformer transformer = null;
		/* pass in methodName for adding initialize code for new field */
		transformer = new Transformer(Transformer.ADD_NEW_FIELD, "getArraySize");
		byte[] transformedClassBytes = null;

		try {
			logger.debug("Attempting BCI...");
			String className = theClass.getName();
			className = className.replace('.', '/');
			transformedClassBytes = transformer.transform(null, className, theClass, null, null);
			logger.warn("transformedClassBytes is " + transformedClassBytes);
			Assert.assertNotNull(transformedClassBytes, "transformedClassBytes is null");

			logger.debug("BCI complete");
			AgentMain.redefineClass(theClass, transformedClassBytes);
		} catch (IllegalClassFormatException e) {
			Assert.fail("Unexpected exception occured " + e.getMessage(), e);
		}
	}
}
