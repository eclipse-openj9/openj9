/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.*;

import java.io.*;
import java.util.logging.*;

/**
 * This class emulates the Ceexdsaf macro (Ph variant). Most of the comments are from Ceexdsaf.
 * The purpose of this class is to derive the previous stackframe from the passed address and
 * stackframe format.
 */
public class Ceexdsaf {
    /** Transition is stack swap */
    private static final long Ceedsahp_UpToDown = 2;
    /** Transition is stack swap */
    private static final long Ceedsahp_DownToUp = 3;
    /** Desired output DSA address of previous frame */
    long DSA_Prev;
    /** Format of previous frame */
    int DSA_Format;
    /** The AddressSpaceImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** True if this is a 64-bit thread */
    private boolean is64bit;
    /** Follow the rules leniently */
    //private static final boolean lenient = Boolean.getBoolean("zebedee.dsa.lenient");
    private static final boolean lenient = true;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    Ceexdsaf(AddressSpace space, long DSA_In, int dsa_fmt, boolean is64bit) throws IOException {
        log.fine("enter Ceexdsaf for dsa " + hex(DSA_In));
        /* Create any templates we need if not already created */
        CeedsahpTemplate ceedsahpTemplate = null;
        Ceedsahp_transitionTemplate ceedsahp_transitionTemplate = null;
        if (is64bit) {
            ceedsahpTemplate = new Ceedsahp64Template();
            ceedsahp_transitionTemplate = new Ceedsahp_transition64Template();
        } else {
            ceedsahpTemplate = new Ceedsahp32Template();
            ceedsahp_transitionTemplate = new Ceedsahp_transition32Template();
        }
        inputStream = space.getImageInputStream();
        boolean isTransitional;
        DSA_Format = dsa_fmt;   // Default to same as parent
        try {
            if (DSA_Format == Caa.CEECAASTACK_UP) {
                /* Stack Format is UP. If passed DSA is a transitional, return
                 * Ceedsahp_Bkc from tx area. Else, old-style backchain */
                long ceedsabkc = CeedsaTemplate.getCeedsabkc(inputStream, DSA_In);
                if (ceedsabkc == DsaStackFrame.Ceedsahp_UpTran_Ind) {
                    isTransitional = true;
                    /* If the transitional frame is for "DownToUp" (3), reset the
                     * stack format to Down and use the Ceedsahp_Bkc field. */
                    long ceedsatran = CeedsaTemplate.getCeedsatran(inputStream, DSA_In);
                    long ceedsahp_trtype = ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsatran);
                    if (ceedsahp_trtype == Ceedsahp_DownToUp) {
                        DSA_Prev = ceedsahp_transitionTemplate.getCeedsahp_bkc(inputStream, ceedsatran);
                        DSA_Format = Caa.CEECAASTACK_DOWN;
                    } else {
                        DSA_Prev = ceedsabkc;
                    }
                } else {
                    /* Old-style backchain */
                    DSA_Prev = ceedsabkc;
                }
            } else {
                /* Stack Format is DOWN. If passed DSA is a transitional UptoDown (2),
                 * reset the DSA format to Up */
                long ceedsahpr7 = ceedsahpTemplate.getCeedsahpr7(inputStream, DSA_In);
                log.fine("ceedsahpr7 = " + hex(ceedsahpr7));
                if (!is64bit)
                    ceedsahpr7 &= 0x7fffffff;
                if (ceedsahpr7 == 0) {
                    long ceedsahptran = ceedsahpTemplate.getCeedsahptran(inputStream, DSA_In);
                    log.fine("ceedsahptran = " + hex(ceedsahptran));
                    long ceedsahp_trtype = ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsahptran);
                    log.fine("ceedsahp_trtype = " + hex(ceedsahp_trtype));
                    if (ceedsahp_trtype == Ceedsahp_UpToDown) {
                        DSA_Format = Caa.CEECAASTACK_UP;
                    }
                    /* Now return Ceedsahp_Bkc from tx area. Else, XPLINK-style backchain */
                    isTransitional = true;
                    DSA_Prev = ceedsahp_transitionTemplate.getCeedsahp_bkc(inputStream, ceedsahptran);
                    log.fine("DSA_Prev = " + hex(DSA_Prev));
                } else {
                    /* Now do an XPL backchain. First we need the stackframe owner`s
                     * Entry Point. From this we can retrieve the SF`s DSA size. This
                     * is then added to the current DSA to locate the previous stackframe */
                    try {
                        long Entry_Temp = DsaStackFrame.hpclEntryPoint(space, DSA_In, ceedsahpr7, ceedsahpTemplate);
                        log.fine("Entry_Temp = " + hex(Entry_Temp));
                        if (Entry_Temp == 0) {
                            /* Unable to find entry. Leave with zero DSA as previous */
                            DSA_Prev = 0;
                        } else {
                            /* Check if the stackframe owner uses Alloca. If so, use the saved
                             * backchain pointer in the DSA+0 to find previous DSA. If not, add
                             * current function`s DSA size to current DSA to find the previous */
                            int dsaFlags = space.readInt(Entry_Temp - 4);
                            if ((dsaFlags & 4) == 4) {
                                DSA_Prev = ceedsahpTemplate.getCeedsahpr4(inputStream, DSA_In);
                            } else {
                                int dsaSize = dsaFlags & 0xfffffff0;
                                DSA_Prev = DSA_In + dsaSize;
                            }
                        }
                    } catch (IOException e) {
                        log.finer("caught exception: " + e);
                        if (lenient) {
                            /* No idea why this works! */
                            DSA_Prev = ceedsahpTemplate.getCeedsahpr4(inputStream, DSA_In);
                        } else {
                            throw e;
                        }
                    }
                }
            }
        /* Propagate IO exceptions, all others are fatal errors */
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            log.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.tdump.zebedee.le.Ceexdsaf", "Ceexdsaf","Unexpected Exception", e);
            throw new Error("Unexpected Exception: " + e);
        }
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }
}
