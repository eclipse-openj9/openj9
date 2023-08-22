/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 *******************************************************************************/
package org.openj9.test.lworld;

public class ValhallaUtils {
	/**
	 * Currently value type is built on JDK22, so use java file major version 66 for now.
	 * If moved this needs to be incremented to the next class file version. The check in j9bcutil_readClassFileBytes()
	 * against BCT_JavaMajorVersionShifted needs to be updated as well.
	 */
	static final int CLASS_FILE_MAJOR_VERSION = 66;

	/* workaround till the new ASM is released */
	static final int ACONST_INIT = 203;
	static final int WITHFIELD = 204;
	static final int ACC_IDENTITY = 0x20;
	static final int ACC_VALUE_TYPE = 0x040;
	static final int ACC_PRIMITIVE = 0x800;
}
