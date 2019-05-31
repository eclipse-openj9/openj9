/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package org.openj9.test.regression;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import org.testng.log4testng.Logger;

/*
 * This test case is written to catch defect 191564: Java 626 AnnotationParser executes class static code and throws
 * RuntimeException.
 */

@Test(groups = {"level.extended", "req.cmvc.191564"})
public class Cmvc191564 {
	
	private static Logger logger = Logger.getLogger(Cmvc191564.class);
	
	@Test
	public void testAnnotation()  {
		Annotation[] annotations= OtherClass.class.getAnnotations();
		for (int i =0 ; i < annotations.length; i++){
			logger.info("Get Annotations:" + annotations[i].toString());
			AssertJUnit.assertTrue("Wrong Annotation.",annotations[i].toString().contains("ReferencedClass"));
		}
				
	}
}


@MyAnno(ReferencedClass.class)
class OtherClass {
}

class ReferencedClass {

	static {
	
		if (true) {
			throw new RuntimeException();
		}
	}
}

@Retention(RetentionPolicy.RUNTIME)
@interface MyAnno {
	Class<?> value();
}
