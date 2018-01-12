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

public final class Ceedsahp64Template implements CeedsahpTemplate {

    public int length() {
        return 2177;
    }

    public long getCeedsahpr4(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 2048);
        return inputStream.readLong();
    }
    public int getCeedsahpr4$offset() {
        return 2048;
    }
    public int getCeedsahpr4$length() {
        return 64;
    }
    public long getCeedsahpr6(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 2064);
        return inputStream.readLong();
    }
    public int getCeedsahpr6$offset() {
        return 2064;
    }
    public int getCeedsahpr6$length() {
        return 64;
    }
    public long getCeedsahpr7(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 2072);
        return inputStream.readLong();
    }
    public int getCeedsahpr7$offset() {
        return 2072;
    }
    public int getCeedsahpr7$length() {
        return 64;
    }
    public long getCeedsahptran(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + 2160);
        return inputStream.readLong();
    }
    public int getCeedsahptran$offset() {
        return 2160;
    }
    public int getCeedsahptran$length() {
        return 64;
    }
}
