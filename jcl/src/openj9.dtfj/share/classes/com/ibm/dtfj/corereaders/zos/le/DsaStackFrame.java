/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpaceImageInputStream;
import com.ibm.dtfj.corereaders.zos.mvs.RegisterSet;

/**
 * This class represents a single LE stack frame (aka DSA). Use {@link #getParentFrame} to
 * get the stack frame that called this one. 
 * XXX see ceekdmdr
 *
 * @has - - - com.ibm.dtfj.corereaders.zos.le.Function
 */
public class DsaStackFrame {

    /** The address of this dsa */
    private long address;
    /** Is this a down stack frame? */
    private boolean isDownStack;
    /** Is the parent frame a down stack frame? (This is set before creating the parent frame) */
    private boolean isParentDownStack;
    /** Is the parent frame a transition frame? */
    private boolean isParentTransitionFrame;
    /** The address of the parent dsa */
    private long parentAddress;
    /** The address at which the caller called us */
    private long parentCallingAddress;
    /** The child dsa (if any) */
    private DsaStackFrame childDsa;
    /** The registers. This will only be non-null for the frame at the top of the stack. */
    private RegisterSet registers;
    /** The address of the function's entry point */
    private long entryPoint;
    /** The offset from the entry point where we called the next function */
    private long entryOffset;
    /** The name of the function */
    private String entryName;
    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpaceImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** The Caa we are associated with */
    private Caa caa;
    /** Dsa template - size depends on 32/64 bit mode */
    /*static*/ CeedsahpTemplate ceedsahpTemplate;
    /** Hcom template - size depends on 32/64 bit mode */
    /*static*/ CeexhcomTemplate ceexhcomTemplate;
    /** RCB template - size depends on 32/64 bit mode */
    /*static*/ CeexrcbTemplate ceexrcbTemplate;
    /** Constant for Detecting a Transition on the Up Stack */
    static final long Ceedsahp_UpTran_Ind = 0xffffffffL;
    /** Transition is stack swap */
    private static final long Ceedsahp_UpToDown = 2;
    /** Transition is stack swap */
    private static final long Ceedsahp_DownToUp = 3;
    /** BASR call */
    private static final long Hpcl_Basr_Call = 0;
    /** BRAS call */
    private static final long Hpcl_Bras_Call = 1;
    /** BRASL call */
    private static final long Hpcl_Brasl_Call = 3;
    /** Up DSA Format */
    private static final int US_FORMAT = 0;
    /** Down DSA Format */
    private static final int DS_FORMAT = 1;
    /** Unknown DSA Format */
    private static final int UK_FORMAT = -1;
    /** "CEE" */
    private static final int CEEEYECAT = 0xC3C5C5;
    /** branch and set mode inst in CEL glue code */
    private static final short BASSM1415 = 0x0CEF;
    /** load r14 inst in CEL glue code */
    private static final int L14DSAMODE = 0x58E0D06C;
    /** Logger */
    private static Logger log = Logger.getLogger(DsaStackFrame.class.getName());

    /**
     * Create a new DsaStackFrame from the given dsa address and with the given stack direction.
     */
    public DsaStackFrame(long address, boolean isDownStack, RegisterSet registers, AddressSpace space, Caa caa) throws IOException {
        this.address = address;
        this.isDownStack = isDownStack;
        this.registers = registers;
        this.space = space;
        inputStream = space.getImageInputStream();
        this.caa = caa;
        /* Create any templates we need if not already created */
        createTemplates(space);
        /* Call the ceextbck method to calculate entry point, previous dsa etc */
        ceextbck(address, isDownStack, caa);
    }

    /**
     * Create any templates we need if not already created.
     * XXX Presumably we will never have mixed 32-bit and 64-bit address spaces?
     */
    /*static*/ void createTemplates(AddressSpace space) {
        if (ceedsahpTemplate == null) {
            if (space.is64bit()) {
                ceedsahpTemplate = new Ceedsahp64Template();
                ceexhcomTemplate = new Ceexhcom64Template();
                ceexrcbTemplate = new Ceexrcb64Template();
            } else {
                ceedsahpTemplate = new Ceedsahp32Template();
                ceexhcomTemplate = new Ceexhcom32Template();
                ceexrcbTemplate = new Ceexrcb32Template();
            }
        }
    }

    /**
     * Returns the AddressSpace we belong to.
     */
    public AddressSpace getAddressSpace() {
        return space;
    }

    /**
     * Returns the address of the DSA for this stack frame.
     */
    public long getDsaAddress() {
        return address;
    }

    /**
     * Returns the current register set (or null if there are no saved registers).
     */
    public RegisterSet getRegisterSet() {
        return registers;
    }

    /**
     * Returns a guess as to the given argument number. Note that there is no guarantee that
     * the returned value is legitimate. The argument number might be greater than the number
     * of arguments that the function takes for instance. In which case whatever happens to be
     * on the stack will be returned regardless. The function might not obey the convention
     * of storing its arguments on the stack*/
    /*
    public int getArg(int argNumber) throws Exception {
        if (downstack) {
            return space.readInt(previous().address + 0x840 + (argNumber << 2));
        } else {
            int r1 = previous().registers[1];
            return space.readInt(r1 + (argNumber << 2));
        }
    }
    */

    /**
     * Returns the offset from the function entry point. This is the offset within the
     * function's executable code where the call was made to the successor stack frame.
     */
    public long getEntryOffset() {
        if (registers != null) {
            /* We have a failing register set so use the PSW from there */
            // XXX 64-bit dependency
            return space.stripTopBit(registers.getPSW() & 0x7fffffffL) - getEntryPoint();
            //return (registers.getPSW() & 0xffffffffL) - getEntryPoint();
        }
        if (childDsa != null)
            return childDsa.parentCallingAddress - entryPoint;
        return entryOffset;
    }

    /**
     * Returns the entry point address for the function belonging to this dsa.
     */
    public long getEntryPoint() {
        return entryPoint;
    }

    /** Returns the name of the function */
    public String getEntryName() {
        return entryName;
    }

    /**
     * Returns the caller of this stack frame.
     * @return the stack frame for the function that called this one or null if we have
     * reached the bottom of the stack
     */
    public DsaStackFrame getParentFrame() {
        if (parentAddress == 0 || parentAddress == caa.ceecaaddsa())
            /* bottom of stack reached */
            return null;
        try {
            DsaStackFrame parent = new DsaStackFrame(parentAddress, isParentDownStack, null, space, caa);
            parent.childDsa = this;
            return parent;
        } catch (IOException e) {
            return null;
        }
    }

    /**
     * Returns the function call that this stack frame represents.
     */
    public Function getFunction() {
        // XXX need to share functions between different DSAs
        return new Function(this);
    }

    /**
     * This method emulates the Ceektbck traceback macro. Most of the comments are from there 
     * including the following description:
     *       This module performs the processing of the CEETRCB low level  
     *       service.  This service determines the entry address, entry    
     *       point name, compile unit address, compile unit name, calling  
     *       instruction address, calling statement number, CIB address,   
     *       and callers DSA address, given the DSA address for a routine. 
     */
    void ceextbck(long dsaptr, boolean isDownStack, Caa caa) throws IOException {
        log.fine("ceextbck, dsaptr = " + hex(dsaptr) + " downstack = " + isDownStack);
        /* stack direction of DSA */
        int dsa_format = isDownStack ? DS_FORMAT : US_FORMAT;
        /* entry point */
        long entry_address = space.WILD_POINTER;

        int dsaformat8 = dsa_format;    // Not sure why they use a byte version sometimes
        if (dsa_format == UK_FORMAT) {
            throw new Error("tbc");
        }
        int callers_dsa_format = dsaformat8;
        boolean transition = verifyFormat(dsaptr, dsa_format);
        if (dsaptr < 0) {
            throw new Error("tbc");
        }
        Ceexdsaf dsaf = new Ceexdsaf(space, dsaptr, dsa_format);
        callers_dsa_format = dsaf.DSA_Format;
        long callers_dsaptr = dsaf.DSA_Prev;
        /* Set instance variables containing info about parent DSA */
        parentAddress = callers_dsaptr;
        isParentDownStack = callers_dsa_format == DS_FORMAT;
        /* Determine the CIB corresponding to this DSA, if any, and the caller's DSA */
        long cibptr = findCib(dsaptr);
        long callers_cibptr = findCib(callers_dsaptr);
        /* Determine the SFXM (Stack Frame Exit control block) corresponding to the DSA,
         * if any, and the Caller's DSA */
        long sfxmptr = findSfxm(dsaptr, dsaformat8);
        long callers_sfxmptr = findSfxm(callers_dsaptr, callers_dsa_format);

        if (dsaformat8 == DS_FORMAT) {
            processDsfmt(dsaptr, dsaformat8, callers_dsaptr, callers_dsa_format, transition, cibptr, callers_cibptr, sfxmptr);
        } else {
            if (!transition) {
                int hdsp_signature = space.readInt(dsaptr + 72);    // dsaptr->hdspdsasig
                if ((hdsp_signature & 0xfffffff0) == 0x0808cee0) {  // dispatcher eyecatch
                    /* CEEHDSP with XPLINK support uses CODENUM() for subroutines. 
                     * This means R11 may not be the entry for the call to this routine.
                     * Get entry from a local VCON. */
                    //entry_address = addr(CEEHDSP);
                    // XXX how do we get the address of CEEHDSP?
                    throw new Error("tbc");
                } else {
                    entry_address = CeedsaTemplate.getCeedsar15(inputStream, callers_dsaptr);
                    log.fine("upstack entry address = " + hex(entry_address));
                }
            } else {
                /* It is an UP-Transition */
                long ceedsatran = CeedsaTemplate.getCeedsatran(inputStream, dsaptr);
                entry_address = Ceedsahp_transitionTemplate.getCeedsahp_tran_ep(inputStream, ceedsatran);
                log.fine("upstack transition entry address = " + hex(entry_address));
            }
            /* Test if this entry address is really valid by trying to access it */
            try {
                space.readLong(entry_address);
            } catch (IOException e) {
                entry_address = 0;
            }
            boolean cel_enabled;
            long signaturePtr = entry_address;
            /* Determine if the routine is CEL Enabled by looking at 
             * the signature byte in the PPA1 */
            if (entry_address == 0) {
                cel_enabled = false;
            } else {
                /* Have an Entry Point Address. Obtain the entry point vector.
                 * If the name offset ='00X or '01'X and the  the eyecatcher is
                 * 'CEE' then the entry point vector is valid */
                int signature = 0;  // default is invalid
                try {
                    signature = (int)CeexoepvTemplate.getOepv_eyecatch(inputStream, signaturePtr);
                } catch (IOException e) {}
                log.fine("signature = " + hex(signature));
                int msb = signature >>> 24;
                if (((msb == 0) || (msb == 1)) && ((signature & 0xffffff) == CEEEYECAT)) {
                    /* get the ppa1 signature */
                    try {
                        signature = getPpa1Sig(signaturePtr);
                    } catch (IOException e) {
                        signature = 0;
                    }
                    if ((signature & 0xff) == 0xce) {   // PPA1EYE
                        cel_enabled = true;
                    } else {
                        cel_enabled = false;
                    }
                } else {
                    /* 
                     * Check whether this module is a wrapped
                     * transfer vector.                      
                     *   If the module entry-point + 0 = X'47F0Fxxx'
                     *      (a 'BR xxx(15)' instruction)           
                     *     Calculate the address of the signature:
                     *         Module entry-point                
                     *         Plus 'xxx' from the BR instruction
                     *         Minus X'14' (signature size) 
                     *     If signature address + 0 = 0    
                     *         (i.e., no BR instruction)  &
                     *        signature address + 5 = X'C3C5C5'
                     *       Get PPA1 address        
                     *       If PPA1 eyecatcher = X'CE'
                     *         Valid wrapped transfer vector
                     */
                    cel_enabled = false;
                    try {
                        signature = (int)CeexoepvTemplate.getOepv_oldep(inputStream, signaturePtr);
                        log.fine("oldep signature = " + hex(signature));
                        int bcrinstr = signature >>> 12;
                        if (bcrinstr == 0x47f0f) {
                            int bcrdispl = signature & 0xfff;
                            int oepvLength = CeexoepvTemplate.length();
                            assert oepvLength == 0x14 : oepvLength;
                            signaturePtr = signaturePtr + bcrdispl - oepvLength;
                            /* adjusted address.  now re-read */
                            signature = (int)CeexoepvTemplate.getOepv_oldep(inputStream, signaturePtr);
                            if (signature == 0) {
                                signature = (int)CeexoepvTemplate.getOepv_eyecatch(inputStream, signaturePtr);
                                if ((signature & 0xffffff) == CEEEYECAT) {
                                    signature = getPpa1Sig(signaturePtr);
                                    if ((signature & 0xff) == 0xce) {   // PPA1EYE
                                        cel_enabled = true;
                                        log.fine("found a good ppa1 eyecatcher");
                                    } else {
                                        throw new Error("tbc");
                                    }
                                } else {
                                    throw new Error("tbc");
                                }
                            } else {
                                signaturePtr = entry_address;
                            }
                        } else {
                            signaturePtr = entry_address;
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                        throw new Error("tbc");
                    }
                }
            }
            boolean v2ppa = false;
            if (cel_enabled) {
                int oepv_cnameoffs = (int)CeexoepvTemplate.getOepv_cnameoffs(inputStream, signaturePtr);
                if (oepv_cnameoffs == 1)
                    v2ppa = true;
            }
            /* determine callers calling address */
            long callers_instruction_address = getCallingAddr(callers_dsaptr, callers_dsa_format, dsaptr, dsaformat8, transition, callers_cibptr, callers_sfxmptr);
            parentCallingAddress = callers_instruction_address;
            /*
             * If the routine is CEL enabled, determine the calling  
             * address if none was supplied, the callers instruction 
             * address, entry point name, member id, compile unit    
             * unit address, and compile unit name, from standard    
             * fields in the callers DSA and this routines PPAs.     
             */
            if (cel_enabled) {
                /* If p_call_instruction_address is zero, try to determine
                 * the calling address. (I assume that p_call_instruction_address == 0
                 * means the entry at the top of the stack which will be the only one
                 * with registers) */
                if (registers != null) {
                    long call_instruction_address = getCallingAddr(dsaptr, dsaformat8, 0, 0, false, cibptr, sfxmptr);
                    log.fine("found call_instruction_address at top of stack: 0x" + hex(call_instruction_address));
                }
                int offset_to_name = getPpa1Nmo(signaturePtr);
                if (offset_to_name == 0) {
                    throw new Error("tbc");
                } else {
                    /* offset or offset*2 to entry name obtained */
                    long ppa1_epn_address;
                    // XXX should have ppa1 squirreled away somewhere
                    int ppa1offset = (int)CeexoepvTemplate.getOepv_ppa1offset(inputStream, signaturePtr);
                    long ppa1 = signaturePtr + ppa1offset;
                    if (v2ppa) {
                        ppa1_epn_address = ppa1 + offset_to_name*2;
                    } else {
                        ppa1_epn_address = ppa1 + offset_to_name;
                    }
                    entryName = space.readEbcdicString(ppa1_epn_address);
                    log.fine("read entry name: " + entryName);
                }
            } else {
                entryName = "(unknown)";
            }
            entryPoint = space.stripTopBit(entry_address);
        }
    }

    /** Extract information for a DSA on the down stack */
    void processDsfmt(long in_dsa, int in_dsafmt, long in_callerdsa, int in_caller_dsafmt, boolean in_dsatrans, long in_cibptr, long in_caller_cibptr, long in_sfxmptr) throws IOException {
        /* Obtain Entry Point. PPA1, and PPA2 addresses */
        Ceexepaf epaf = new Ceexepaf(in_dsa, in_dsafmt);
        entryPoint = space.stripTopBit(epaf.entry_address);
        /* ceexepaf performs verification for down stack. if ppa1 address is not set,
         * this is not cel enabled code */
        if (epaf.ppa1_addr != 0) {
            /* Find calling address if none supplied, and the callers calling address */
            if (registers != null) {
                long call_instruction_address = getCallingAddr(in_dsa, in_dsafmt, 0, 0, false, in_cibptr, 0);
                log.fine("found call_instruction_address at top of stack: 0x" + hex(call_instruction_address));
            }
            long callers_instruction_address = getCallingAddr(in_callerdsa, in_caller_dsafmt, in_dsa, in_dsafmt, in_dsatrans, in_caller_cibptr, in_sfxmptr);
            parentCallingAddress = callers_instruction_address;
            assert parentCallingAddress != 0;
            Ceexppaf ppaf = new Ceexppaf(epaf.ppa1_addr, "NAM");
            entryName = ppaf.opt_ptr == 0 ? "(unknown)" : space.readEbcdicString(ppaf.opt_ptr);
            log.fine("read entry name: " + entryName);
        } else {
            throw new Error("tbc");
        }
    }

    /** Reads the ppa1_sig field given an entry point */
    private int getPpa1Sig(long signaturePtr) throws IOException {
        int ppa1offset = (int)CeexoepvTemplate.getOepv_ppa1offset(inputStream, signaturePtr);
        long ppa1 = signaturePtr + ppa1offset;
        int signature = (int)Ceexpp1bTemplate.getPpa1_sig(inputStream, ppa1);
        log.fine("read ppa1 signature " + hex(signature) + " from entry point " + hex(signaturePtr));
        return signature;
    }

    /** Reads the ppa1_nmo field given an entry point */
    private int getPpa1Nmo(long signaturePtr) throws IOException {
        int ppa1offset = (int)CeexoepvTemplate.getOepv_ppa1offset(inputStream, signaturePtr);
        long ppa1 = signaturePtr + ppa1offset;
        int nmo = (int)Ceexpp1bTemplate.getPpa1_nmo(inputStream, ppa1);
        nmo &= 0xff;    // XXX the template stuff should be able to sort this out?
        log.fine("read ppa1 nmo " + hex(nmo) + " from entry point " + hex(signaturePtr));
        return nmo;
    }

    /** Determines if this is a transition DSA */
    boolean verifyFormat(long dsaptr, int dsa_format) throws IOException {
        if (dsa_format == US_FORMAT) {
            long ceedsabkc = CeedsaTemplate.getCeedsabkc(inputStream, dsaptr);
            log.fine("read ceedsabkc " + hex(ceedsabkc));
            if (ceedsabkc == Ceedsahp_UpTran_Ind) {
                long ceedsatran = CeedsaTemplate.getCeedsatran(inputStream, dsaptr);
                log.fine("read ceedsatran " + hex(ceedsatran));
                long ceedsahp_trtype = Ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsatran);
                if (ceedsahp_trtype > 0 && ceedsahp_trtype <= 6) {
                    return true;
                }
            }
        } else if (dsa_format == DS_FORMAT) {
            long ceedsahpr7 = ceedsahpTemplate.getCeedsahpr7(inputStream, dsaptr);
            if (ceedsahpr7 == 0) {
                long ceedsahptran = ceedsahpTemplate.getCeedsahptran(inputStream, dsaptr);
                long ceedsahp_trtype = Ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsahptran);
                if (ceedsahp_trtype > 0 && ceedsahp_trtype <= 6) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Determine the point at which a program lost control
     */
    long getCallingAddr(long in_dsa, int in_dsafmt, long fo_dsa, int fo_dsafmt, boolean fo_dsatrans, long in_cib, long in_sfxm) throws IOException {
        long callingaddr = space.WILD_POINTER;
        long next_instruction_address = 0;
        if (in_cib != 0) {
            callingaddr = CeexcibTemplate.getCib_int(inputStream, in_cib);
            log.fine("got calling address " + hex(callingaddr) + " from cib");
        } else {
            /* do not have cib, check sfxm */
            if (in_sfxm != 0) {
                throw new Error("tbc");
            } else {
                /* do not have sfxm */
                if (in_dsafmt == US_FORMAT) {
                    next_instruction_address = CeedsaTemplate.getCeedsar14(inputStream, in_dsa);
                } else {
                    /* Downstack DSA.  The return address is in the callers DSA. If the
                     * callee dsa was supplied, obtain the return address from reg7.
                     * If no callee DSA, see if the CAA is also CEEKTBCK's CAA. If they 
                     * are the same, this means that the DSA is on CEEKTBCK's BackChain.
                     * Will scan the DSA chain to find the DSA immediately before the
                     * input DSA. */
                    if (fo_dsa == 0) {
                        /* No callee dsa supplied. If the input CAA is the same as CEEKTBCK's
                         * CAA then we can run the chain to locate the callee's DSA. */

                        /* Can't do anything here! */
                    } else {
                        /* If there is a former DSA, it will either be an Upstack Transition,
                         * DownStack Transition, or a DownStack. The next_instruction address
                         * will be obtained based on the tupe of DSA. */
                        if (fo_dsafmt == US_FORMAT && fo_dsatrans) {
                            /* It is an UP-Transition */
                            long ceedsatran = CeedsaTemplate.getCeedsatran(inputStream, fo_dsa);
                            next_instruction_address = Ceedsahp_transitionTemplate.getCeedsahp_retaddr(inputStream, ceedsatran);
                        } else {
                            if (fo_dsafmt == DS_FORMAT && fo_dsatrans) {
                                /* It is a DOWN-Transition */
                                long ceedsahptran = ceedsahpTemplate.getCeedsahptran(inputStream, fo_dsa);
                                next_instruction_address = Ceedsahp_transitionTemplate.getCeedsahp_retaddr(inputStream, ceedsahptran);
                            } else {
                                next_instruction_address = ceedsahpTemplate.getCeedsahpr7(inputStream, fo_dsa);
                            }
                        }
                    }
                }
            }
            /* Adjust next instruction address if a valid one was obtained.
             * Adjustments will be different based on the stack direction */
            if (next_instruction_address != 0) {
                if (!space.is64bit()) {
                    if (in_dsafmt == US_FORMAT && (next_instruction_address & 0x80000000) == 0) {
                        next_instruction_address &= 0xffffff;
                    } else {
                        next_instruction_address &= 0x7fffffff;
                    }
                }
                /* Check if the instruction address is within ceeosigr or ceeosigx.
                 * If so, the true calling address has been stored in the HCOM. */
                if (in_dsafmt == US_FORMAT) {
                    /* upstack nsi adjust */
                    if (next_instruction_address == caa.getEdb().ceeedb_ceeosigr()) {
                        throw new Error("tbc");
                    }
                    /* Assume calling instruction is a BALR type instruction which is
                     * 2 bytes long. Thus the calling instruction is 2 bytes ahead of
                     * the next instruction to be executed in the routine. 
                     *
                     * Test for amode switching glue code at the next instruction address.
                     * If so, replace next instruction address with real return address
                     * saved in DSAMODE field of callers DSA. */
                    try {
                        short rr_inst = space.readShort(next_instruction_address - 2);
                        if (rr_inst == BASSM1415) {
                            /* call done with BASSM 14,15 */
                            int rx_inst = space.readInt(next_instruction_address);
                            if (rx_inst == L14DSAMODE) {
                                next_instruction_address = CeedsaTemplate.getCeedsamode(inputStream, in_dsa);
                                /* Assume calling instruction is a BALR type instruction
                                 * which is 2 bytes long. Thus the calling instruction is
                                 * 2 bytes ahead of the next instruction to be executed in
                                 * the routine. Also, if the high order bit is zero, then
                                 * address is amode 24, and the high order byte must be cleared. */
                                if (!space.is64bit()) {
                                    if ((next_instruction_address & 0x80000000) == 0) {
                                        next_instruction_address &= 0xffffff;
                                    } else {
                                        next_instruction_address &= 0x7fffffff;
                                    }
                                }
                            }
                        }
                        /* If next_instruction is  NOP N '4700bddd'X, the routine uses
                         * OPLINK linkage convention */
                        boolean fstlink_call = false;
                        int rx_inst = space.readInt(next_instruction_address);
                        if ((rx_inst >>> 16) == 0x4700) {
                            fstlink_call = true;
                        }
                        callingaddr =  next_instruction_address - 2;
                        log.fine("found callingaddr = 0x" + hex(callingaddr));
                    } catch (IOException e) {
                        log.fine("could not read next_instruction_address at " + hex(next_instruction_address));
                        callingaddr = 0;
                        throw e;
                    }
                } else {
                    /* downstack nsi adjust */
                    long ceecaarcb = caa.ceecaarcb();
                    long ceercb_ceeosigx = ceexrcbTemplate.getCeercb_ceeosigx(inputStream, ceecaarcb);
                    if (next_instruction_address == ceercb_ceeosigx) {
                        /* Get NSI from saved register7 in edb */
                        throw new Error("tbc");
                    }
                    /* read in noop.  call instruction will be either:
                     *   - BASR(0)   - 2 byte length
                     *   - BRAS(1)   - 4 byte length
                     *   - BRASL(2)  - 6 byte length
                     */
                    int rx_inst = space.readInt(next_instruction_address);
                    int Hpcl_Call_Type = (rx_inst >> 16) & 0xf;
                    if ((rx_inst >>> 20) != (space.is64bit() ? 0x070 : 0x470)) {
                        callingaddr = 0;
                        log.fine("did not find expected nop");
                    } else if (Hpcl_Call_Type == Hpcl_Basr_Call) {
                        callingaddr = next_instruction_address - 2;
                    } else if (Hpcl_Call_Type == Hpcl_Bras_Call) {
                        callingaddr = next_instruction_address - 4;
                    } else if (Hpcl_Call_Type == Hpcl_Brasl_Call) {
                        callingaddr = next_instruction_address - 6;
                    } else {
                        callingaddr = 0;
                        log.fine("did not recognize call type " + Hpcl_Call_Type);
                    }
                }
            } else {
                log.fine("next_instruction_address zero, cannot get calling address");
            }
        }
        return callingaddr;
    }

    /**
     * Scan CIB chain and return CIB address corresponding to the input dsa.
     */
    long findCib(long in_dsa) {
        try {
            long cibh_ptr_cib = 0;
            long ceecaaerrcm = caa.ceecaaerrcm();
            long cibhptr = ceexhcomTemplate.getHcom_cibh(inputStream, ceecaaerrcm);
            while (cibhptr != 0) {
                boolean cibh_in_use = CeexcibhTemplate.getCibh_in_use(inputStream, cibhptr) != 0;
                cibh_ptr_cib = CeexcibhTemplate.getCibh_ptr_cib(inputStream, cibhptr);
                long cib_sv1 = CeexcibTemplate.getCib_sv1(inputStream, cibh_ptr_cib);
                if (cibh_in_use && cib_sv1 == in_dsa)
                    break;
                cibhptr = CeexcibhTemplate.getCibh_back(inputStream, cibhptr);
            }
            if (cibhptr == 0) {
                return 0;
            } else {
                log.fine("found a cib: " + hex(cibh_ptr_cib));
                return cibh_ptr_cib;
            }
        } catch (Exception e) {
            throw new Error("oops: " + e);
        }
    }

    /**
     * Scan SFXM chain and return SFXM corresponding to the input dsa.
     */
    long findSfxm(long sfxm_dsa, int p_dsafmt) throws IOException {
        final long HEPV_Entry_Sig = 0x00C300C500C500L; /* Entry Marker signature */
        long p_sfxm = 0;
        try {
            if (p_dsafmt == US_FORMAT) {
                /* processing UPstack DSA */
                log.fine("findSfxm processing upstack dsa");
                long hcom_exit_stk = ceexhcomTemplate.getHcom_exit_stk(inputStream, caa.ceecaaerrcm());
                log.fine("findSfxm hcom_exit_stk = 0x" + hex(hcom_exit_stk));
                for (long tptr = hcom_exit_stk; tptr != 0; ) {
                    /* If the SFXM is for an UPSTACK DSA and that DSA     
                     * backchains to the input DSA then we have found     
                     * the SFXM that we want.                             
                     * NOTE: An SFXM that belongs to an UPSTACK DSA starts
                     * with a couple of NOPRs and a BALR. An SFXM that    
                     * belongs to a DOWNSTACK DSA starts with an XPLINK   
                     * style entry marker. */
                    long sfxm_code_eyecatch = CeexsfxmTemplate.getSfxm_code_eyecatch(inputStream, tptr);
                    if ((sfxm_code_eyecatch >>> 8) != HEPV_Entry_Sig) {
                        long sfxm_parm_sf = CeexsfxmTemplate.getSfxm_parm_sf(inputStream, tptr);
                        long ceedsabkc = CeedsaTemplate.getCeedsabkc(inputStream, sfxm_parm_sf);
                        if (ceedsabkc == sfxm_dsa) {
                            log.fine("found the upstack sfxm at " + hex(tptr));
                            p_sfxm = tptr;
                        }
                    }
                    tptr = CeexsfxmTemplate.getSfxm_next(inputStream, tptr);
                }
            } else {
                /* processing a DownStack DSA */
                log.fine("findSfxm processing downstack dsa");
                long hcom_exit_stk = ceexhcomTemplate.getHcom_exit_stk(inputStream, caa.ceecaaerrcm());
                log.fine("findSfxm hcom_exit_stk = 0x" + hex(hcom_exit_stk));
                int offset = CeexsfxmTemplate.getSfxm_code_return_pt$offset();
                long ceedsahpr7 = ceedsahpTemplate.getCeedsahpr7(inputStream, sfxm_dsa);
                ceedsahpr7 = space.stripTopBit(ceedsahpr7);
                for (long tptr = hcom_exit_stk; tptr != 0; ) {
                    if ((tptr + offset) == ceedsahpr7) {
                        /* Find the last SFXM by comparing Sfxm_Next with Sfxm_Save_R7.
                         * When they are not equal, we're at the last SFXM for this routine. */
                        throw new Error("tbc");
                    } else {
                        /* No match, go to next Sfxm */
                        tptr = CeexsfxmTemplate.getSfxm_next(inputStream, tptr);
                    }
                }
            }
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new Error("oops: " + e);
        }
        return p_sfxm;
    }

    /**
     * This class emulates the Ceexdsaf macro (Ph variant). Most of the comments are from Ceexdsaf.
     * The purpose of this class is to derive the previous stackframe from the passed address and
     * stackframe format.
     */
    static class Ceexdsaf {
        /** Desired output DSA address of previous frame */
        long DSA_Prev;
        /** Format of previous frame */
        int DSA_Format;
        /** The AddressSpaceImageInputStream */
        private AddressSpaceImageInputStream inputStream;

        Ceexdsaf(AddressSpace space, long DSA_In, int dsa_fmt) throws IOException {
            log.fine("enter Ceexdsaf for dsa " + hex(DSA_In));
            /* Create any templates we need if not already created */
			CeedsahpTemplate ceedsahpTemplate;
			if (space.is64bit()) {
                ceedsahpTemplate = new Ceedsahp64Template();
            } else {
                ceedsahpTemplate = new Ceedsahp32Template();
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
                        long ceedsahp_trtype = Ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsatran);
                        if (ceedsahp_trtype == Ceedsahp_DownToUp) {
                            DSA_Prev = Ceedsahp_transitionTemplate.getCeedsahp_bkc(inputStream, ceedsatran);
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
                    if (ceedsahpr7 == 0) {
                        long ceedsahptran = ceedsahpTemplate.getCeedsahptran(inputStream, DSA_In);
                        long ceedsahp_trtype = Ceedsahp_transitionTemplate.getCeedsahp_trtype(inputStream, ceedsahptran);
                        if (ceedsahp_trtype == Ceedsahp_UpToDown) {
                            DSA_Format = Caa.CEECAASTACK_UP;
                        }
                        /* Now return Ceedsahp_Bkc from tx area. Else, XPLINK-style backchain */
                        isTransitional = true;
                        DSA_Prev = Ceedsahp_transitionTemplate.getCeedsahp_bkc(inputStream, ceedsahptran);
                    } else {
                        /* Now do an XPL backchain. First we need the stackframe owner`s
                         * Entry Point. From this we can retrieve the SF`s DSA size. This
                         * is then added to the current DSA to locate the previous stackframe */
                        long Entry_Temp = hpclEntryPoint(space, DSA_In, ceedsahpr7, ceedsahpTemplate);
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
                    }
                }
            /* Propagate IO exceptions, all others are fatal errors */
            } catch (IOException e) {
                throw e;
            } catch (Exception e) {
                e.printStackTrace();
                throw new Error("oops: " + e);
            }
        }
    }

    static long hpclEntryPoint(AddressSpace space, long DSA_In, long ceedsahpr7, CeedsahpTemplate ceedsahpTemplate) throws Exception {
        log.fine("ceedsahpr7: " + hex(ceedsahpr7));
        AddressSpaceImageInputStream inputStream = space.getImageInputStream();
         /* We need to first check the Call Type to determine the Entry Point
          * of the routine that owns the SF. Valid call types are:
          *    00=BASR, 01=BRAS, and 03=BRASL
          */
        int Hpcl_Call_Type = (space.readInt(ceedsahpr7) >> 16) & 0xf;
        long Entry_Temp;
        if (Hpcl_Call_Type == Hpcl_Basr_Call) {
            /* Call Type BASR 7,6. Use Ceedsahpr6 to find entry */
            Entry_Temp = ceedsahpTemplate.getCeedsahpr6(inputStream, DSA_In);
            log.fine("BASR 7,6: " + hex(Entry_Temp));
        } else if (Hpcl_Call_Type == Hpcl_Bras_Call) {
            /* Call Type BRAS 7,xxx. Compute Entry by looking at instr offset */
            int offset = space.readShort(ceedsahpr7 - 2);
            log.fine("offset: " + hex(offset));
            //if (offset < 0)
                //throw new Error("is this supposed to be unsigned? " + offset);
            Entry_Temp = ceedsahpr7 - 4 + (offset * 2);
            log.fine("BRAS 7,xxx: " + hex(Entry_Temp) + (Entry_Temp < 0 ? " (neg)" : "") + " offset = " + offset);
        } else if (Hpcl_Call_Type == Hpcl_Brasl_Call) {
            /* Call Type BRASL 7,xxx. Compute Entry by looking at instr offset */
            int offset = space.readInt(ceedsahpr7 - 4);
            Entry_Temp = ceedsahpr7 - 6 + (offset * 2);
            log.fine("BRASL 7,xxx: " + hex(Entry_Temp));
        } else {
            /* Unable to find entry */
            return 0;
        }
        if (space.readInt(Entry_Temp - 16) != 0x00c300c5)
            //throw new Error("bad entry point eyecatcher: " + hex(space.readInt(Entry_Temp - 16)));
            Entry_Temp = 0;
        return Entry_Temp;
    }

    /**
     * This class emulates the Ceexepaf macro. Note that it only does the downstack variant.
     */
    class Ceexepaf {
        /** The PPA1 address */
        long ppa1_addr;
        /** The PPA2 address */
        long ppa2_addr;
        /** The entry point address */
        long entry_address;

        Ceexepaf(long DSA_In, int DSA_Format) throws IOException {
            try {
                /* Stack format is DOWN. If passed DSA is transitional, return Ceedsahpr6. Else,
                 * check call point info at R7 to determine the EP */
                long ceedsahpr7 = ceedsahpTemplate.getCeedsahpr7(inputStream, DSA_In);
                if (ceedsahpr7 == 0) {
                    entry_address = ceedsahpTemplate.getCeedsahpr6(inputStream, DSA_In);
                } else {
                    /* Not a transitional. Go get the XPLINK stackframe owner`s Entry Point.
                     * From this we can address the PPA1/PPA2
                     */

                    /* Share code with Ceexdsaf rather than repeating it */
                    entry_address = hpclEntryPoint(space, DSA_In, ceedsahpr7, ceedsahpTemplate);
                    assert entry_address != 0;
                }
                log.fine("read entry_address 0x" + hex(entry_address));
                int hepv_prefix_length = CeexhepvTemplate.getHepv_entry_point$offset();
                long hepv = entry_address - hepv_prefix_length;
                long hepv_ppa1_offset_p = CeexhepvTemplate.getHepv_ppa1_offset_p(inputStream, hepv);
                ppa1_addr = hepv + hepv_ppa1_offset_p;
                log.fine("ppa1 = 0x" + hex(ppa1_addr));
                long ppa1h_ppa2_off = Ceexhp1bTemplate.getPpa1h_ppa2_off(inputStream, ppa1_addr);
                ppa2_addr = ppa1_addr + ppa1h_ppa2_off;
                log.fine("ppa2 = 0x" + hex(ppa2_addr));
            } catch (IOException e) {
                /* Go easy on dodgy stack entries for now */
            } catch (Exception e) {
                e.printStackTrace();
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * This class emulates the Ceexppaf macro.
     */
    class Ceexppaf {
        /** The output pointer */
        long opt_ptr;

        Ceexppaf(long ppa1_ptr, String opt_nam) throws IOException {
            try {
                int ppa1h_lgth = Ceexhp1bTemplate.length();
                assert ppa1h_lgth == 20 : ppa1h_lgth;
                opt_ptr = ppa1_ptr + ppa1h_lgth;
                if (opt_nam.equals("NAM")) {
                    /* No table pointer was provided so get the one from the rcb */
                    long ceecaarcb = caa.ceecaarcb();
                    log.fine("ceecaarcb = " + hex(ceecaarcb));
                    long tabl_ptr = ceexrcbTemplate.getCeercb_ppa1tabl(inputStream, ceecaarcb);
                    log.fine("tabl_ptr = " + hex(tabl_ptr));
                    long ppa1h_flag3 = Ceexhp1bTemplate.getPpa1h_flag3(inputStream, ppa1_ptr);
                    log.fine("ppa1h_flag3 = " + hex(ppa1h_flag3));
                    int offset = space.readUnsignedByte(tabl_ptr + (ppa1h_flag3 & 0xff));
                    opt_ptr += offset;
                    log.fine("found offset of " + offset + ", opt_ptr now " + hex(opt_ptr));
                } else {
                    throw new Error("unsupported option: " + opt_nam);
                }
            } catch (IOException e) {
                /* Go easy on dodgy stack entries for now */
                opt_ptr = 0;
            } catch (Exception e) {
                e.printStackTrace();
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * Returns the function name for the given entry point address.
     * XXX This was borrowed from the old svcdump code. We really should reuse some of the
     * code elsewhere in this class that obtains the name but it's currently horribly
     * intertwined with other DSA logic. Might be possible to fake up a DSA?
     */
    public static String getEntryPointName(AddressSpace space, long ep) throws IOException {
        int eyecatcher;
        Long mapKey = Long.valueOf(ep);
        String function = (String)space.getUserMap().get(mapKey);
        if (function != null)
            return function;

        eyecatcher = space.readInt(ep + 4);
        if (eyecatcher == 0xc3c5c5 || eyecatcher == 0x1c3c5c5) {
            long ppa1 = ep + space.readInt(ep + 12);
            int offset = space.readUnsignedByte(ppa1);
            if (eyecatcher == 0x1c3c5c5) offset <<= 1;
            function = space.readEbcdicString(ppa1 + offset);
        } else if ((eyecatcher & 0xff0000) == 0xce0000) {
            long ppa1 = ep;
            int offset = eyecatcher >>> 24;
            function = space.readEbcdicString(ppa1 + offset);
        } else if ((space.readInt(ep) & 0xfffff000) == 0x47f0f000) {
            int length = space.readUnsignedByte(ep + 4);
            function = space.readEbcdicString(ep + 5, length);
            for (int i = 0; i < function.length(); i++) {
                char c = function.charAt(i);
                if (!Character.isLetterOrDigit(c) && !Character.isSpaceChar(c)) {
                    function = "(unknown)";
                    break;
                }
            }
        } else {
            long xplhdr = ep - 16;
            long ceesig = space.readLong(xplhdr);
            if (ceesig == 0x00C300C500C500F1L) {
                int ppa1off = space.readInt(xplhdr + 8);
                long ptrppa1 = xplhdr + ppa1off;
                long ptrnam = ptrppa1 + 0x14;
                int ppa1flags3 = space.readUnsignedByte(ptrppa1 + 10);
                int ppa1flags4 = space.readUnsignedByte(ptrppa1 + 11);
                /* step over all structures in ppa1 to get to name */
                /* state variable locator */
                if ((ppa1flags3 & 0x80) != 0)
                    ptrnam = ptrnam + 4;
                /* Argument Area Length */
                if ((ppa1flags3 & 0x40) != 0)
                    ptrnam = ptrnam + 4;
                /* FP reg save area locator */
                if ((ppa1flags3 & 0x20) != 0)
                    ptrnam = ptrnam + 4;
                /* reserved               */
                if ((ppa1flags3 & 0x10) != 0)
                    ptrnam = ptrnam + 4;
                /* PPA1 member word */
                if ((ppa1flags3 & 0x08) != 0)
                    ptrnam = ptrnam + 4;
                /* block debug info */
                if ((ppa1flags3 & 0x04) != 0)
                    ptrnam = ptrnam + 4;
                /* Interface mapping flags */
                if ((ppa1flags3 & 0x02) != 0)
                    ptrnam = ptrnam + 4;
                /* JAVA method locator table */
                if ((ppa1flags3 & 0x01) != 0)
                    ptrnam = ptrnam + 8;
                /* if no name is present  */
                if ((ppa1flags4 & 0x01) == 0) {
                    function = "(bad ppa1flags4)";
                } else {
                    function = space.readEbcdicString(ptrnam);
                }
            } else {
                function = "(unknown)";
            }
        }
        space.getUserMap().put(mapKey, function);
        return function;
    }
    
    private static String hex(long i) {
        return Long.toHexString(i);
    }
}
