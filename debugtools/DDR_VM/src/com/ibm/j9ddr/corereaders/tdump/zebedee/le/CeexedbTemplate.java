/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

public interface CeexedbTemplate {

    public int length();
    public long getCeeedboptcb(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedboptcb$offset();
    public int getCeeedboptcb$length();
    public long getCeeedbenvar(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedbenvar$offset();
    public int getCeeedbenvar$length();
    public long getCeeedb_qpcb(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedb_qpcb$offset();
    public int getCeeedb_qpcb$length();
    public long getCeeedbdba(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedbdba$offset();
    public int getCeeedbdba$length();
    public long getCeeedb_dlcb_first(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedb_dlcb_first$offset();
    public int getCeeedb_dlcb_first$length();
    public long getCeeedbensm(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedbensm$offset();
    public int getCeeedbensm$length();
    public long getCeeedb_ceeosigr(ImageInputStream inputStream, long address) throws IOException;
    public int getCeeedb_ceeosigr$offset();
    public int getCeeedb_ceeosigr$length();
}
