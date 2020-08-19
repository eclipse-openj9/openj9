/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.types.U16;

public class J9UTF8Helper {

	// Do not use J9UTF8.SIZEOF here in order to maintain compatibility
	// with older core files.
	public static final int J9UTF8_HEADER_SIZE = U16.SIZEOF;

	public static U8Pointer dataEA(J9UTF8Pointer utf8pointer) throws CorruptDataException
	{
		return U8Pointer.cast(utf8pointer).add(J9UTF8_HEADER_SIZE);
	}

	public static String stringValue(J9UTF8Pointer utf8pointer) throws CorruptDataException
	{
		byte[] buffer = new byte[utf8pointer.length().intValue()];
		dataEA(utf8pointer).getBytesAtOffset(0, buffer);
		return new String(buffer, StandardCharsets.UTF_8);
	}

}
