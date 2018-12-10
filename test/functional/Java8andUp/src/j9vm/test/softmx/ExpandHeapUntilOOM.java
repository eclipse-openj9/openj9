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

package j9vm.test.softmx;

import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class ExpandHeapUntilOOM {
	/**
	 * maximum size we can force the heap size up to in MB
	 */
	private static final int MAX_OBJECTS = 5000;

	/**
	 * size of the objects we used for force the heap size up
	 */
	private static final int OBJECT_SIZE = 1024*1024;

	public static void testExpandHeapUntilOOM () {
		Object[] myObjects = new Object[MAX_OBJECTS];

		// stop when we get an OOM
		for (int i=0; i< MAX_OBJECTS; i++){
			try {
				myObjects[i] = new byte[OBJECT_SIZE];
			} catch (OutOfMemoryError e){
				// at this point we stop
				// avoid object allocation here
				break;
			}
		}
	}
}
