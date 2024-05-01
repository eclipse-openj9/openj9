package org.openj9.test.com.ibm.jit;

/*
 * Copyright IBM Corp. and others 1998
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


/*
 * This file is going to be copied to TestResources.jar.
 * Running tests in Test_JITHelpers need to add this file into -Xbootclasspath/a:"<path_to_this_file>"
 */

import java.io.FileInputStream;
import java.io.InputStream;
import com.ibm.jit.JITHelpers;

public class Test_JITHelpersImpl {

	/**
	 * @tests com.ibm.jit.JITHelpers#getSuperclass(java.lang.Class)
	 */

	public static Class test_getSuperclassImpl(Class childClass) {
			return JITHelpers.getHelpers().getSuperclass(childClass);
	}

}

