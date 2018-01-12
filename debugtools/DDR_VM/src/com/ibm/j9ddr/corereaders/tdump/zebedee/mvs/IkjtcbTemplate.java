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

public final class IkjtcbTemplate {

    public static int length() {
        return 344;
    }

    public static long getTcbrbp(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getTcbrbp$offset() {
        return 0;
    }
    public static int getTcbrbp$length() {
        return 32;
    }
    public static long getTcbcmp(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 16);
        return inputStream.readInt();
    }
    public static int getTcbcmp$offset() {
        return 16;
    }
    public static int getTcbcmp$length() {
        return 32;
    }
    public static long getTcbgrs(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 48);
        throw new Error("request for long value for field tcbgrs which has length of 64");
    }
    public static int getTcbgrs$offset() {
        return 48;
    }
    public static int getTcbgrs$length() {
        return 512;
    }
    public static long getTcbtcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 116);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getTcbtcb$offset() {
        return 116;
    }
    public static int getTcbtcb$length() {
        return 32;
    }
    public static long getTcbrtwa(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 224);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getTcbrtwa$offset() {
        return 224;
    }
    public static int getTcbrtwa$length() {
        return 32;
    }
    public static long getTcbstcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 312);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getTcbstcb$offset() {
        return 312;
    }
    public static int getTcbstcb$length() {
        return 32;
    }
    public static long getTcbcelap(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 324);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getTcbcelap$offset() {
        return 324;
    }
    public static int getTcbcelap$length() {
        return 32;
    }
}
