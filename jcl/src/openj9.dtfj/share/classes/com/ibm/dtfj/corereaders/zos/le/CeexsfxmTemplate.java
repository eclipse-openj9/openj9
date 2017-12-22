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

public final class CeexsfxmTemplate {

    public static int length() {
        return 332;
    }

    public static long getSfxm_code_eyecatch(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        return inputStream.readLong();
    }
    public static int getSfxm_code_eyecatch$offset() {
        return 0;
    }
    public static int getSfxm_code_eyecatch$length() {
        return 64;
    }
    public static long getSfxm_code_return_pt(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 10);
        return inputStream.readInt();
    }
    public static int getSfxm_code_return_pt$offset() {
        return 10;
    }
    public static int getSfxm_code_return_pt$length() {
        return 32;
    }
    public static long getSfxm_next(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 68);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getSfxm_next$offset() {
        return 68;
    }
    public static int getSfxm_next$length() {
        return 32;
    }
    public static long getSfxm_parm_sf(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 300);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getSfxm_parm_sf$offset() {
        return 300;
    }
    public static int getSfxm_parm_sf$length() {
        return 32;
    }
}
