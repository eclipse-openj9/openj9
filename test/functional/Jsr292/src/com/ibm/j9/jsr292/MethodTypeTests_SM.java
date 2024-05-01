/*
 * Copyright IBM Corp. and others 2022
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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.io.*;
import java.lang.invoke.MethodType;

/**
 * @author mesbah
 * This class contains tests for the MethodType API.
 */
public class MethodTypeTests_SM {

	/**
	 * Ensure that MethodTypes serialization works. Runs with the SecurityManager enabled.
	 */
	@Test(groups = { "level.extended" })
	public void test_SerializeGenericMethodType() throws IOException, ClassNotFoundException {
		Class<?> returnType = String.class;
		Class<?> paramType = Class.class;
		MethodType mt = MethodType.methodType(returnType, paramType);
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		ObjectOutputStream oos = new ObjectOutputStream(baos);
		oos.writeObject(mt);
		oos.close();
		ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(baos.toByteArray()));
		System.setSecurityManager(new SecurityManager());
		try {
			MethodType newMT = (MethodType) ois.readObject();

			// Validate MethodType constructed from serialized data
			AssertJUnit.assertEquals(returnType, newMT.returnType());
			AssertJUnit.assertEquals(paramType, newMT.parameterType(0));
		} finally {
			ois.close();
			System.setSecurityManager(null);
		}
	}
}
