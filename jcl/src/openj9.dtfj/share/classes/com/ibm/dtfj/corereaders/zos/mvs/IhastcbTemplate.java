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

public final class IhastcbTemplate {

    public static int length() {
        return 1048;
    }

    public static long getStcblsdp(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 116);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getStcblsdp$offset() {
        return 116;
    }
    public static int getStcblsdp$length() {
        return 32;
    }
    public static long getStcbestk(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 128);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getStcbestk$offset() {
        return 128;
    }
    public static int getStcbestk$length() {
        return 32;
    }
    public static long getStcbotcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 216);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getStcbotcb$offset() {
        return 216;
    }
    public static int getStcbotcb$length() {
        return 32;
    }
    public static long getStcbg64h(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 416);
        throw new Error("request for long value for field stcbg64h which has length of 64");
    }
    public static int getStcbg64h$offset() {
        return 416;
    }
    public static int getStcbg64h$length() {
        return 512;
    }
    public static long getStcblaa(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 516);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getStcblaa$offset() {
        return 516;
    }
    public static int getStcblaa$length() {
        return 32;
    }
}
