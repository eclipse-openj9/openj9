/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

package org.openj9.test.classtests;

import java.io.*;
import java.util.*;
import java.lang.reflect.Field;

import jdk.internal.misc.Unsafe;

/* Generates the Anonymous class and the Unsafe class */

public class ClassTestHelper {
	private static Unsafe unsafe = Unsafe.getUnsafe();
	public byte[] getContent(InputStream is) throws Throwable {
		if (null == is) {
			return null;
		}
		int read;
		byte[] data = new byte[4096];
		ByteArrayOutputStream isbuffer = new ByteArrayOutputStream();
		while (-1 != (read = is.read(data, 0, data.length))) {
			isbuffer.write(data, 0, read);
		}
		return isbuffer.toByteArray();
	}

	public void func(Class hostclass, InputStream anonUnsafeClassStream, InputStream unsafeBootClassStream) throws Throwable {
		byte[] content = getContent(anonUnsafeClassStream);
		ClassLoader extLoader = hostclass.getClassLoader();
		if (null != content) {
			Class<?> anonclass = unsafe.defineAnonymousClass(hostclass, content, null);
			Class unsafeclass = unsafe.defineClass(null, content, 0, content.length, extLoader, hostclass.getProtectionDomain());
			anonclass.getMethod("func").invoke(anonclass.getDeclaredConstructor().newInstance());
			unsafeclass.getMethod("funcunsafe").invoke(unsafeclass.getDeclaredConstructor().newInstance());
		}
		content = getContent(unsafeBootClassStream);
		if (null != content) {
			Class<?> unsafeclass = unsafe.defineClass(null, content, 0, content.length, null, null);
			unsafeclass.getMethod("bar").invoke(unsafeclass.getDeclaredConstructor().newInstance());
		}
		System.out.println("test done!");
	}
}
