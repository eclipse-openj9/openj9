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

public final class Ceexqpcb64Template implements CeexqpcbTemplate {

    public int length() {
        return 1008;
    }

    public long getQpcb_eyecatcher(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 0);
        return inputStream.readInt();
    }
    public int getQpcb_eyecatcher$offset() {
        return 0;
    }
    public int getQpcb_eyecatcher$length() {
        return 32;
    }
    public long getQpcb_length(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 4);
        return inputStream.readInt();
    }
    public int getQpcb_length$offset() {
        return 4;
    }
    public int getQpcb_length$length() {
        return 32;
    }
    public long getQpcb_numpools(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 8);
        return inputStream.readInt();
    }
    public int getQpcb_numpools$offset() {
        return 8;
    }
    public int getQpcb_numpools$length() {
        return 32;
    }
    public long getQpcb_pool_data_array(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 48);
        throw new Error("request for long value for field qpcb_pool_data_array which has length of 960");
    }
    public int getQpcb_pool_data_array$offset() {
        return 48;
    }
    public int getQpcb_pool_data_array$length() {
        return 7680;
    }
}
