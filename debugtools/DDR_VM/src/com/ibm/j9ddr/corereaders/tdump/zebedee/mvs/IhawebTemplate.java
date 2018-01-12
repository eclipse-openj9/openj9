/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.tdump.zebedee.mvs;

import java.io.IOException;
import javax.imageio.stream.ImageInputStream;

// Template for zOS Work Element Block (IHAWEB) data structure. See z/OS MVS Data Areas Volume 3.
public final class IhawebTemplate {

	public static int length() {
		return 128;
	}

	public static byte getWebflag2(ImageInputStream inputStream, long address) throws IOException {
		inputStream.seek(address + 6);
		return inputStream.readByte();
	}
	public static int getWebflag2$offset() {
		return 6;
	}
	public static int getWebflag2$length() {
		return 1;
	}
}
