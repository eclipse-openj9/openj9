/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package org.openj9.test.automodule;

import java.io.DataInputStream;
import java.io.InputStream;
import java.io.IOException;

public class CustomClassLoader extends ClassLoader {
	public CustomClassLoader() {
		super(null);
	}

	protected Class<?> findClass(String className) throws ClassNotFoundException {
		if ("org.openj9.test.automodule.Dummy".equalsIgnoreCase(className)) {
			String classFile = className.replace('.', '/') + ".class";
			DataInputStream dis = null;
			try {
				InputStream is = getClass().getClassLoader().getResourceAsStream(classFile);
				if (is == null) {
					throw new ClassNotFoundException();
				}
				byte classRep[] = new byte[is.available()];
				dis = new DataInputStream(is);
				dis.readFully(classRep);
				return defineClass(className, classRep, 0, classRep.length);
			} catch (IOException e) {
				throw new ClassNotFoundException();
			} finally {
				if (dis != null) {
					try {
						dis.close();
					} catch (IOException e) {
					}
				}
			}
		} else {
			return super.loadClass(className);
		}
	}
}
