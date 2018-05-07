/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.ibm.dtfj.corereaders.zos.le;

import java.io.IOException;
import java.util.Vector;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpaceImageInputStream;
import com.ibm.dtfj.corereaders.zos.mvs.Ihartm2aTemplate;
import com.ibm.dtfj.corereaders.zos.mvs.IhastcbTemplate;
import com.ibm.dtfj.corereaders.zos.mvs.Lse;
import com.ibm.dtfj.corereaders.zos.mvs.RegisterSet;
import com.ibm.dtfj.corereaders.zos.mvs.Tcb;

/**
 * This class represents the LE view of an MVS {@link com.ibm.dtfj.corereaders.zos.mvs.Tcb}. Caa stands for
 * "Common Anchor Area" and this structure is the starting point for all the other LE
 * structures. For programs written in C, this is the most useful thread view because it
 * gives access to the C stack.
 *
 * The layout of the CAA control block is documented in the zOS Language Environment Debugging Guide GA22-7560-05 
 * 
 * @depend - - - com.ibm.dtfj.corereaders.zos.mvs.Tcb
 * @depend - - - com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace
 * @has - - - com.ibm.dtfj.corereaders.zos.le.DsaStackFrame
 * @has - - - com.ibm.dtfj.corereaders.zos.mvs.RegisterSet
 * @assoc - - - com.ibm.dtfj.corereaders.zos.le.Edb
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
    private /*static*/ CaaTemplate caaTemplate;
    private static int hcomLength = new Ceexhcom32Template().length();
    private String whereFound;
    static final int CEECAASTACK_UP = 0;
    static final int CEECAASTACK_DOWN = 1;
    private static final int WARNING = 4;
    private static final int ERROR = 8;
    private static final long F1SA = 0xe6f1e2c1;
    //private static final long F4SA = 0xe6f4e2c1;
    //private static final long F5SA = 0xe6f5e2c1;
    /** Logger */
    private static Logger log = Logger.getLogger(Caa.class.getName());

    //CMVC 156864 : : updated code to fix traversal of native stacks
    /** Address of LE Library Anchor Area (64-bit only) */
    private long laa;
    /** Is this a 64bit Caa? */
    private boolean is64bit;

    /**
     * Returns an array containing all of the Caas in the given
     * {@link com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace}
     */
    public static Caa[] getCaas(AddressSpace space) {
        Tcb[] tcbs = Tcb.getTcbs(space);
        if (tcbs == null) {
            log.fine("no tcbs found in address space " + space);
            return new Caa[0];
        }
        log.fine("found " + tcbs.length + " tcbs for address space " + space);
        Vector v = new Vector();
        for (int i = 0; i < tcbs.length; i++) {
            try {
                v.add(new Caa(tcbs[i]));
            } catch (CaaNotFound e) {
            }
        }
        return (Caa[])v.toArray(new Caa[0]);
    }

    /**
     * Create a new Caa from the given {@link com.ibm.dtfj.corereaders.zos.mvs.Tcb}. Note that not all
     * Tcbs have a Caa associated with them.
     * @throws CaaNotFound if there is no Caa associated with this Tcb.
     */
    // CMVC 160438 : changes made to the address space configuration to enable to correct determination of the CAA
    public Caa(Tcb tcb) throws CaaNotFound {
        log.finer("creating Caa for Tcb at address " + hex(tcb.address()));
        this.tcb = tcb;
        space = tcb.space();
        inputStream = space.getImageInputStream();
        space.setIs64bit(false);
        if (is32bitCaa()) {
            /* Now we know this is 32 bit, create the template */
            caaTemplate = new Caa32Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = Integer.parseInt(s);
                if (release >= 11) {
                    caaTemplate = new Caa32_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 32-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()));
            return;
        }
        space.setIs64bit(true);			//have to assume now that we are dealing with 64 bit address space
        if (is64bitCaa()) {
            /* Now we know this is 64 bit, create the template and set flag in address space */
            is64bit = true;
            caaTemplate = new Caa64Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = Integer.parseInt(s);
                if (release >= 11) {
                    caaTemplate = new Caa64_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 64-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()));
            return;
        }
        space.setIs64bit(false);	//now treat the address space as 32 bit again for final attempt to create the CAA
        if (is32bitCaaLastDitch()) {
            caaTemplate = new Caa32Template();
            String s = space.getDump().getProductRelease();
            if (s != null) {
                int release = Integer.parseInt(s);
                if (release >= 11) {
                    caaTemplate = new Caa32_11Template();
                    log.fine("switched to new caa format");
                }
            }
            log.fine("created 32-bit Caa " + hex(address) + " for Tcb at address " + hex(tcb.address()) + " (last ditch)");
            return;
        }
        throw new CaaNotFound();
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
            return false;
        }
    }

    /**
     * Returns a string indicating where the registers were found. This is mainly for
     * debugging purposes.
     */
    public String whereFound() {
        return whereFound;
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
            /*
             * Try and get the registers from the following locations:
             *
             * 1) RTM2 work area
             * 2) BPXGMSTA service
             * 3) linkage stack entries
             * 4) TCB
             *
             * if any succeeds we return otherwise move to the next location.
             */
            try {
                if ((regs = getRegistersFromRTM2()) != null) {
                    whereFound = "RTM2";
                    return;
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            /* If we still have not found a dsa, invoke kernel svs */
            try {
                if ((regs = getRegistersFromBPXGMSTA()) != null) {
                    whereFound = regs.whereFound();
                    if (whereFound == null)
                        whereFound = "BPXGMSTA";
                    return;
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            try {
                if ((regs = getRegistersFromLinkageStack()) != null) {
                    whereFound = "Linkage";
                    return;
                }
            } catch (IOException e) {
                e.printStackTrace();
                throw new Error("oops: " + e);
            }
            try {
                if ((regs = getRegistersFromTCB()) != null) {
                    whereFound = "TCB";
                    return;
                }
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
            //CMVC 156864 : : updated code to fix traversal of native stacks
            try {
                if (is64bit) {
                    /* This is from celqrreg.plx370: "Get the save R4 from a NOSTACK call" */
                    long lca = CeexlaaTemplate.getCeelaa_lca64(inputStream, laa);
                    p_dsaptr = CeelcaTemplate.getCeelca_savstack(inputStream, lca);
                    log.fine("p_dsaptr from lca = " + hex(p_dsaptr));
                    p_dsafmt = stackdirection = CEECAASTACK_DOWN;
                    if (validateDSA() == 0 ) {
                        whereFound = "LCA";
                        return;
                    }
                }
            } catch (IOException e) {
            	throw new Error("oops: " + e);
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
            if (level >= 13) { /* If LE 2.10 or higher */
                /* Obtain dsa format from the CAA */
                stackdirection = ceecaa_stackdirection();
                log.finer("stack direction is " + (stackdirection == CEECAASTACK_UP ? "up" : "down"));
            } else {
                stackdirection = CEECAASTACK_UP;
                log.finer("stack direction is up");
            }
            if (stackdirection == CEECAASTACK_DOWN) {
                try {
                    long tempptr = ceecaasmcb();
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
                    log.fine("found some rtm2 registers");
                    RegisterSet regs = new RegisterSet();
                    long rtm2grs = rtm2ptr + Ihartm2aTemplate.getRtm2ereg$offset();
                    for (int i = 0; i < 16; i++) {
                        regs.setRegister(i, space.readUnsignedInt(rtm2grs + i*4));
                    }
                    long rtm2psw = rtm2ptr + Ihartm2aTemplate.getRtm2apsw$offset();
                    regs.setPSW(space.readLong(rtm2psw));
                    if (registersValid(regs)) {
                        log.fine("found good dsa in rtm2");
                        return regs;
                    } else {
                        log.fine("bad dsa in rtm2");
                        return null;
                    }
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
            p_dsafmt = stackdirection;
            if (p_dsafmt == CEECAASTACK_DOWN) {
                p_dsaptr = regs.getRegisterAsAddress(4);
                log.fine("p_dsaptr from reg 4 = " + hex(p_dsaptr));
            } else {
                p_dsaptr = regs.getRegisterAsAddress(13);
                log.fine("p_dsaptr from reg 13 = " + hex(p_dsaptr));
            }
            int lastrc = validateDSA();
            if (lastrc == 0) {
                log.fine("found valid dsa");
                return true;
            } else {
                if (stackdirection == CEECAASTACK_DOWN) {
                    p_dsaptr = regs.getRegisterAsAddress(13);
                    log.fine("p_dsaptr from reg 13 (again) = " + hex(p_dsaptr));
                    p_dsafmt = CEECAASTACK_UP;
                    lastrc = validateDSA();
                    if (lastrc == WARNING) {
                        lastrc = validateDSA();
                        if (lastrc == 0) {
                            log.fine("found valid dsa");
                            return true;
                        }
                    }
                }
                /* reset values */
                log.fine("p_dsaptr invalid so reset: " + hex(p_dsaptr));
                p_dsaptr = 0;
            }
            return false;
        }

        /**
         * Try and get the registers from the BPXGMSTA service.
         */
        private RegisterSet getRegistersFromBPXGMSTA() throws IOException {
            RegisterSet regs = tcb.getRegistersFromBPXGMSTA();
            if (space.is64bit()) {
                // celqrreg appears to always assume down stack
                stackdirection = CEECAASTACK_DOWN;
				log.fine("forced stack direction down as 64-bit");
            }
            if (registersValid(regs)) {
                log.fine("found good dsa in BPXGMSTA");
                return regs;
            } else {
                return null;
            }
        }

        /**
         * Try and get the registers from the linkage stack.
         */
        private RegisterSet getRegistersFromLinkageStack() throws IOException {
            log.fine("enter getRegistersFromLinkageStack");
            try {
                Lse[] linkageStack = tcb.getLinkageStack();
                /* If Linkage stack is empty, leave */
                if (linkageStack.length == 0) {
                    log.fine("empty linkage stack");
                    return null;
                }
                for (int i = 0; i < linkageStack.length; i++) {
                    Lse lse = linkageStack[i];
                    if (lse.lses1pasn() == space.getAsid()) {
                        RegisterSet regs = new RegisterSet();
                        if (lse.isZArchitecture() && (lse.lses1typ7() == Lse.LSED1PC || lse.lses1typ7() == Lse.LSED1BAKR)) {
                            log.fine("found some z arch registers");
                            regs.setPSW(lse.lses1pswh());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lses1grs(j));
                            }
                        } else {
                            log.fine("found some non z arch registers");
                            regs.setPSW(lse.lsespsw());
                            for (int j = 0; j < 16; j++) {
                                regs.setRegister(j, lse.lsesgrs(j));
                            }
                        }
                        if (registersValid(regs)) {
                            log.fine("found good dsa in linkage stack");
                            return regs;
                        }
                    } else {
                        log.fine("different asid: " + hex(lse.lses1pasn()));
                    }
                }
            } catch (IOException e) {
                throw e;
            } catch (Exception e) {
                throw new Error("oops: " + e);
            }
            log.fine("could not find registers in linkage stack");
            return null;
        }

        /**
         * Try and get the registers from the TCB.
         */
        private RegisterSet getRegistersFromTCB() throws IOException {
            log.fine("getRegistersFromTCB");
            RegisterSet regs = tcb.getRegisters();
            if (registersValid(regs)) {
                log.fine("found good dsa in TCB");
                return regs;
            } else {
                return null;
            }
        }

        /**
         * Validate the given DSA. Returns 0 if valid. Note because this is Java, we can't
         * modify the input parameters, so we use the instance variables instead and
         * val_dsa == p_dsaptr, val_dsafmt == p_dsafmt.
         */
        private int validateDSA() {
            log.fine("attempt to validate " + hex(p_dsaptr) + " on " + (p_dsafmt == CEECAASTACK_DOWN ? "down" : "up") + " stack");
            try {
                if (p_dsafmt == CEECAASTACK_DOWN) {
                    /* the check for being in the current segment is commented out */
                } else {
                    if (space.is64bit())
                        return ERROR;
                    long tptr = ceecaaerrcm();
                    /* Chicken egg situation */
                    //assert !space.is64bit();
                    /* If the input DSA address is within the HCOM and double word aligned, 
                     * assume that it is good. */
                    if (p_dsaptr < (tptr + hcomLength) && p_dsaptr >= tptr && (p_dsaptr & 7) == 0) {
                        log.fine("upstack dsa " + hex(p_dsaptr) + " is inside hcom");
                        return 0;
                    }
                }
                long ddsa = ceecaaddsa();
                long dsaptr = p_dsaptr;
                int dsafmt8 = p_dsafmt;
                long slowdsaptr = p_dsaptr;
                int slowdsafmt8 = p_dsafmt;
                for (boolean slow = false;; slow = !slow) {
                    DsaStackFrame.Ceexdsaf dsaf = new DsaStackFrame.Ceexdsaf(space, dsaptr, dsafmt8);
                    /* If the stack direction is down but we are validating an upstack DSA
                     * and the current DSA is inside the current segment of the down stack,
                     * assume this must be a OS_NOSTACK call, return WARNING and replace
                     * input DSA and DSAFmt with R4 value from this DSA */
                    if (stackdirection == CEECAASTACK_DOWN && p_dsafmt == CEECAASTACK_UP &&
                            dsaptr < seghigh && dsaptr >= seglow) {
                        p_dsaptr = CeedsaTemplate.getCeedsar4(inputStream, dsaptr);
                        p_dsafmt = CEECAASTACK_DOWN;
                        log.fine("warning, try switching to down stack");
                        return WARNING;
                    }
                    long callers_dsaptr = dsaf.DSA_Prev;
                    dsafmt8 = dsaf.DSA_Format;
                    /* If we are not able to backchain any farther or we have encountered
                     * a linkage stack, assume that the input DSA address is bad. */
                    if (callers_dsaptr == 0 || callers_dsaptr == F1SA) {
                        log.fine("cannot backchain futher because " + (callers_dsaptr == 0 ? "zero" : "linkage stack") + " found");
                        return ERROR;
                    }
                    /* If we were able to backchain to the dummy DSA, the input DSA address
                     * must be good. */
                    if (callers_dsaptr == ddsa) {
                        log.fine("dummy dsa reached");
                        return 0;
                    }
                    /* If we backchained across a stack transition, assume that the input
                     * DSA address is good. */
                    if (dsafmt8 != p_dsafmt) {
                        log.fine("backchained across a stack transition");
                        return 0;
                    }
                    /* If we have located an upstack DSA with a valid NAB value, assume that
                     * the input DSA address is good. */
                    if (dsafmt8 == CEECAASTACK_UP) {
                        long tptr = CeedsaTemplate.getCeedsanab(inputStream, callers_dsaptr);
                        if (tptr == dsaptr) {
                            log.fine("upstack DSA is good");
                            return 0;
                        }
                    }
                    dsaptr = callers_dsaptr;
                    /* We use the Tortoise and the Hare algorithm to detect loops. If the slow
                     * iterator is lapped it means there is a loop. */
                    if (slow) {
                        dsaf = new DsaStackFrame.Ceexdsaf(space, slowdsaptr, slowdsafmt8);
                        slowdsaptr = dsaf.DSA_Prev;
                        slowdsafmt8 = dsaf.DSA_Format;
                    }
                    if (dsaptr == slowdsaptr) {
                        log.fine("loop detected in DSA chain");
                        return ERROR;
                    }
                }
            } catch (IOException e) {
                /* Any bad read means the DSA was invalid */
                log.fine("bad read: " + e);
                return ERROR;
            } catch (Exception e) {
                e.printStackTrace();
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * Get the current (ie top) stack frame for this thread. 
     * @return the current stack frame or null if there is no stack
     */
    public DsaStackFrame getCurrentFrame() {
        if (currentFrame == null) {
            log.fine("about to get top dsa for caa " + hex(address));
            Cel4rreg cel = new Cel4rreg();
            assert cel.p_dsafmt != -1;
            RegisterSet regs = cel.regs;
            boolean isDownStack = cel.p_dsafmt == CEECAASTACK_DOWN;
            long dsa = cel.p_dsaptr;
            if (dsa == 0) {
                log.fine("dsa is zero for caa " + hex(address));
                return null;
            }
            try {
                currentFrame = new DsaStackFrame(dsa, isDownStack, regs, space, this);
            } catch (IOException e) {
                /* Return null if we can't find one */
                log.fine("exception getting top dsa: " + e);
            }
        }
        return currentFrame;
    }

    /**
     * Get the pthread id for this thread. This is the top half of the CEECAATHDID field in the CAA
      */
    public int getPThreadID() throws IOException {
        return caaTemplate.getCeecaathdid(inputStream, address);
    }

    /**
     * Returns true if this thread failed (ie crashed, abended, has gone to meet its maker).
     */
    public boolean hasFailed() {
        throw new Error("tbc");
    }

    /**
     * Returns the {@link com.ibm.dtfj.corereaders.zos.mvs.RegisterSet} for a failed thread. This will return null if the
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
        // XXX there should be just one copy of the EDB
        if (edb == null) {
            try {
                edb = new Edb(ceecaaedb(), space);
            } catch (Exception e) {
                throw new Error("oops: " + e);
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
            throw new Error("ceecaa_stackdirection is not available in this level of LE! "+ceecaalevel());
        return (int)caaTemplate.getCeecaa_stackdirection(inputStream, address);
    }

    /** 
     * Returns the address of the storage manager control block.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaasmcb() throws IOException {
        // XXX only appears to be in 32-bit version?
        return ((Caa32Template)caaTemplate).getCeecaasmcb(inputStream, address);
    }

    /** 
     * Returns the address of the region control block.
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaarcb() throws IOException {
        return caaTemplate.getCeecaarcb(inputStream, address);
    }

    /** 
     * Returns the address of the thread value block anchor
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceecaavba() throws IOException {
        return caaTemplate.getCeecaavba(inputStream, address);
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
            log.fine("ceecaavba is zero!");
            return 0;
        }
		//if (space.is64bit()) return 0;// Safest!
        long eyecatcher = space.readInt(ceecaavba);
        if (eyecatcher == 0xe3e5c240) {
            /* This is a TVB */
            // XXX Use templates and fixup for 64-bit
            long ceeedbdba = getEdb().ceeedbdba();
            log.fine("ceeedbdba = " + hex(ceeedbdba));
            assert space.readInt(ceeedbdba) == 0xd2c4c240;  // KDB
            long ceekdb_bptr = space.readInt(ceeedbdba + 8);
            long keyIndex = (key - ceekdb_bptr) / 16;
            int bucketNumber = (int)keyIndex / 32;
            int bucketIndex = (int)keyIndex % 32;
            assert bucketNumber < 32;
            long bucket = space.readInt(ceecaavba + 4 + (bucketNumber * 4));
            if (bucket == 0) {
                return 0;
            }
            log.fine("bucket = " + hex(bucket));
            long value = space.readInt(bucket + 4 + (bucketIndex * 4));
            log.fine("value = " + hex(value));
            return value;
        }
        long ceetvbcount = CeextvbTemplate.getCeetvbcount(inputStream, ceecaavba);
        log.fine("eye = " + hex(space.readInt(ceecaavba)));
        long ceetvbe = ceecaavba + CeextvbTemplate.length();
        for (int i = 0; i < ceetvbcount; i++) {
            long ceetvbekey = CeextvbeTemplate.getCeetvbekey(inputStream, ceetvbe);
            if (key == ceetvbekey) {
                return CeextvbeTemplate.getCeetvbevalue(inputStream, ceetvbe);
            }
            ceetvbe += CeextvbeTemplate.length();
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
