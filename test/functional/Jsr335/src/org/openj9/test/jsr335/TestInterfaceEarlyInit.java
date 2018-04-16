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
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;

public class TestInterfaceEarlyInit {
	@Test(groups = { "level.sanity" })
	public void testInterfaceEarlyInit() throws Throwable {
		ClassLoaderInterfaceEarlyInit loader = new ClassLoaderInterfaceEarlyInit(TestInterfaceEarlyInit.class.getClassLoader());
		
		loader.loadClass("I");
		Class<?> classC = loader.loadClass("C");
		
		Method methodInC = classC.getMethod("bar");
		
		try {
			methodInC.invoke(null);
		} catch (InternalError e) {
			AssertJUnit.assertTrue("Interface I was not initialized before its implementer C", e.getMessage().equals("interface"));
			return;
		}
		
		Assert.fail("Did not catch expected InternalError.");
	}
}

class ClassLoaderInterfaceEarlyInit extends ClassLoader {
	public ClassLoaderInterfaceEarlyInit(ClassLoader parent) {
		super(parent);
	}
	
	@Override
	public Class<?> loadClass(String className) throws ClassNotFoundException {
		if (className.equals("C")) {
			byte[] classBytes = ASMInterfaceEarlyInit.dumpC();
			return defineClass("C", classBytes, 0, classBytes.length);
		} else if (className.equals("I")) {
			byte[] classBytes = ASMInterfaceEarlyInit.dumpI();
			return defineClass("I", classBytes, 0, classBytes.length);
		}
		return super.loadClass(className);
	}
}

/*
interface I {
	static {
		throw new InternalError("interface");
	}

	public default void foo() { }
}
*/

/*
class C implements I {
	static {
			throw new InternalError("class");
	}

	static void bar() { }
}
*/
