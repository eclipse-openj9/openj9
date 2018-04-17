/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package org.openj9.test.varhandle;

import java.nio.ByteBuffer;

abstract class ViewHelper {
	static void reset(ByteBuffer buffer) {
		buffer.put(0, (byte)1);
		buffer.put(1, (byte)2);
		buffer.put(2, (byte)3);
		buffer.put(3, (byte)4);
		buffer.put(4, (byte)5);
		buffer.put(5, (byte)6);
		buffer.put(6, (byte)7);
		buffer.put(7, (byte)8);
		buffer.put(8, (byte)9);
		buffer.put(9, (byte)10);
		buffer.put(10, (byte)11);
		buffer.put(11, (byte)12);
		buffer.put(12, (byte)13);
		buffer.put(13, (byte)14);
		buffer.put(14, (byte)15);
		buffer.put(15, (byte)16);
	}
}
