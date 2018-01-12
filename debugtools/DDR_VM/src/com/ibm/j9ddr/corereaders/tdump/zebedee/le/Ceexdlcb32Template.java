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

public final class Ceexdlcb32Template implements CeexdlcbTemplate {

    public int length() {
        return 96;
    }

    public long getDlcbnextptr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getDlcbnextptr$offset() {
        return 0;
    }
    public int getDlcbnextptr$length() {
        return 32;
    }
    public long getDlcbwsaptr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 20);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getDlcbwsaptr$offset() {
        return 20;
    }
    public int getDlcbwsaptr$length() {
        return 32;
    }
    public long getDlcbnamelen(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 60);
        long result = inputStream.readBits(16);
        result <<= 48;
        result >>= 48;
        return result;
    }
    public int getDlcbnamelen$offset() {
        return 60;
    }
    public int getDlcbnamelen$length() {
        return 16;
    }
    public long getDlcbnameptr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 64);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getDlcbnameptr$offset() {
        return 64;
    }
    public int getDlcbnameptr$length() {
        return 32;
    }
    public long getDlcbiewbcie(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 68);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getDlcbiewbcie$offset() {
        return 68;
    }
    public int getDlcbiewbcie$length() {
        return 32;
    }
}
