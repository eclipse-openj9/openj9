package org.openj9.test.annotationIdentity;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.AssertJUnit;
import java.lang.annotation.*;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

@ArraysIntTest(value = { 23, 22, 45 }, id = "class", state = Enum1.BLOCKED)
@ArraysByteTest({ 23, 22, 45, (byte)128 })
public class Test_Annotations {

	static Logger logger = Logger.getLogger(Test_Annotations.class);

	@ArraysIntTest(value = { 80, 81, 85 }, id = "field", state = Enum1.WAITING) public Object field1;

	@Test(groups = { "level.sanity" })
	public void test_identicalAnnotations() throws Exception {
		Annotation[] all = Test_Annotations.class.getDeclaredAnnotations();
		for (int i = 0; i < all.length; i++) {
			logger.debug(all[i]);
		}
		ArraysIntTest taint1 = Test_Annotations.class.getAnnotation(ArraysIntTest.class);
		ArraysIntTest taint2 = Test_Annotations.class.getAnnotation(ArraysIntTest.class);
		logger.debug(taint1);
		AssertJUnit.assertTrue("Class annotations not identical", taint1 == taint2);
		int[] ints1 = taint1.value();
		int[] ints2 = taint1.value();
		AssertJUnit.assertTrue("Annotation content was identical", ints1 != ints2);
		ints1[0] = 255;
		logger.debug(taint1);
		logger.debug(Test_Annotations.class.getAnnotation(ArraysByteTest.class));

		Method foo = Test_Annotations.class.getMethod("foo", new Class[] { Integer.TYPE });
		ArraysIntTest mtaint1 = foo.getAnnotation(ArraysIntTest.class);
		ArraysIntTest mtaint2 = foo.getAnnotation(ArraysIntTest.class);
		logger.debug(mtaint1);
		AssertJUnit.assertTrue("Method annotations not identical", mtaint1 == mtaint2);

		ArraysIntTest ptaint1 = (ArraysIntTest)foo.getParameterAnnotations()[0][0];
		ArraysIntTest ptaint2 = (ArraysIntTest)foo.getParameterAnnotations()[0][0];
		logger.debug(ptaint1);
		AssertJUnit.assertTrue("Parameter annotations are identical", ptaint1 != ptaint2);

		Field f1 = Test_Annotations.class.getField("field1");
		ArraysIntTest ftaint1 = f1.getAnnotation(ArraysIntTest.class);
		ArraysIntTest ftaint2 = f1.getAnnotation(ArraysIntTest.class);
		logger.debug(ftaint1);
		AssertJUnit.assertTrue("Field annotations not identical", ftaint1 == ftaint2);
	}

	@ArraysIntTest(value = { 51, 52, 65 }, id = "method", state = Enum1.NEW)
	public void foo(@ArraysIntTest(value = { 70, 71, 75 }, id = "param", state = Enum1.RUNNABLE) int param1) {
	}
}
