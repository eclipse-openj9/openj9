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

public final class CeexsancTemplate {

    public static int length() {
        return 256;
    }

    public static long getSanc_stack(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 16);
        return inputStream.readLong();
    }
    public static int getSanc_stack$offset() {
        return 16;
    }
    public static int getSanc_stack$length() {
        return 64;
    }
    public static long getSanc_bos(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 24);
        return inputStream.readLong();
    }
    public static int getSanc_bos$offset() {
        return 24;
    }
    public static int getSanc_bos$length() {
        return 64;
    }
    public static long getSanc_user_stack(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 48);
        return inputStream.readLong();
    }
    public static int getSanc_user_stack$offset() {
        return 48;
    }
    public static int getSanc_user_stack$length() {
        return 64;
    }
    public static long getSanc_user_floor(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 64);
        return inputStream.readLong();
    }
    public static int getSanc_user_floor$offset() {
        return 64;
    }
    public static int getSanc_user_floor$length() {
        return 64;
    }
}
