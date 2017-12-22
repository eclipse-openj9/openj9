/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

import javax.imageio.stream.ImageInputStream;
import java.io.IOException;

/* This class was generated automatically by com.ibm.zebedee.util.Xml2Java */

public final class BpxzotcbTemplate {

    public static int length() {
        return 352;
    }

    public static long getOtcbptregsinusta(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 30);
        inputStream.setBitOffset(1);
        long result = inputStream.readBits(1);
        result <<= 63;
        result >>= 63;
        return result;
    }
    public static int getOtcbptregsinusta$offset() {
        return 30;
    }
    public static int getOtcbptregsinusta$length() {
        return 1;
    }
    public static long getOtcbcofptr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 56);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getOtcbcofptr$offset() {
        return 56;
    }
    public static int getOtcbcofptr$length() {
        return 32;
    }
    public static long getOtcbppsdptr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 84);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getOtcbppsdptr$offset() {
        return 84;
    }
    public static int getOtcbppsdptr$length() {
        return 32;
    }
    public static long getOtcbthli(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 188);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getOtcbthli$offset() {
        return 188;
    }
    public static int getOtcbthli$length() {
        return 32;
    }
}
