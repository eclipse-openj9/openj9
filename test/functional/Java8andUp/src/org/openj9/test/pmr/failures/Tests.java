package org.openj9.test.pmr.failures;
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

@MyAnnotationArray({ @MyAnnotation1(a = @MyAnnotation2) })
@BooleanAnnotation(val1 = true, val2 = false)
@Test(groups = { "level.sanity" })
public class Tests {
	static Logger logger = Logger.getLogger(Tests.class);

	/**
	 * @param args
	 */

	/* PMR 35871,001,866
	 * CMVC 104230*/
	@Test
	public void test_arrayStoreFailure() {
		Class<Tests> fc = Tests.class;
		MyAnnotationArray maArr = (MyAnnotationArray)fc.getAnnotation(MyAnnotationArray.class);
		maArr.value();
	}

	/* PMR 91072,101,616
	 * CMVC 98901
	 * Only failed on Big Endian */
	@Test
	public void test_booleanReturnFailure() {
		Class<Tests> fc = Tests.class;
		BooleanAnnotation ann = (BooleanAnnotation)fc.getAnnotation(BooleanAnnotation.class);
		logger.debug("ALHJUDKASFBSDJDFS " + ann);
		AssertJUnit.assertTrue("val1 returned wrong value", ann.val1());
		AssertJUnit.assertFalse("val2 returned wrong value", ann.val2());
	}
}
