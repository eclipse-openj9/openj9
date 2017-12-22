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

public final class Caa64Template implements CaaTemplate {

    public int length() {
        return 2304;
    }

    public long getCeecaalevel(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 868);
        return inputStream.readByte();
    }
    public int getCeecaalevel$offset() {
        return 868;
    }
    public int getCeecaalevel$length() {
        return 8;
    }
    public long getCeecaaddsa(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 896);
        return inputStream.readLong();
    }
    public int getCeecaaddsa$offset() {
        return 896;
    }
    public int getCeecaaddsa$length() {
        return 64;
    }
    public long getCeecaaedb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 904);
        return inputStream.readLong();
    }
    public int getCeecaaedb$offset() {
        return 904;
    }
    public int getCeecaaedb$length() {
        return 64;
    }
    public long getCeecaathdid(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 976);
        return inputStream.readLong();
    }
    public int getCeecaathdid$offset() {
        return 976;
    }
    public int getCeecaathdid$length() {
        return 64;
    }
    public long getCeecaarcb(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 984);
        return inputStream.readLong();
    }
    public int getCeecaarcb$offset() {
        return 984;
    }
    public int getCeecaarcb$length() {
        return 64;
    }
    public long getCeecaa_stackdirection(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1053);
        return inputStream.readByte();
    }
    public int getCeecaa_stackdirection$offset() {
        return 1053;
    }
    public int getCeecaa_stackdirection$length() {
        return 8;
    }
    public long getCeecaaerrcm(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1224);
        return inputStream.readLong();
    }
    public int getCeecaaerrcm$offset() {
        return 1224;
    }
    public int getCeecaaerrcm$length() {
        return 64;
    }
    public long getCeecaavba(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1296);
        return inputStream.readLong();
    }
    public int getCeecaavba$offset() {
        return 1296;
    }
    public int getCeecaavba$length() {
        return 64;
    }
    public long getCeecaatcs(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 1304);
        return inputStream.readLong();
    }
    public int getCeecaatcs$offset() {
        return 1304;
    }
    public int getCeecaatcs$length() {
        return 64;
    }
    public long getCeecaasmcb(ImageInputStream inputStream, long address) throws IOException {
        throw new Error("field ceecaasmcb does not exist in template Caa64Template");
    }
    public int getCeecaasmcb$offset() {
        throw new Error("field ceecaasmcb does not exist in template Caa64Template");
    }
    public int getCeecaasmcb$length() {
        throw new Error("field ceecaasmcb does not exist in template Caa64Template");
    }
    public long getCeecaa_stackfloor(ImageInputStream inputStream, long address) throws IOException {
        throw new Error("field ceecaa_stackfloor does not exist in template Caa64Template");
    }
    public int getCeecaa_stackfloor$offset() {
        throw new Error("field ceecaa_stackfloor does not exist in template Caa64Template");
    }
    public int getCeecaa_stackfloor$length() {
        throw new Error("field ceecaa_stackfloor does not exist in template Caa64Template");
    }
}
