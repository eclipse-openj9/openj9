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
package com.ibm.dtfj.corereaders.zos.mvs;

import javax.imageio.stream.ImageInputStream;
import java.io.IOException;

/* This class was generated automatically by com.ibm.dtfj.corereaders.zos.util.Xml2Java */

public final class Lsestate1Template {

    public static int length() {
        return 296;
    }

    public static long getLses1grs(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        throw new Error("request for long value for field lses1grs which has length of 128");
    }
    public static int getLses1grs$offset() {
        return 0;
    }
    public static int getLses1grs$length() {
        return 1024;
    }
    public static long getLses1pasn(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 134);
        long result = inputStream.readBits(16);
        result <<= 48;
        result >>= 48;
        return result;
    }
    public static int getLses1pasn$offset() {
        return 134;
    }
    public static int getLses1pasn$length() {
        return 16;
    }
    public static long getLses1pswh(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 136);
        return inputStream.readLong();
    }
    public static int getLses1pswh$offset() {
        return 136;
    }
    public static int getLses1pswh$length() {
        return 64;
    }
    public static long getLses1_ed1(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 288);
        return inputStream.readLong();
    }
    public static int getLses1_ed1$offset() {
        return 288;
    }
    public static int getLses1_ed1$length() {
        return 64;
    }
}
