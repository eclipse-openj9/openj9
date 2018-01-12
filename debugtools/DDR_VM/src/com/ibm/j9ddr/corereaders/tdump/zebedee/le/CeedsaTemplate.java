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

public final class CeedsaTemplate {

    public static int length() {
        return 128;
    }

    public static long getCeedsabkc(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 4);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsabkc$offset() {
        return 4;
    }
    public static int getCeedsabkc$length() {
        return 32;
    }
    public static long getCeedsar14(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 12);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsar14$offset() {
        return 12;
    }
    public static int getCeedsar14$length() {
        return 32;
    }
    public static long getCeedsar15(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 16);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsar15$offset() {
        return 16;
    }
    public static int getCeedsar15$length() {
        return 32;
    }
    public static long getCeedsar4(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 36);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsar4$offset() {
        return 36;
    }
    public static int getCeedsar4$length() {
        return 32;
    }
    public static long getCeedsanab(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 76);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsanab$offset() {
        return 76;
    }
    public static int getCeedsanab$length() {
        return 32;
    }
    public static long getCeedsatran(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 100);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsatran$offset() {
        return 100;
    }
    public static int getCeedsatran$length() {
        return 32;
    }
    public static long getCeedsamode(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 108);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsamode$offset() {
        return 108;
    }
    public static int getCeedsamode$length() {
        return 32;
    }
}
