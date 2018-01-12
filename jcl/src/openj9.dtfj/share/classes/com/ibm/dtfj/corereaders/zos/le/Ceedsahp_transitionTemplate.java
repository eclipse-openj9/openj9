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

public final class Ceedsahp_transitionTemplate {

    public static int length() {
        return 108;
    }

    public static long getCeedsahp_trtype(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 8);
        return inputStream.readInt();
    }
    public static int getCeedsahp_trtype$offset() {
        return 8;
    }
    public static int getCeedsahp_trtype$length() {
        return 32;
    }
    public static long getCeedsahp_tran_ep(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 28);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsahp_tran_ep$offset() {
        return 28;
    }
    public static int getCeedsahp_tran_ep$length() {
        return 32;
    }
    public static long getCeedsahp_bkc(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 48);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsahp_bkc$offset() {
        return 48;
    }
    public static int getCeedsahp_bkc$length() {
        return 32;
    }
    public static long getCeedsahp_retaddr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 60);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getCeedsahp_retaddr$offset() {
        return 60;
    }
    public static int getCeedsahp_retaddr$length() {
        return 32;
    }
}
