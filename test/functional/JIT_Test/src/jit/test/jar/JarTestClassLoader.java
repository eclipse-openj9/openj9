/*
 * Copyright IBM Corp. and others 2001
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
package jit.test.jar;

import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import org.openj9.test.util.CompilerAccess;
import org.testng.Assert;

class JarTestClassLoader extends ZipTestClassLoader {

	protected Class<?> defineClass(ZipFile file, ZipEntry entry, String className) {
		Class<?> clazz = super.defineClass(file, entry, className);
		if (clazz != null) {
			if (CompilerAccess.compileClass(clazz) == false) {
				Assert.fail("Compilation of " + className + " failed -- aborting");
				System.exit(1);
			}
		}
		return clazz;
	}

}
