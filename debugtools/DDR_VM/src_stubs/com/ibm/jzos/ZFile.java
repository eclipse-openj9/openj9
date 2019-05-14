/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.jzos;

import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * This class is a stub that allows DDR to be compiled on platforms other than z/OS.
 */
public class ZFile {

	public static boolean exists(String name) {
		return false;
	}

	public ZFile(String name, String options) throws IOException {
		super();
		throw new FileNotFoundException();
	}

	public void close() throws IOException {
		return;
	}

	public int getLrecl() throws IOException {
		return 0;
	}

	public byte[] getPos() throws IOException {
		return new byte[] { 0 };
	}

	public int read(byte[] buffer) throws IOException {
		return 0;
	}

	public void setPos(byte[] position) throws IOException {
		return;
	}

}
