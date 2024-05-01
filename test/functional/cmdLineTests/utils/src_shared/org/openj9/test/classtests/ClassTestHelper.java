/*
 * Copyright IBM Corp. and others 2019
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
package org.openj9.test.classtests;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;

import org.openj9.test.access.UnsafeClasses;

/*
 * Generates the anonymous (or hidden) class and the unsafe class.
 */
public class ClassTestHelper {

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

	public void func(Class<?> hostclass, InputStream anonUnsafeClassStream, InputStream unsafeBootClassStream) throws Throwable {
		byte[] content = getContent(anonUnsafeClassStream);
		if (null != content) {
			ClassLoader extLoader = hostclass.getClassLoader();
			Class<?> anonClass = UnsafeClasses.defineAnonOrHiddenClass(hostclass, content);
			Class unsafeClass = UnsafeClasses.defineClass(null, content, 0, content.length, extLoader, hostclass.getProtectionDomain());
			anonClass.getMethod("func").invoke(anonClass.getDeclaredConstructor().newInstance());
			unsafeClass.getMethod("funcunsafe").invoke(unsafeClass.getDeclaredConstructor().newInstance());
		}
		content = getContent(unsafeBootClassStream);
		if (null != content) {
			Class<?> unsafeClass = UnsafeClasses.defineClass(null, content, 0, content.length, null, null);
			unsafeClass.getMethod("bar").invoke(unsafeClass.getDeclaredConstructor().newInstance());
		}
		System.out.println("test done!");
	}

}
