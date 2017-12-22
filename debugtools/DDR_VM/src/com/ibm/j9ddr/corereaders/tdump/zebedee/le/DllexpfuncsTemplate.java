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

public final class DllexpfuncsTemplate {

    public static int length() {
        return 12;
    }

    public static long getDllexpfuncscount(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        return inputStream.readInt();
    }
    public static int getDllexpfuncscount$offset() {
        return 0;
    }
    public static int getDllexpfuncscount$length() {
        return 32;
    }
    public static long getDllexpfuncsarray(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 4);
        return inputStream.readLong();
    }
    public static int getDllexpfuncsarray$offset() {
        return 4;
    }
    public static int getDllexpfuncsarray$length() {
        return 64;
    }
    public static long getDllexpfuncsname(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 4);
        return inputStream.readInt();
    }
    public static int getDllexpfuncsname$offset() {
        return 4;
    }
    public static int getDllexpfuncsname$length() {
        return 32;
    }
    public static long getDllexpfuncsaddr(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 8);
        return inputStream.readUnsignedInt() & 0xffffffffL;
    }
    public static int getDllexpfuncsaddr$offset() {
        return 8;
    }
    public static int getDllexpfuncsaddr$length() {
        return 32;
    }
}
