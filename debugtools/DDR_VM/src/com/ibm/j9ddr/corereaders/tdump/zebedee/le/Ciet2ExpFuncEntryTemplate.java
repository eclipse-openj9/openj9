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
package com.ibm.j9ddr.corereaders.tdump.zebedee.le;

import javax.imageio.stream.ImageInputStream;
import java.io.IOException;

/* This class was generated automatically by com.ibm.zebedee.util.Xml2Java */

public final class Ciet2ExpFuncEntryTemplate {

    public static int length() {
        return 36;
    }

    public static long getCiet2_exp_func_offset(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 4);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCiet2_exp_func_offset$offset() {
        return 4;
    }
    public static int getCiet2_exp_func_offset$length() {
        return 32;
    }
    public static long getCiet2_exp_func_name_addr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 8);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCiet2_exp_func_name_addr$offset() {
        return 8;
    }
    public static int getCiet2_exp_func_name_addr$length() {
        return 32;
    }
    public static long getCiet2_exp_func_is_addr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 12);
        inputStream.setBitOffset(1);
        long result = inputStream.readBits(1);
        result <<= 63;
        result >>= 63;
        return result;
    }
    public static int getCiet2_exp_func_is_addr$offset() {
        return 12;
    }
    public static int getCiet2_exp_func_is_addr$length() {
        return 1;
    }
    public static long getCiet2_exp_ada_is_addr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 12);
        inputStream.setBitOffset(2);
        long result = inputStream.readBits(1);
        result <<= 63;
        result >>= 63;
        return result;
    }
    public static int getCiet2_exp_ada_is_addr$offset() {
        return 12;
    }
    public static int getCiet2_exp_ada_is_addr$length() {
        return 1;
    }
    public static long getCiet2_exp_func_ada_offset(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 20);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCiet2_exp_func_ada_offset$offset() {
        return 20;
    }
    public static int getCiet2_exp_func_ada_offset$length() {
        return 32;
    }
}
