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
import com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.*;
import com.ibm.j9ddr.corereaders.tdump.zebedee.util.*;

import java.util.Vector;
import java.util.logging.*;
import java.io.*;

/**
 * This class represents the LE view of an MVS {@link com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.Tcb}. Caa stands for
 * "Common Anchor Area" and this structure is the starting point for all the other LE
 * structures. For programs written in C, this is the most useful thread view because it
 * gives access to the C stack.
 *
 * @depend - - - com.ibm.zebedee.mvs.Tcb
 * @depend - - - com.ibm.zebedee.dumpreader.AddressSpace
 * @has - - - com.ibm.zebedee.le.DsaStackFrame
 * @has - - - com.ibm.zebedee.mvs.RegisterSet
 * @assoc - - - com.ibm.zebedee.le.Edb
 */

public class Caa {

    /** The address of the CAA structure */
    private long address;
    /** The TCB we are associated with */
    private Tcb tcb;
    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpace ImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** The Edb we belong to */
    private Edb edb;
    /** The current DSA */
    private DsaStackFrame currentFrame;
    /** The stack direction - down is XPLINK. Use this field rather than call ceecaa_stackdirection
     *  because the latter call is only valid for later levels of LE */
    private int stackdirection;
    /** The template we use, either 32 bit or 64 bit */
    private CaaTemplate caaTemplate;
    /** Address of LE Library Anchor Area (64-bit only) */
    private long laa;
    /** Is this a 64bit Caa? */
    private boolean is64bit;
    private static int hcomLength = new Ceexhcom32Template().length();
    private String whereFound;
    /** The failing registers or null if this is not a failing thread */
    private RegisterSet failingRegisters;
    /** The set of registers at the top of the stack (may be the same as failingRegisters!) */
    private RegisterSet registers;
    static final int CEECAASTACK_UP = 0;
    static final int CEECAASTACK_DOWN = 1;
    private static final int WARNING = 4;
    private static final int ERROR = 8;
    private static final long F1SA = 0xe6f1e2c1;
    private static final long F4SA = 0xe6f4e2c1;
    private static final long F5SA = 0xe6f5e2c1;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    private static int whereSkip = 0;

    static {
        /* Sometimes it's handy for debugging purposes to skip the first set
         * of registers found */
        String s = System.getProperty("zebedee.where.skip");
        if (s != null) {
            whereSkip = new Integer(s).intValue();
        }
    }

    /**
     * Returns an array containing all of the Caas in the given
     * {@link com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressSpace}
     */
    public static Caa[] getCaas(AddressSpace space) {
        return getCaas(space, null);
    }

    /**
     * Returns an array containing all of the Caas in the given
     * {@link com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressSpace} and which belong
     * to the given enclave (or all if enclave is null).
     */
    public static Caa[] getCaas(AddressSpace space, Edb edb) {
        Tcb[] tcbs = Tcb.getTcbs(space);
        if (tcbs == null) {
            log.fine("no tcbs found in address space " + space);
            return new Caa[0];
        }
        log.fine("found " + tcbs.length + " tcbs for address space " + space);
        Vector v = new Vector();
        for (int i = 0; i < tcbs.length; i++) {
            ProgressMeter.set("getting caas", (i*100)/tcbs.length);
            try {
                Caa caa = new Caa(tcbs[i]);
                if (edb == null || caa.getEdb().equals(edb)) {
                    v.add(caa);
                }
            } catch (CaaNotFound e) {
            }
        }
        return (Caa[])v.toArray(new Caa[0]);
    }

    /**
     * Create a new Caa from the given {@link com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.Tcb}. Note that not all
     * Tcbs have a Caa associated with them.
     * @throws CaaNotFound if there is no Caa associated with this Tcb.
     */
    public Caa(Tcb tcb) throws CaaNotFound {
        log.finer("creating Caa for Tcb at address " + hex(tcb.address()));
        this.tcb = tcb;
        space = tcb.space();
        inputStream = space.getImageInputStream();
        if (is32bitCaa()) {
            /* Now we know this is 32 bit, create the template */
            caaTemplate = new Caa32Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = new Integer(s).intValue();
                if (release >= 11) {
                    caaTemplate = new Caa32_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 32-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()));
        } else if (is64bitCaa()) {
            /* Now we know this is 64 bit, create the template and set flag in address space */
            is64bit = true;
            space.setIs64bit(true);
            caaTemplate = new Caa64Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = new Integer(s).intValue();
                if (release >= 11) {
                    caaTemplate = new Caa64_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 64-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()));
        } else if (is32bitCaaLastDitch()) {
            caaTemplate = new Caa32Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = new Integer(s).intValue();
                if (release >= 11) {
                    caaTemplate = new Caa32_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 32-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()) + " (last ditch)");
        } else {
            throw new CaaNotFound();
        }
    }

    /**
     * Try to locate the 32-bit caa pointer
     */
    private boolean is32bitCaa() {
        try {
            long celap = tcb.tcbcelap();
            log.finer("celap = 0x" + hex(celap));
            // http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/CEEV1180/1.15.1?SHELF=CEE2BK80&DT=20070428015544&CASE=
            int celap0 = space.readInt(celap);
            log.finer("celap0 = 0x" + hex(celap0));
            if (celap0 == 0)
                return false;
            address = space.readInt(celap0 + 0x20);
            int xxx = space.readInt(celap0 + 8);
            log.finer("caa address = " + hex(address));
            return validate();
        } catch (IOException e) {
            log.finer("caught exception: " + e);
            return false;
        }
    }

    /**
     * Try to locate the 64-bit caa pointer
     */
    private boolean is64bitCaa() {
        try {
            long stcb = tcb.tcbstcb();
            log.finer("stcb = 0x" + hex(stcb));
            laa = IhastcbTemplate.getStcblaa(inputStream, stcb);
            log.finer("laa = 0x" + hex(laa));
            long lca = CeexlaaTemplate.getCeelaa_lca64(inputStream, laa);
            log.finer("lca = 0x" + hex(lca));
            address = CeelcaTemplate.getCeelca_caa(inputStream, lca);
            log.finer("caa address = " + hex(address));
            return validate();
        } catch (IOException e) {
            log.finer("caught exception: " + e);
            return false;
        }
    }

    /**
     * Try to locate the 32-bit caa pointer (last ditch if all else fails!)
     */
    private boolean is32bitCaaLastDitch() {
        try {
            address = space.readInt(tcb.address() + 0x60);
            log.finer("tcb caa address = " + hex(address));
            if (address == 0)
                throw new IOException("oh");
            return validate();
        } catch (IOException e) {
            log.finer("caught exception: " + e);
            /*
            try {
                long r13 = space.readInt(tcb.address() + 0x64);
                log.finer("r13 = " + hex(r13));
                long rtm2ptr = tcb.tcbrtwa();
                if (rtm2ptr != 0) {
                    try {
                        log.finer("found some rtm2 registers");
                        RegisterSet regs = new RegisterSet();
                        long rtm2grs = rtm2ptr + Ihartm2aTemplate.getRtm2ereg$offset();
                        for (int i = 0; i < 16; i++) {
                            long low = space.readUnsignedInt(rtm2grs + i*4);
                            regs.setRegister(i, low);
                            log.finer("r[" + i + "] = " + hex(low));
                        }
                        long rtm2psw = rtm2ptr + Ihartm2aTemplate.getRtm2apsw$offset();
                        regs.setPSW(space.readLong(rtm2psw));
                    } catch (Exception ee) {
                        log.finer("oh well: " + ee);
                    }
                } else {
                    log.finer("nope, rtm2 is zero");
                }
            } catch (IOException ee) {
                log.finer("ho hum: " + ee);
            }
            try {
                Lse[] linkageStack = tcb.getLinkageStack();
                for (int i = 0; i < linkageStack.length; i++) {
                    Lse lse = linkageStack[i];
                    if (lse.lses1pasn() == space.getAsid()) {
                        RegisterSet regs = new RegisterSet();
                        if (lse.isZArchitecture() && (lse.lses1typ7() == Lse.LSED1PC || lse.lses1typ7() == Lse.LSED1BAKR)) {
                            log.finer("found some z arch registers");
                            regs.setPSW(lse.lses1pswh());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lses1grs(j));
                                log.finer("r[" + j + "] = " + hex(lse.lses1grs(j)));
                            }
                        } else {
                            log.finer("found some non z arch registers");
                            regs.setPSW(lse.lsespsw());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lsesgrs(j));
                                log.finer("r[" + j + "] = " + hex(lse.lsesgrs(j)));
                            }
                        }
                    } else {
                        log.finer("different asid: " + hex(lse.lses1pasn()));
                    }
                }
            } catch (Exception ee) {
                log.finer("ho hum: " + ee);
            }
            */
            return false;
        }
    }

    /**
     * Returns true if this is a 64-bit CAA.
     */
    public boolean is64bit() {
        return is64bit;
    }

    /**
     * Returns a string indicating where the registers were found. This is mainly for
     * debugging purposes.
     */
    public String whereFound() {
        return whereFound;
    }

    /**
     * @return the failing registers or null if this is not a failing thread. Currently
     * only return registers found in RTWA/RTM2.
     */
    public RegisterSet getFailingRegisters() {
        getCurrentFrame();  // force failingRegisters to be set (if any)
        return failingRegisters;
    }

    /**
     * @return the set of registers at the top of the stack (may be the same as getFailingRegisters!)
     */
    public RegisterSet getRegisters() {
        getCurrentFrame();  // force registers to be set
        return registers;
    }

    /**
     * Validates the caa address.
     */
    private boolean validate() {
        try {
            int eye1 = space.readInt(address - 0x18);
            int eye2 = space.readInt(address - 0x14) & 0xffff0000;

            if (eye1 == 0xc3c5c5c3 && eye2 == 0xc1c10000) { // CEECAA
                log.fine("found valid caa at " + hex(address));
                return true;
            }
        } catch (IOException e) {
            log.finer("caught exception: " + e);
        }
        return false;
    }

    /**
     * This class emulates the cel4rreg macro. Most of the comments are from there.
     */
    class Cel4rreg {

        long seghigh;
        long seglow;
        int p_dsafmt = -1;
        long p_dsaptr;
        RegisterSet regs;

        /**
         * Creates the instance and attempts to locate the registers.
         */
        Cel4rreg() {
            /* Debug option - before we do anything else, try using the old svcdump code */
            String useSvcdump = System.getProperty("zebedee.use.svcdump");
            if (useSvcdump != null && useSvcdump.equals("true")) {
                getRegistersFromSvcdump();
                return;
            }

            /*
             * Try and get the registers from the following locations:
             *
             * 1) RTM2 work area
             * 2) BPXGMSTA service
             * 3) linkage stack entries
             * 4) TCB
             * 5) Usta
             *
             * if any succeeds we return otherwise move to the next location.
             */

            int whereCount = 0;

            try {
                if ((regs = getRegistersFromRTM2()) != null && whereCount++ >= whereSkip) {
                    whereFound = "RTM2";
                    failingRegisters = regs;
                    registers = regs;
                    return;
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            /* If we still have not found a dsa, invoke kernel svs */
            try {
                if ((regs = getRegistersFromBPXGMSTA()) != null && whereCount++ >= whereSkip) {
                    whereFound = regs.whereFound();
                    if (whereFound == null)
                        whereFound = "BPXGMSTA";
                    if (tcb.tcbcmp() != 0)
                        failingRegisters = regs;
                    registers = regs;
                    return;
                }
            } catch (IOException e) {
                //throw new Error("oops: " + e);
            }
            try {
                if ((regs = getRegistersFromLinkageStack()) != null && whereCount++ >= whereSkip) {
                    whereFound = "Linkage";
                    if (tcb.tcbcmp() != 0)
                        failingRegisters = regs;
                    registers = regs;
                    return;
                }
            } catch (IOException e) {
                log.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.tdump.zebedee.le.Caa.Cel4rreg", "Cel4rreg","Unexepected exception", e);
                throw new Error("Unexpected IOException: " + e);
            }
            try {
                if ((regs = getRegistersFromTCB()) != null && whereCount++ >= whereSkip) {
                    whereFound = "TCB";
                    if (tcb.tcbcmp() != 0)
                        failingRegisters = regs;
                    registers = regs;
                    return;
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            try {
                if (is64bit) {
                    /* This is from celqrreg.plx370: "Get the save R4 from a NOSTACK call" */
                    long lca = CeexlaaTemplate.getCeelaa_lca64(inputStream, laa);
                    p_dsaptr = CeelcaTemplate.getCeelca_savstack(inputStream, lca);
                    log.fine("p_dsaptr from lca = " + hex(p_dsaptr));
                    p_dsafmt = stackdirection = CEECAASTACK_DOWN;
                    if (validateDSA() == 0 && whereCount++ >= whereSkip) {
                        whereFound = "LCA";
                        return;
                    }
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            /* Last ditch */
            try {
                if ((regs = getRegistersFromUsta()) != null && whereCount++ >= whereSkip) {
                    whereFound = regs.whereFound();
                    if (tcb.tcbcmp() != 0)
                        failingRegisters = regs;
                    registers = regs;
                    return;
                }
            } catch (IOException e) {
            }
            whereFound = "not found";
        }

        /**
         * Try and get the registers from the RTM2 work area. Returns null if none found. As a
         * side-effect it also sets the stackdirection.
         */
        private RegisterSet getRegistersFromRTM2() throws IOException {
            int level = ceecaalevel();
            log.finer("caa level is " + level);
            /* If the CAA level is 13 or greater, get stack direction from 
             * CAA.  For older releases or the dummy CAA, default stack 
             * direction to UP.
             */
            if (is64bit) {
                /* Always use downstack in 64-bit mode? */
                stackdirection = CEECAASTACK_DOWN;
                log.finer("stack direction is down");
            } else if (level >= 13) { /* If LE 2.10 or higher */
                /* Obtain dsa format from the CAA */
                stackdirection = ceecaa_stackdirection();
                log.finer("stack direction is " + (stackdirection == CEECAASTACK_UP ? "up" : "down"));
            } else {
                stackdirection = CEECAASTACK_UP;
                log.finer("stack direction is up");
            }
            if ((stackdirection == CEECAASTACK_DOWN) && !is64bit) {
                try {
                    long tempptr = ceecaasmcb();			//the ceecaasmcb call is not currently supported for 64 bit CAAs
                    seghigh = SmcbTemplate.getSmcb_dsbos(inputStream, tempptr);
                    seglow = CeexstkhTemplate.getStkh_stackfloor(inputStream, seghigh);
                } catch (Exception e) {
                    //throw new Error("oops: " + e);
                    return null;
                }
            }
            /* At this point, a valid CAA has been obtained. Access the RTM2 to obtain the DSA. */
            long rtm2ptr = tcb.tcbrtwa();
            if (rtm2ptr != 0) {
                try {
                    log.finer("found some rtm2 registers");
                    RegisterSet regs = new RegisterSet();
                    long rtm2grs = rtm2ptr + Ihartm2aTemplate.getRtm2ereg$offset();
                    long rtm2grshi = rtm2ptr + Ihartm2aTemplate.getRtm2g64h$offset();
                    for (int i = 0; i < 16; i++) {
                        long low = space.readUnsignedInt(rtm2grs + i*4);
                        long high = is64bit ? space.readUnsignedInt(rtm2grshi + i*4) : 0;
                        regs.setRegister(i, (high << 32) | low);
                    }
                    long rtm2psw = rtm2ptr + Ihartm2aTemplate.getRtm2apsw$offset();
                    regs.setPSW(space.readLong(rtm2psw));
                    if (registersValid(regs)) {
                        log.finer("found good dsa in rtm2");
                    } else {
                        log.finer("bad dsa in rtm2");
                        regs = null;
                    }
                    return regs;
                } catch (IOException e) {
                    throw e;
                } catch (Exception e) {
                    throw new Error("oops: " + e);
                }
            } else {
                log.finer("failed to get registers from rtm2");
                return null;
            }
        }

        /**
         * Validates the given register set with retry for down stack
         */
        private boolean registersValid(RegisterSet regs) throws IOException {
            if (regs == null)
                return false;
            p_dsafmt = stackdirection;
            if (p_dsafmt == CEECAASTACK_DOWN) {
                p_dsaptr = regs.getRegisterAsAddress(4);
                log.finer("p_dsaptr from reg 4 = " + hex(p_dsaptr));
            } else {
                p_dsaptr = regs.getRegisterAsAddress(13);
                log.finer("p_dsaptr from reg 13 = " + hex(p_dsaptr));
            }
            int lastrc = validateDSA();
            if (lastrc == 0) {
                log.finer("found valid dsa");
                return true;
            } else {
                if (stackdirection == CEECAASTACK_DOWN) {
                    p_dsaptr = regs.getRegisterAsAddress(13);
                    log.finer("p_dsaptr from reg 13 (again) = " + hex(p_dsaptr));
                    p_dsafmt = CEECAASTACK_UP;
                    lastrc = validateDSA();
                    if (lastrc == WARNING) {
                        lastrc = validateDSA();
                        if (lastrc == 0) {
                            log.finer("found valid dsa");
                            return true;
                        }
                    }
                }
                /* reset values */
                log.finer("p_dsaptr invalid so reset: " + hex(p_dsaptr));
                p_dsaptr = 0;
            }
            return false;
        }

        /**
         * Try and get the registers from the BPXGMSTA service.
         */
        private RegisterSet getRegistersFromBPXGMSTA() throws IOException {
            RegisterSet regs = tcb.getRegistersFromBPXGMSTA();
            if (is64bit)
                // celqrreg appears to always assume down stack
                stackdirection = CEECAASTACK_DOWN;
            if (registersValid(regs)) {
                log.finer("found good dsa in BPXGMSTA");
                return regs;
            } else {
                log.finer("BPX registers are invalid so keep looking");
                return null;
            }
        }

        /**
         * Try and get the registers from the linkage stack.
         */
        private RegisterSet getRegistersFromLinkageStack() throws IOException {
            log.finer("enter getRegistersFromLinkageStack");
            try {
                Lse[] linkageStack = tcb.getLinkageStack();
                /* If Linkage stack is empty, leave */
                if (linkageStack.length == 0) {
                    log.finer("empty linkage stack");
                    return null;
                }
                for (int i = 0; i < linkageStack.length; i++) {
                    Lse lse = linkageStack[i];
                    if (lse.lses1pasn() == space.getAsid()) {
                        RegisterSet regs = new RegisterSet();
                        if (lse.isZArchitecture() && (lse.lses1typ7() == Lse.LSED1PC || lse.lses1typ7() == Lse.LSED1BAKR)) {
                            log.finer("found some z arch registers");
                            regs.setPSW(lse.lses1pswh());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lses1grs(j));
                            }
                        } else {
                            log.finer("found some non z arch registers");
                            regs.setPSW(lse.lsespsw());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lsesgrs(j));
                            }
                        }
                        if (registersValid(regs)) {
                            log.finer("found good dsa in linkage stack");
                            return regs;
                        }
                    } else {
                        log.finer("different asid: " + hex(lse.lses1pasn()));
                    }
                }
            } catch (IOException e) {
                throw e;
            } catch (Exception e) {
                throw new Error("oops: " + e);
            }
            log.finer("could not find registers in linkage stack");
            return null;
        }

        /**
         * Try and get the registers from the TCB.
         */
        private RegisterSet getRegistersFromTCB() throws IOException {
            log.finer("getRegistersFromTCB");
            RegisterSet regs = tcb.getRegisters();
            if (registersValid(regs)) {
                log.finer("found good dsa in TCB");
                return regs;
            } else {
                return null;
            }
        }

        /**
         * Try and get the registers from the Usta. Note that this is a kind of last-ditch
         * thing and so no validation is done.
         */
        private RegisterSet getRegistersFromUsta() throws IOException {
            log.fine("enter getRegistersFromUsta");
            RegisterSet regs = tcb.getRegistersFromUsta();
            if (registersValid(regs)) {
                log.finer("found good dsa in Usta");
                return regs;
            } else {
                /* If there are more than three stack entries that's probably better than nothing */
                boolean isDownStack = stackdirection == CEECAASTACK_DOWN;
                long dsaptr;
                if (isDownStack) {
                    dsaptr = regs.getRegister(4);
                    log.finer("p_dsaptr from reg 4 = " + hex(p_dsaptr));
                } else {
                    dsaptr = regs.getRegister(13);
                    log.finer("p_dsaptr from reg 13 = " + hex(p_dsaptr));
                }
                try {
                    DsaStackFrame dsa = new DsaStackFrame(dsaptr, isDownStack, regs, space, Caa.this);
                    int count = 0;
                    for (; dsa != null; dsa = dsa.getParentFrame()) {
                    	if (++count > 3) {
                    		p_dsaptr = dsaptr;
                    		p_dsafmt = stackdirection;
                    		return regs;
                    	}
                    }
                } catch (IOException e) {
                } catch (AssertionError e) {
                }
            }
            return null;
        }

        /**
         * Try and get the registers using the old svcdump code. This is for debugging
         * purposes only. Uses reflection so there is no compilation dependency.
         */
        private void getRegistersFromSvcdump() {
        }

        /**
         * Validate the given DSA. Returns 0 if valid. Note because this is Java, we can't
         * modify the input parameters, so we use the instance variables instead and
         * val_dsa == p_dsaptr, val_dsafmt == p_dsafmt.
         */
        private int validateDSA() {
            log.finer("attempt to validate " + hex(p_dsaptr) + " on " + (p_dsafmt == CEECAASTACK_DOWN ? "down" : "up") + " stack");
            try {
                if (is64bit) {
                    assert laa != 0;
                    long l_sancptr = CeexlaaTemplate.getCeelaa_sanc64(inputStream, laa);
                    assert l_sancptr != 0;
                    long seghigh = CeexsancTemplate.getSanc_bos(inputStream, l_sancptr);
                    long seglow = 0;
                    long sanc_stack = CeexsancTemplate.getSanc_stack(inputStream, l_sancptr);
                    long sanc_user_stack = CeexsancTemplate.getSanc_user_stack(inputStream, l_sancptr);
                    if (sanc_stack == sanc_user_stack) {
                        /* Get Stackfloor from sanc */
                        seglow = CeexsancTemplate.getSanc_user_floor(inputStream, l_sancptr);
                    } else {
                        /* Get StackFloor from LAA */
                        seglow = CeexlaaTemplate.getCeelaa_stackfloor64(inputStream, laa);
                    }
                    if (p_dsaptr < seghigh && (p_dsaptr + 0x800) >= seglow && (p_dsaptr & 0xf) == 0) {
                        log.finer("dsa " + hex(p_dsaptr) + " is within seglow = " + hex(seglow) + " seghigh = " + hex(seghigh));
                        return 0;
                    } else {
                        log.finer("dsa " + hex(p_dsaptr) + " is NOT within seglow = " + hex(seglow) + " seghigh = " + hex(seghigh));
                        return ERROR;
                    }
                }
                if (p_dsafmt == CEECAASTACK_DOWN) {
                    /* the check for being in the current segment is commented out */
                } else {
                    if (is64bit)
                        return ERROR;
                    long tptr = ceecaaerrcm();
                    /* Chicken egg situation */
                    //assert !space.is64bit();
                    /* If the input DSA address is within the HCOM and double word aligned, 
                     * assume that it is good. */
                    if (p_dsaptr < (tptr + hcomLength) && p_dsaptr >= tptr && (p_dsaptr & 7) == 0) {
                        log.finer("upstack dsa " + hex(p_dsaptr) + " is inside hcom");
                        return 0;
                    }
                }
                long ddsa = ceecaaddsa();
                long dsaptr = p_dsaptr;
                int dsafmt8 = p_dsafmt;
                long slowdsaptr = p_dsaptr;
                int slowdsafmt8 = p_dsafmt;
                for (boolean slow = false;; slow = !slow) {
                    Ceexdsaf dsaf = new Ceexdsaf(space, dsaptr, dsafmt8, is64bit);
                    /* If the stack direction is down but we are validating an upstack DSA
                     * and the current DSA is inside the current segment of the down stack,
                     * assume this must be a OS_NOSTACK call, return WARNING and replace
                     * input DSA and DSAFmt with R4 value from this DSA */
                    log.finer("looping with dsa = " + hex(dsaptr));
                    if (stackdirection == CEECAASTACK_DOWN && p_dsafmt == CEECAASTACK_UP &&
                            dsaptr < seghigh && dsaptr >= seglow) {
                        p_dsaptr = CeedsaTemplate.getCeedsar4(inputStream, dsaptr);
                        p_dsafmt = CEECAASTACK_DOWN;
                        log.finer("warning, try switching to down stack");
                        return WARNING;
                    }
                    long callers_dsaptr = dsaf.DSA_Prev;
                    dsafmt8 = dsaf.DSA_Format;
                    /* If we are not able to backchain any farther or we have encountered
                     * a linkage stack, assume that the input DSA address is bad. */
                    if (callers_dsaptr == 0 || callers_dsaptr == F1SA) {
                        log.finer("cannot backchain futher because " + (callers_dsaptr == 0 ? "zero" : "linkage stack") + " found");
                        return ERROR;
                    }
                    /* If we were able to backchain to the dummy DSA, the input DSA address
                     * must be good. */
                    if (callers_dsaptr == ddsa) {
                        log.finer("dummy dsa reached");
                        return 0;
                    }
                    /* If we backchained across a stack transition, assume that the input
                     * DSA address is good. */
                    if (dsafmt8 != p_dsafmt) {
                        log.finer("backchained across a stack transition");
                        return 0;
                    }
                    /* If we have located an upstack DSA with a valid NAB value, assume that
                     * the input DSA address is good. */
                    if (dsafmt8 == CEECAASTACK_UP) {
                        long tptr = CeedsaTemplate.getCeedsanab(inputStream, callers_dsaptr);
                        if (tptr == dsaptr) {
                            log.finer("upstack DSA is good");
                            return 0;
                        }
                    }
                    dsaptr = callers_dsaptr;
                    /* We use the Tortoise and the Hare algorithm to detect loops. If the slow
                     * iterator is lapped it means there is a loop. */
                    if (slow) {
                        dsaf = new Ceexdsaf(space, slowdsaptr, slowdsafmt8, is64bit);
                        slowdsaptr = dsaf.DSA_Prev;
                        slowdsafmt8 = dsaf.DSA_Format;
                    }
                    if (dsaptr == slowdsaptr) {
                        log.finer("loop detected in DSA chain");
                        return ERROR;
                    }
                }
            } catch (IOException e) {
                /* Any bad read means the DSA was invalid */
                log.logp(Level.FINER,"com.ibm.j9ddr.corereaders.tdump.zebedee.le.Caa.Cel4rreg", "validateDSA","Bad read", e);
                return ERROR;
            } catch (Exception e) {
                log.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.tdump.zebedee.le.Caa.Cel4rreg", "validateDSA","Unexepected exception", e);
                throw new Error("Unexpected Exception:: " + e);
            }
        }
    }

    /**
     * Get the current (ie top) stack frame for this thread. 
     * @return the current stack frame or null if there is no stack
     */
    public DsaStackFrame getCurrentFrame() {
        if (currentFrame == null) {
            log.finer("about to get top dsa for caa " + hex(address));
            Cel4rreg cel = new Cel4rreg();
            assert cel.p_dsafmt != -1;
            RegisterSet regs = cel.regs;
            boolean isDownStack = cel.p_dsafmt == CEECAASTACK_DOWN;
            long dsa = cel.p_dsaptr;
            if (dsa == 0) {
                log.finer("dsa is zero for caa " + hex(address));
                return null;
            }
            try {
                currentFrame = new DsaStackFrame(dsa, isDownStack, regs, space, this);
            } catch (IOException e) {
                /* Return null if we can't find one */
                log.finer("exception getting top dsa: " + e);
            }
        }
        return currentFrame;
    }

    /**
     * Get the pthread id for this thread. This is the double word id returned to
     * pthread_create when the thread was created.
     */
    public long getPthreadId() throws IOException {
        return caaTemplate.getCeecaathdid(inputStream, address);
    }

    /**
     * Returns true if this thread failed (ie crashed, abended, has gone to meet its maker).
     */
    public boolean hasFailed() {
        throw new Error("tbc");
    }

    /**
     * Returns the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.RegisterSet} for a failed thread. This will return null if the
     * thread did not fail. In general, registers are only available (or at least only
     * useful) for a failed thread.
     */
    public RegisterSet getRegisterSet() {
        throw new Error("tbc");
    }

    /**
     * Returns the EDB (Enclave Data Block) for this Caa.
     */
    public Edb getEdb() {
        if (edb == null) {
            try {
                Long edbkey = new Long(ceecaaedb());
                edb = (Edb)space.getUserMap().get(edbkey);
                if (edb == null) {
                    /* We are the first Caa in this Edb */
                    edb = new Edb(ceecaaedb(), space, is64bit);
                    space.getUserMap().put(edbkey, edb);
                }
            } catch (Exception e) {
                log.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.tdump.zebedee.le.Caa", "getEdb","Unexepected exception", e);
                throw new Error("Unexpected Exception: " + e);
            }
        }
        return edb;
    }

    /**
     * Returns the address of this Caa.
     */
    public long address() {
        return address;
    }

    /**
     * Returns the AddressSpace we belong to
     */
    public AddressSpace space() {
        return space;
    }

    /**
     * Returns the Tcb we are associated with.
     */
    public Tcb getTcb() {
        return tcb;
    }

    /**
     * Returns the CEL level identifier.
     * @throws IOException if an error occurred reading from the address space
     */
    public int ceecaalevel() throws IOException {
        return (int)caaTemplate.getCeecaalevel(inputStream, address);
    }

    /** 
     * Returns the stack direction, either 0 (up) or 1 (down). Should only be called if the LE
     * level is 13 or greater.
     * @throws IOException if an error occurred reading from the address space
     */
    public int ceecaa_stackdirection() throws IOException {
        if (ceecaalevel() < 13)
            // sanity check
            throw new Error("ceecaa_stackdirection is not available in this level of LE! " + ceecaalevel());
        return (int)caaTemplate.getCeecaa_stackdirection(inputStream, address);
    }

    /** 
     * Returns the address of the storage manager control block.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaasmcb() throws IOException {
        // XXX only appears to be in 32-bit version?
    	if (caaTemplate instanceof Caa32Template) {
    		return ((Caa32Template)caaTemplate).getCeecaasmcb(inputStream, address);
    	} else {
    		return ((Caa64Template)caaTemplate).getCeecaasmcb(inputStream, address);
    	}
    }

    /** 
     * Returns the address of the region control block.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaarcb() throws IOException {
        return caaTemplate.getCeecaarcb(inputStream, address);
    }

    /** 
     * Returns the stackfloor.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaa_stackfloor() throws IOException {
        //return caaTemplate.getCeecaa_stackfloor(inputStream, address);
        throw new Error("tbc");
    }

    /** 
     * Returns the address of the thread value block anchor
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaavba() throws IOException {
        return caaTemplate.getCeecaavba(inputStream, address);
    }

    public long ceecaatcs() throws IOException {
        return caaTemplate.getCeecaatcs(inputStream, address);
    }

    /**
     * Searches thread value blocks for key matching the input key
     * and returns the thread specific data associated with the specified
     * key for the current thread.
     * If no thread specific data has been set for key, Zero is returned.
     */
    public long pthread_getspecific_d8_np(long key) throws IOException {
        long ceecaavba = ceecaavba();
        if (ceecaavba == 0) {
            log.finer("ceecaavba is zero in caa " + hex(address));
            return 0;
        }
        long eyecatcher = 0;
        try {
            eyecatcher = space.readInt(ceecaavba);
        } catch (IOException e) {
            log.fine("bad ceecaavba: " + hex(ceecaavba));
            return 0;
        }
        if (eyecatcher == 0xe3e5c240) {
            /* This is a TVB */
            // Described in ceexkdb.copy
            long ceeedbdba = getEdb().ceeedbdba();
            log.finer("ceeedbdba = " + hex(ceeedbdba));
            assert space.readInt(ceeedbdba) == 0xd2c4c240;  // KDB
            long ceekdb_bptr = space.readWord(ceeedbdba + 8, is64bit);
            long keyIndex = (key - ceekdb_bptr) / 16;
            int bucketNumber = (int)keyIndex / 32;
            int bucketIndex = (int)keyIndex % 32;
            log.fine("keyIndex = " + keyIndex + " bucketNumber = " + bucketNumber + " bucketIndex = " + bucketIndex);
            if (bucketNumber < 0 || bucketNumber > 31)
                return 0;
            assert bucketNumber < 32;
            int bucketLength = is64bit ? 8 : 4;
            long bucket = space.readWord(ceecaavba + bucketLength + (bucketNumber * bucketLength), is64bit);
            if (bucket == 0) {
                return 0;
            }
            log.finer("bucket = " + hex(bucket));
            long value = space.readWord(bucket + bucketLength + (bucketIndex * bucketLength), is64bit);
            log.finer("value = " + hex(value));
            return value;
        }
        long ceetvbcount = CeextvbTemplate.getCeetvbcount(inputStream, ceecaavba);
        log.finer("eye = " + hex(space.readInt(ceecaavba)));
        long ceetvbe = ceecaavba + (is64bit ? 16 : 12);
        for (int i = 0; i < ceetvbcount; i++) {
            long ceetvbekey = space.readWord(ceetvbe);
            log.finer("key = " + hex(key) + " ceetvbekey = " + hex(ceetvbekey));
            if (key == ceetvbekey) {
                return space.readWord(ceetvbe + (is64bit ? 8 : 4));
            }
            ceetvbe += (is64bit ? 16 : 8);
        }
        /* XXX Note: this is incomplete. Should also check other blocks */
        return 0;
    }

    /** 
     * Returns the address of the dummy dsa. This is used to indicate the bottom of 
     * the stack.
     */
    public long ceecaaddsa() {
        try {
            return caaTemplate.getCeecaaddsa(inputStream, address);
        } catch (IOException e) {
            throw new Error("oops");
        }
    }

    /** 
     * Returns the address of the enclave data block.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaaedb() throws IOException {
        return caaTemplate.getCeecaaedb(inputStream, address);
    }

    /** 
     * Returns the address of the hcom.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaaerrcm() throws IOException {
        return caaTemplate.getCeecaaerrcm(inputStream, address);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    public String toString() {
        return hex(address);
    }
}
