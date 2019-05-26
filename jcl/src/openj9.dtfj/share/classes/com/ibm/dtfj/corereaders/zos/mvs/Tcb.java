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
package com.ibm.dtfj.corereaders.zos.mvs;

import java.io.IOException;
import java.util.Hashtable;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpaceImageInputStream;

/**
 * This class represents an MVS Task Control Block (TCB). Methods that begin with "tcb"
 * return the value of fields as defined in the IKJTCB macro.
 *
 * @depend - - - com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace
 * @has - - - com.ibm.dtfj.corereaders.zos.mvs.RegisterSet
 */

public class Tcb {

    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpace ImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** The address of our tcb structure */
    private long address;
    /** The Linkage Stack */
    private Lse[] linkageStack;
    /** Map AddressSpace to its array of Tcbs */
    private static Hashtable tcbMap = new Hashtable();
    /** PRB */
    private static final int RBFTPRB = 0;
    private static final int FastPathPCLow = 0x1307;
    private static final int OmvsPcLow = 0x1300;
    /** Logger */
    private static Logger log = Logger.getLogger(Tcb.class.getName());

    /**
     * Returns an array containing all of the Tcbs in the given
     * {@link com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace}. Returns null if
     * this address space contains no Tcbs.
     */
    public static Tcb[] getTcbs(AddressSpace space) {
    	if(space == null) {
    		return null;
    	}
        AddressSpaceImageInputStream inputStream = space.getImageInputStream();
        if(space.getRootAddressSpace() == null) {
        	return null;
        }
        AddressSpaceImageInputStream rootInputStream = space.getRootAddressSpace().getImageInputStream();
        Tcb[] tcbs = (Tcb[])tcbMap.get(space);
        if (tcbs != null) {
            return tcbs;
        }
        log.fine("creating Tcb array for asid " + space);
        Vector v = new Vector();
        try {
            /*
             * We go from the PSA to the ASCB and then the ASXB which contains pointers
             * to the first and last TCBs
             */
            long psaaold = IhapsaTemplate.getPsaaold(rootInputStream, 0);
            if (psaaold == 0) {
                log.fine("psaaold is zero so no tcbs in asid " + space);
                return null;
            }
            long ascbasxb = IhaascbTemplate.getAscbasxb(rootInputStream, psaaold);
            long asxbftcb = IhaasxbTemplate.getAsxbftcb(inputStream, ascbasxb);
            long asxbltcb = IhaasxbTemplate.getAsxbltcb(inputStream, ascbasxb);
            if (asxbftcb == asxbltcb) {
                log.fine("first and last tcb pointers are equal so no tcbs in asid " + space);
                return null;
            }
            /* Now follow the TCB chain creating objects for each */
            for (Tcb tcb = new Tcb(space, asxbftcb); ; tcb = new Tcb(space, tcb.tcbtcb())) {
                v.add(tcb);
                if (tcb.address() == asxbltcb)
                    break;
            }
        } catch (Exception e) {
            if (log.isLoggable(Level.FINER))
                e.printStackTrace();
            log.fine("exception: " + e);
            return null;
        }
        /* Add the array of TCBs to the map */
        tcbs = (Tcb[])v.toArray(new Tcb[0]);
        tcbMap.put(space, tcbs);

        return tcbs;
    }

    /**
     * Returns the address of this Tcb
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
     * Create a new Tcb for the given address space.
     * @param space the {@link com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace} to which this Tcb belongs
     * @param address the address of this Tcb
     */
    public Tcb(AddressSpace space, long address) {
        log.fine("creating Tcb at address " + hex(address));
        this.space = space;
        this.address = address;
        inputStream = space.getImageInputStream();
    }

    /**
     * Return the celap pointer for this Tcb.
     * @throws IOException if an error occurred reading from the address space
     */
    public long tcbcelap() throws IOException {
        return IkjtcbTemplate.getTcbcelap(inputStream, address);
    }

    /**
     * Return the rtwa pointer for this Tcb.
     * @throws IOException if an error occurred reading from the address space
     */
    public long tcbrtwa() throws IOException {
        return IkjtcbTemplate.getTcbrtwa(inputStream, address);
    }

    /**
     * Return the rbp pointer for this Tcb.
     * @throws IOException if an error occurred reading from the address space
     */
    public long tcbrbp() throws IOException {
        return IkjtcbTemplate.getTcbrbp(inputStream, address);
    }

    /**
     * Return the stcb pointer for this Tcb.
     * @throws IOException if an error occurred reading from the address space
     */
    public long tcbstcb() throws IOException {
        return IkjtcbTemplate.getTcbstcb(inputStream, address);
    }

    /**
     * Return the pointer to the next Tcb in the queue
     * @throws IOException if an error occurred reading from the address space
     */
    public long tcbtcb() throws IOException {
        return IkjtcbTemplate.getTcbtcb(inputStream, address);
    }

    /**
     * Get registers from the TCB itself. The registers are in the TCB and the PSW
     * comes from the RB.
     */
    public RegisterSet getRegisters() throws IOException {
        log.fine("getRegisters");
        RegisterSet regs = new RegisterSet();
        try {
            long tcbgrs = address + IkjtcbTemplate.getTcbgrs$offset();
            for (int i = 0; i < 16; i++) {
                regs.setRegister(i, space.readUnsignedInt(tcbgrs + i*4));
            }
            regs.setPSW(IharbTemplate.getRbopsw(inputStream, tcbrbp()));
            if (space.is64bit()) {
                /* Or in the top half from the stcb */
                long stcbg64h = tcbstcb() + IhastcbTemplate.getStcbg64h$offset();
                for (int i = 0; i < 16; i++) {
                    long high = space.readUnsignedInt(stcbg64h + i*4);
                    regs.setRegister(i, regs.getRegister(i) | (high << 32));
                }
            }
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new Error("oops: " + e);
        }
        return regs;
    }

    /**
     * Try and get the registers by simulating the BPXGMSTA service. (XXX I think this is
     * probably the best place to implement this kernel call?)
     */
    public RegisterSet getRegistersFromBPXGMSTA() throws IOException {
        RegisterSet regs = new RegisterSet();
        /* Following comments are from bpxgmsta.plx:
         *
         * BPXGMSTA accesses the dump to get data necessary to             
         * extract PSW and GPRs relevant to the input TCB.                 
         *                                                                 
         * 1) Single RB on the input TCB RB chain                          
         *    a) If current linkage stack is empty, registers are extracted
         *       from the input TCB and PSW is extracted from the current   
         *       RB.                                                       
         *    b) If linkage stack is present, and non-fast path Kernel     
         *       call has been issued, registers and PSW are extracted     
         *       from USTA, which can be accessed via the input TCB.       
         *    c) For all other cases, first entry of the linkage stack     
         *       contains the registers and PSW from the last point        
         *       where LE/C had control.                                   
         * 2) multiple RBs on the input TCB RB chain                       
         *    a) PSW is extracted from the oldest RB on the chain, which   
         *       is the first RB added to the chain.  And the adjacent     
         *       RB to the oldest RB contains the registers from the last  
         *       point where LE/C had control.                             
         * 3) Any other scenario not covered by above cases are            
         *    considered to be very unlikely to occur.                     
         */
        try {
            long rbp = tcbrbp();
            long rbsecptr = 0;    /* The last RB read */
            long save2rbptr = 0;  /* The second to last RB read */
            log.fine("for tcb " + hex(address) + ", rbp = " + hex(rbp));
            int countRbs = 0;
            do {
                log.fine("currently looking at rbp 0x" + hex(rbp));
                save2rbptr = rbsecptr;
                rbsecptr = rbp;
                rbp = IharbTemplate.getRblinkXrblnkRblinkb(inputStream, rbp);
                countRbs++;
            } while (rbp != address);
            log.fine("found " + countRbs + " rbs");
            /*
             * If only a single RB exist in the RB chain, GPRs and PSW must
             * be obtained from the linkage stack.  If the linkage stack is
             * empty, then GPRs and PSW must be obtained from TCB and RB
             * respectively.
             * Else if multiple RB exist in the RB chain, obtain GPRs from
             * the oldest PRB and PSW from the RB next to it.
             */
            if (countRbs == 1) {
                getLinkageStack();
                if (linkageStack.length == 0) {
                    /*
                     * This case represents waiting in LE or suspended by
                     * dumping.  TCB is ready to run but not yet dispatched.
                     * Target GPRs in TCB and target PSW is in RB.
                     */
                    log.fine("empty linkage stack, get registers from TCB");
                    regs = getRegisters();
                    regs.setWhereFound("BPXGMSTA/TCB");
                } else {
                    /* Most likely a stacking PC.  use first linkage stack entry which
                     * contains GPRs and PSW from the last point where LE/C had control. */
                    long stcbotcb = IhastcbTemplate.getStcbotcb(inputStream, tcbstcb());
                    boolean otcbptregsinusta = false;
                    if (stcbotcb != 0) {
                        otcbptregsinusta = BpxzotcbTemplate.getOtcbptregsinusta(inputStream, stcbotcb) != 0;
                    }
                    if (otcbptregsinusta && stcbotcb != 0) {
                        long otcbcofptr = BpxzotcbTemplate.getOtcbcofptr(inputStream, stcbotcb);
                        throw new Error("tbc");
                    } else {
                        log.fine("try first linkage stack entry");
                        /* map to 1st linkage stack entry. Get pointer to start of register savearea in
                         * state entry. 1st entry must be either PC or BAKR status entry since linkage stack
                         * can't be empty. If the first LSE indicates this is a OMVS syscall and not a
                         * fastpath syscall get the registers from the usta. */
                        Lse lse = linkageStack[linkageStack.length - 1];
                        /* Check the first LSE on the stack for the OMVS non-fastpath 
                         * syscall range  */
                        assert stcbotcb != 0;
                        long otcbcofptr = BpxzotcbTemplate.getOtcbcofptr(inputStream, stcbotcb);
                        long otcbustaptr = OtcbcopyonforkTemplate.getOtcbustaptr(inputStream, otcbcofptr);
						log.fine("lse type "+lse.lses1typ7());
                        if ((lse.lsestyp7() == Lse.LSEDPC || lse.lses1typ7() == Lse.LSED1PC) && lse.lsestarg() < FastPathPCLow && lse.lsestarg() >= OmvsPcLow && otcbustaptr != 0) {
                            /* If this is a slowpath syscall get the USTA regs */
                            long ustagrs = otcbustaptr + BpxzustaTemplate.getUstagrs$offset();
                            /* This is a bit of a hack to adjust the usta pointer according to
                             * the OS level. Eventually we should let the template stuff handle
                             * multiple variants of structures. Usta psw increased in HBB7708. */
                            long cvt = IhapsaTemplate.getFlccvt(inputStream, 0);
                            long cvtoslv3 = CeexcvtTemplate.getCvtoslv3(inputStream, cvt);
                            if ((cvtoslv3 & 2) != 0) {
                                log.fine("new level of OS (0x" + hex(cvtoslv3) + ")");
                            } else {
                                log.fine("old level of OS (0x" + hex(cvtoslv3) + ") so adjust usta pointer");
                                ustagrs -= 8;   // take into account smaller psw
                            }
                            for (int i = 0; i < 16; i++) {
                                regs.setRegister(i, space.readUnsignedInt(ustagrs + i*4));
                            }
                            long ustapsw = otcbustaptr + BpxzustaTemplate.getUstapswg$offset();
                            regs.setPSW(space.readLong(ustapsw));
                            regs.setWhereFound("BPXGMSTA/USTA");
                        } else {
                            log.fine("try last linkage stack entry");
                            /* last entry in linkage stack. Calculate the address of the
                             * last LSE on the chain */
                            lse = linkageStack[0];
                            if (lse.lses1typ7() == Lse.LSED1PC || lse.lses1typ7() == Lse.LSED1BAKR) {
                                /* Esame PC entry */
                                regs.setPSW(lse.lses1pswh());
                                for (int i = 0; i < 16; i++) {
                                    regs.setRegister(i, lse.lses1grs(i));
                                }
                            } else {
                                regs.setPSW(lse.lsespsw());
                                for (int i = 0; i < 16; i++) {
                                    regs.setRegister(i, lse.lsesgrs(i));
                                }
                            }
                            regs.setWhereFound("BPXGMSTA/Linkage");
                        }
                    }
                }
            } else {
                long rbftp = IkjrbTemplate.getRbftp(inputStream, rbsecptr);
                if (rbftp == RBFTPRB) {
                    regs.setPSW(IharbTemplate.getRbopsw(inputStream, rbsecptr));
                    long rbgrsave_offset = IharbTemplate.getRbgrsave$offset();
                    for (int i = 0; i < 16; i++) {
                        regs.setRegister(i, space.readUnsignedInt(save2rbptr + rbgrsave_offset + i*4));
                    }
                    regs.setWhereFound("BPXGMSTA/RBFTPRB");
                } else {
                    throw new Error("tbc");
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            throw new Error("oops: " + e);
        }
        return regs;
    }

    /**
     * Return the linkage stack as an array of Lse entries. The length of the array will be zero
     * if the linkage stack is empty. The top of the stack (as represented by stcblsdp) is the
     * first element in the array and the end of the stack (also sometimes referred to as
     * the first entry, aka stcbestk) is the last element in the array.
     */
    public Lse[] getLinkageStack() throws IOException {
        if (linkageStack != null)
            return linkageStack;
        try {
            long stcb = tcbstcb();
            long stcbestk = IhastcbTemplate.getStcbestk(inputStream, stcb);
            long stcblsdp = IhastcbTemplate.getStcblsdp(inputStream, stcb);
            if (stcbestk == stcblsdp) {
                log.fine("linkage stack empty");
                linkageStack = new Lse[0];
                return linkageStack;
            }
            long entrysize = LsedTemplate.getLsednes(inputStream, stcbestk);
            if(entrysize <= 0) {
            	log.fine("linkage stack empty");
                linkageStack = new Lse[0];
                return linkageStack;
            }
            // XXX maybe this should be an exception?
            assert entrysize == LsestateTemplate.length() || entrysize == Lsestate1Template.length() : entrysize;
            assert (stcblsdp - stcbestk) % entrysize == 0 : (stcblsdp - stcbestk) % entrysize;
            int numEntries = (int)((stcblsdp - stcbestk) / entrysize);
            assert numEntries >= 0 && numEntries < 100 : numEntries;
            linkageStack = new Lse[numEntries];
            long lsedptr = stcblsdp - entrysize;
            for (int i = 0; i < numEntries; i++) {
                linkageStack[i] = new Lse(space, lsedptr);
                lsedptr -= entrysize;
            }
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new Error("oops: " + e);
        }
        return linkageStack;
    }

    private static String hex(long n) {
        return Long.toHexString(n);
    }
}
