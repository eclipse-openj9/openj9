/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2006, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders.zos.le;

import javax.imageio.stream.ImageInputStream;
import java.io.IOException;

/* This class was generated automatically by com.ibm.dtfj.corereaders.zos.util.Xml2Java */

public final class Ciet2Template {

    public static int length() {
        return 36;
    }

    public static long getCiet2_version(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 8);
        return inputStream.readByte();
    }
    public static int getCiet2_version$offset() {
        return 8;
    }
    public static int getCiet2_version$length() {
        return 8;
    }
    public static long getCiet2_func_addr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 12);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCiet2_func_addr$offset() {
        return 12;
    }
    public static int getCiet2_func_addr$length() {
        return 32;
    }
    public static long getCiet2_func_count(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 16);
        return inputStream.readInt();
    }
    public static int getCiet2_func_count$offset() {
        return 16;
    }
    public static int getCiet2_func_count$length() {
        return 32;
    }
    public static long getCiet2_var_addr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 20);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCiet2_var_addr$offset() {
        return 20;
    }
    public static int getCiet2_var_addr$length() {
        return 32;
    }
    public static long getCiet2_var_count(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 24);
        return inputStream.readInt();
    }
    public static int getCiet2_var_count$offset() {
        return 24;
    }
    public static int getCiet2_var_count$length() {
        return 32;
    }
}
