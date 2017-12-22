/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/* This class was generated automatically by com.ibm.zebedee.util.Xml2Java */

public final class Caa32_11Template implements CaaTemplate {

    public int length() {
        return 2156;
    }

    public long getCeecaalevel(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 688);
        return inputStream.readByte();
    }
    public int getCeecaalevel$offset() {
        return 688;
    }
    public int getCeecaalevel$length() {
        return 8;
    }
    public long getCeecaaddsa(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 736);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaaddsa$offset() {
        return 736;
    }
    public int getCeecaaddsa$length() {
        return 32;
    }
    public long getCeecaaedb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 752);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaaedb$offset() {
        return 752;
    }
    public int getCeecaaedb$length() {
        return 32;
    }
    public int getCeecaathdid(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 828);
        return inputStream.readInt();
    }
    public int getCeecaathdid$offset() {
        return 828;
    }
    public int getCeecaathdid$length() {
        return 64;
    }
    public long getCeecaarcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 848);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaarcb$offset() {
        return 848;
    }
    public int getCeecaarcb$length() {
        return 32;
    }
    public long getCeecaa_stackfloor(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 868);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaa_stackfloor$offset() {
        return 868;
    }
    public int getCeecaa_stackfloor$length() {
        return 32;
    }
    public long getCeecaa_stackdirection(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 925);
        return inputStream.readByte();
    }
    public int getCeecaa_stackdirection$offset() {
        return 925;
    }
    public int getCeecaa_stackdirection$length() {
        return 8;
    }
    public long getCeecaasmcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1284);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaasmcb$offset() {
        return 1284;
    }
    public int getCeecaasmcb$length() {
        return 32;
    }
    public long getCeecaaerrcm(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1288);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaaerrcm$offset() {
        return 1288;
    }
    public int getCeecaaerrcm$length() {
        return 32;
    }
    public long getCeecaavba(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1364);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaavba$offset() {
        return 1364;
    }
    public int getCeecaavba$length() {
        return 32;
    }
    public long getCeecaatcs(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1368);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public int getCeecaatcs$offset() {
        return 1368;
    }
    public int getCeecaatcs$length() {
        return 32;
    }
}

