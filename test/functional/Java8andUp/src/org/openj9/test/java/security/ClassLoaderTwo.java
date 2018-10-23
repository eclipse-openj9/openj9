package org.openj9.test.java.security;

/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Set;
import java.util.HashSet;

// This classloader loads classes Parent and Dummy.
public class ClassLoaderTwo extends ClassLoader {
	private final Set<String> names = new HashSet<>();
	private ClassLoader clOne;
	public ClassLoaderTwo(ClassLoader cl, String... names) {
		clOne = cl;
		for (String name : names) {
			this.names.add(name);
		}
	}
	
	protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
		if (name.equals("org.openj9.test.java.security.ChildMHLookupField")) {
			return clOne.loadClass(name);
		}
		if (!names.contains(name)) {
			return super.loadClass(name, resolve);
		}
		Class<?> result = findLoadedClass(name);
		if (result == null) {
			if (name.equals("org.openj9.test.java.security.Parent")) {
				loadClass("org.openj9.test.java.security.Dummy", resolve);
			}
			String filename = name.replace('.', '/') + ".class";
			try (InputStream data = getResourceAsStream(filename)) {
				if (data == null) {
					throw new ClassNotFoundException();
				}
				try (ByteArrayOutputStream buffer = new ByteArrayOutputStream()) {
					int b;
					do {
						b = data.read();
						if (b >= 0) {
							buffer.write(b);
						}
					} while (b >= 0);
					byte[] bytes = buffer.toByteArray();
					result = defineClass(name, bytes, 0, bytes.length);
				}
			} catch (IOException e) {
				throw new ClassNotFoundException("Error reading" + filename, e);
			}
		}
		if (resolve) {
			resolveClass(result);
		}
		return result;
	}
}
