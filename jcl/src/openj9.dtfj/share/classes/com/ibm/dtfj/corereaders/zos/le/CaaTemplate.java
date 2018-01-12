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

public interface CaaTemplate {

    public int length();
    public long getCeecaalevel(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaalevel$offset();
    public int getCeecaalevel$length();
    public long getCeecaaddsa(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaaddsa$offset();
    public int getCeecaaddsa$length();
    public long getCeecaaedb(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaaedb$offset();
    public int getCeecaaedb$length();
    public int getCeecaathdid(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaathdid$offset();
    public int getCeecaathdid$length();
    public long getCeecaarcb(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaarcb$offset();
    public int getCeecaarcb$length();
    public long getCeecaa_stackdirection(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaa_stackdirection$offset();
    public int getCeecaa_stackdirection$length();
    public long getCeecaaerrcm(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaaerrcm$offset();
    public int getCeecaaerrcm$length();
    public long getCeecaavba(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaavba$offset();
    public int getCeecaavba$length();
    public long getCeecaasmcb(ImageInputStream inputStream, long address) throws IOException;
    public int getCeecaasmcb$offset();
    public int getCeecaasmcb$length();
}
