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
package org.openj9.test.regression;
import sun.misc.Unsafe; 
import java.lang.reflect.Field;
import java.io.*;

import org.testng.log4testng.Logger;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

import com.ibm.misc.IOUtils;

@Test(groups = {"level.extended", "req.cmvc.194280", "defect.117298"})
public class Cmvc194280 {

	@Test
	public void testCmvc194280() throws NoSuchFieldException, SecurityException, IllegalArgumentException, IllegalAccessException {
		String classAsPath = "";
		Test123 test =  new Test123();
		Class<?> c = test.getClass();
		String className = c.getName();
		classAsPath = className.replace('.', '/') + ".class";
		InputStream stream = c.getClassLoader().getResourceAsStream(classAsPath);
		byte raw[] = null; 

		try {
			raw = IOUtils.readFully(stream, 1024, false);
		}
		catch(IOException ioe) {
			ioe.printStackTrace();
			AssertJUnit.fail("Unable to read the Test123 class from path = " + classAsPath);
		}

		Field theUnsafeInstance = Unsafe.class.getDeclaredField("theUnsafe");
		theUnsafeInstance.setAccessible(true);
		Unsafe unsafe = (Unsafe) theUnsafeInstance.get(Unsafe.class);
	
		Class<?> defineClass = unsafe.defineClass("org.openj9.test.regression.Test123", raw, 0, raw.length, null, null);		
		AssertJUnit.assertEquals("An unexpected classloader was used to load class Test123 (Expect the bootstrap classloader)", null, defineClass.getClassLoader());
	}
}

class Test123 
{
	private static Logger logger = Logger.getLogger(Test123.class);
    public void sayHello() {
		logger.debug("Test 123");
    }
}
