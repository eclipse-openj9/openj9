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

public final class BpxzustaTemplate {

    public static int length() {
        return 256;
    }

    public static long getUstapswg(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 16);
        throw new Error("request for long value for field ustapswg which has length of 16");
    }
    public static int getUstapswg$offset() {
        return 16;
    }
    public static int getUstapswg$length() {
        return 128;
    }
    public static long getUstagrs(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 32);
        throw new Error("request for long value for field ustagrs which has length of 64");
    }
    public static int getUstagrs$offset() {
        return 32;
    }
    public static int getUstagrs$length() {
        return 512;
    }
}
