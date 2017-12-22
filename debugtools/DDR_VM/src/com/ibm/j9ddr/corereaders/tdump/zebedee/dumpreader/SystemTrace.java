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

package com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader;

import java.io.*;
import java.util.*;
import java.util.logging.*;

import com.ibm.j9ddr.corereaders.tdump.zebedee.util.ProgressMeter;

/**
 * This class represents the System Trace mechanism. Various inner classes represent the
 * System Trace entires themselves.
 */

public class SystemTrace {

    /** The AddressSpace that the system trace lives in */
    private AddressSpace space;
    /** The List of system trace entries */
    private ArrayList entries = new ArrayList();
    /** Maps processor ids to Context objects */
    private HashMap contextMap = new HashMap();
    /** The cached trace entry address */
    private int cachedAddress;
    /** The cached attributes map. This is the map associated with cachedAddress. */
    private Map cachedMap;
    /** Properties file to map PC numbers to their descriptions */
    private Properties pcDescriptions;
    /** Properties file to map SVC numbers to their descriptions */
    private Properties svcDescriptions;
    /** Properties file to map SSRV numbers to their descriptions */
    private Properties ssrvDescriptions;
    private static int debugIndex;  // XXX get rid of this later
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    public static final int UNKNOWN = 0;
    public static final int SSCH = 0x1;
    public static final int EXT  = 0x3;
    public static final int SVC  = 0x5;
    public static final int PGM  = 0x7;
    public static final int SPER = 0x9;
    public static final int IO   = 0xb;
    public static final int DSP  = 0xf;
    public static final int MCH  = 0x13;
    public static final int RST  = 0x15;
    public static final int ACR  = 0x17;
    public static final int SUSP = 0x19;
    public static final int ALTR = 0x1b;
    public static final int RCVY = 0x1d;
    public static final int TIME = 0x1f;
    public static final int MSCH = 0x101;
    public static final int EMS  = 0x103;
    public static final int SVCR = 0x105;
    public static final int SRB  = 0x10f;
    public static final int HSCH = 0x201;
    public static final int SS   = 0x203;
    public static final int SSRV = 0x205;
    public static final int SSRB = 0x20f;
    public static final int CSCH = 0x301;
    public static final int CALL = 0x303;
    public static final int RSCH = 0x401;
    public static final int CLKC = 0x403;
    public static final int SIGA = 0x501;
    public static final int XSCH = 0x601;
    public static final int SVCE = 0xf05;
    public static final int WAIT = 0xf0f;
    public static final int USR0 = 0x7f;

    public static final int SSAR = 0x10;
    public static final int PC   = 0x21;
    public static final int PC64 = 0x22;
    public static final int PT   = 0x31;
    public static final int PR   = 0x32;
    public static final int BSG =  0x41;

    /**
     * Create a new System Trace object. There is just one such object per dump.
     * @param space the AddressSpace that the system trace lives in
     */
    public SystemTrace(AddressSpace space) throws IOException {
        this.space = space;
        /* Load the PC number descriptions */
        InputStream is = getClass().getResourceAsStream("pcnumbers.properties");
        pcDescriptions = new Properties();
        pcDescriptions.load(is);
        is.close();
        /* Load the SVC number descriptions */
        is = getClass().getResourceAsStream("svcnumbers.properties");
        svcDescriptions = new Properties();
        svcDescriptions.load(is);
        is.close();
        /* Load the SSRV number descriptions */
        is = getClass().getResourceAsStream("ssrvnumbers.properties");
        ssrvDescriptions = new Properties();
        ssrvDescriptions.load(is);
        is.close();
        /* Create the list of trace entries */
        readTrace();
        /* Now sort them in date order */
        ProgressMeter.set("sorting trace entries", 0);
        final int nlogn = entries.size() * log2(entries.size());
        Collections.sort(entries, new Comparator() {
            int n;
            public int compare(Object o1, Object o2) {
                if (n < nlogn)
                    ProgressMeter.set((n++ * 100)/nlogn);
                Entry e1 = (Entry)o1;
                Entry e2 = (Entry)o2;
                if (e1.getRawTime() == e2.getRawTime()) {
                    /* Either one or both must be a TimelessEntry so use the address instead */
                    return e1.address - e2.address;
                } else {
                    return e1.getRawTime() < e2.getRawTime() ? -1 : 1;
                }
            }

            public boolean equals(Object obj) {
                throw new Error("oops");
            }
        });
    }

    /**
     * Returns the list of system trace entries. 
     */
    public List getEntries() {
        return entries;
    }

    private boolean inDebugRange(int index) {
        //(ind >= 260667 && ind <= 260707) || (ind >= 210545 && ind <= 210556)
        //return index >= 248580 && index <= 248618;
        return false;
    }

    private void readTrace() throws IOException {
        long ttch = space.getLowestAddress();
        /* Format of TTCH changed in release 9 of z/OS */
        int zosRelease = 0;
        try {
            zosRelease = Integer.parseInt(space.getDump().getProductRelease());
        } catch (NumberFormatException e) {
            throw new Error("oops: " + e);
        }
        boolean newFormat = zosRelease >= 9;
        int numProcessors = space.readUnsignedShort(ttch + 0x1c);
        int todtop = space.readInt(ttch + 0x20);
        long tod = todtop >> 16;
        tod <<= 48;
        long lastTod = tod;
        AddressSpace lastSpace = null;

        /* About the orphans stuff: I can't quite seem to replicate the logic that systrace
         * uses. If I move lastTimedEntry to the outer proc loop it produces more mismatches
         * (although logically it seems more correct). */
        ProgressMeter.set("reading trace entries", 0);
        for (int proc = 0; proc < numProcessors; proc++) {
            if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :go to proc " + proc);
            long procEntry = ttch + 0x38 + (proc * (newFormat ? 16 : 8));
            int ttchnumb = newFormat ? space.readInt(procEntry + 8) : space.readUnsignedShort(procEntry);
            int ttchcpid = space.readUnsignedShort(procEntry + 2);  // processor id
            int ttchbptr = space.readInt(procEntry + 4);
            int tcb = 0;

            for (int buffer = 0; buffer < ttchnumb; buffer++) {
                if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :go to buffer " + buffer);
                ProgressMeter.set((proc * 100)/numProcessors + (buffer * (100/numProcessors))/ttchnumb);
                int flags = space.readUnsignedShort(ttchbptr + (buffer * 40) + 8);
                int ttchcptr = space.readInt(ttchbptr + (buffer * 40));
                int ttchclen = space.readInt(ttchbptr + (buffer * 40) + 4);
                Entry entry = null;
                Entry lastTimedEntry = null;
                ArrayList orphans = null;

                for (int entryPtr = ttchcptr; entryPtr < (ttchcptr + ttchclen); entryPtr += entry.length()) {
                    int id = space.readUnsignedByte(entryPtr);
                    if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :go to id " + id);
                    if (id < 0x70 || id >= 0x80) {
                        Context context = getContext(tod, ttchcpid, 0);
                        entry = getTimelessEntry(context, entryPtr, lastTimedEntry);
                        if (lastTimedEntry != null) {
                            entries.add(entry);
                            if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :add to entry " + lastTimedEntry.index);
                        } else {
                            if (orphans == null)
                                orphans = new ArrayList();
                            orphans.add(entry);
                            if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :orphan");
                        }
                    } else {
                        Context context = getContext(tod, ttchcpid, space.readUnsignedShort(entryPtr + 8));
                        entry = getEntry(context, entryPtr);
                        if (orphans != null) {
                            if (inDebugRange(debugIndex)) System.out.println("index " + debugIndex + " :adding orphans to " + entry.index);
                            for (Iterator it = orphans.iterator(); it.hasNext();) {
                                TimelessEntry orphan = (TimelessEntry)it.next();
                                orphan.lastTimedEntry = entry;
                                entries.add(orphan);
                            }
                            orphans = null;
                        }
                        entries.add(entry);
                        lastTimedEntry = entry;
                    }
                }
            }
        }
    }

    private Entry getEntry(Context context, int address) throws IOException {
        int type = space.readUnsignedShort(address + 10);
        switch (type) {
        case SSCH: return new SSCH(context, address);
        case EXT:  return new EXT(context, address);
        case SVC:  return new SVC(context, address);
        case PGM:  return new PGM(context, address);
        case SPER: return new SPER(context, address);
        case IO:   return new IO(context, address);
        case DSP:  return new DSP(context, address);
        case MCH:  return new MCH(context, address);
        case RST:  return new RST(context, address);
        case ACR:  return new ACR(context, address);
        case SUSP: return new SUSP(context, address);
        case ALTR: return new ALTR(context, address);
        case RCVY: return new RCVY(context, address);
        case TIME: return new TIME(context, address);
        case MSCH: return new MSCH(context, address);
        case EMS:  return new EMS(context, address);
        case SVCR: return new SVCR(context, address);
        case SRB:  return new SRB(context, address);
        case HSCH: return new HSCH(context, address);
        case SS:   return new SS(context, address);
        case SSRV: return new SSRV(context, address);
        case SSRB: return new SSRB(context, address);
        case CSCH: return new CSCH(context, address);
        case CALL: return new CALL(context, address);
        case RSCH: return new RSCH(context, address);
        case CLKC: return new CLKC(context, address);
        case SIGA: return new SIGA(context, address);
        case XSCH: return new XSCH(context, address);
        case SVCE: return new SVCE(context, address);
        case WAIT: return new WAIT(context, address);
        case USR0: return new USR0(context, address);
        default:
            log.info("Unknown entry type: " + type);
            return new UNKNOWN(context, address);
        }
    }

    private TimelessEntry getTimelessEntry(Context context, int address, Entry lastTimedEntry) throws IOException {
        switch (space.readUnsignedByte(address)) {
        case SSAR: return new SSAR(context, address, lastTimedEntry);
        case PC:   return new PC(context, address, lastTimedEntry);
        case PC64: return new PC64(context, address, lastTimedEntry);
        case PT:   return new PT(context, address, lastTimedEntry);
        case PR:   return new PR(context, address, lastTimedEntry);
        case BSG:  return new BSG(context, address, lastTimedEntry);
        default:
            log.info("Unknown timeless entry type: " + space.readUnsignedByte(address));
            return new UNKNOWNT(context, address, lastTimedEntry);
        }
    }

    /**
     * This class represents the system trace entries. To save space we only store the
     * address of the entry as an int and dynamically resolve the description.
     */
    public abstract class Entry {
        int address;
        Context context;
        public int index;  // XXX temp!

        Entry(Context context, int address) {
            this.address = address;
            this.context = context;
            index = debugIndex++;
        }

        public int getEntryAddress() {
            return address;
        }

        int getId() throws IOException {
            return space.readUnsignedByte(address);
        }

        public int length() throws IOException {
            return 16 + ((getId() & 0xf) * 4);
        }

        /**
         * Return the identifier of the processor that produced this trace entry.
         */
        public String getProcessorId() {
            return context.getProcessorId();
        }

        /**
         * Return the physical CPU id (the CP field).
         */
        public String getPhysicalProcessorId() {
            return context.getPhysicalProcessorId();
        }

        long getRawTime() {
            try {
                return space.readLong(address) & 0xffffffffffffffL;
            } catch (IOException e) {
                assert false;
            }
            return 0;
        }

        /**
         * Return the timestamp.
         */
        public String getTimestamp() {
            return hex(getRawTime() | context.tod);
        }

        /**
         * Return true of this entry has its own timestamp.
         */
        public boolean hasTimestamp() {
            return true;
        }

        /** Return the home asid (Address Space id) for this entry */
        int getHomeAsid() throws IOException {
            return space.readUnsignedShort(address + 18);
        }

        /** Return the asid (Address Space id) for this entry */
        public String getAsidString() throws IOException {
            return hex(getAsid());
        }

        /** Return the asid (Address Space id) for this entry */
        public int getAsid() throws IOException {
            int type = space.readUnsignedShort(address + 10);
            if (type == 0x401 || type == 1) {    // RSCH or SSCH
                return space.readUnsignedShort(address + 16);
            } else {
                return getHomeAsid();
            }
        }

        /** Return the tcb address (aka WU-ADDR) for this entry */
        public String getTcbAddress() throws IOException {
            return hex(getTcb());
        }

        public int getTcb() throws IOException {
            return space.readInt(address + 12);
        }

        public abstract String getIdentString();
        public abstract int getIdent();

        int codeLength()      { return 2; }
        int pswLength()       { return 4; }
        int u2Length()        { return 4; }
        int u3Length()        { return 4; }
        int u4Length()        { return 4; }
        int u5Length()        { return 4; }
        int clhsLength()      { return 4; }

        int codeOffset()      { return 0; }
        int pswOffset()       { return 0; }
        int addressOffset()   { return 0; }
        int u1Offset()        { return 0; }
        int u2Offset()        { return 0; }
        int u3Offset()        { return 0; }
        int u4Offset()        { return 0; }
        int u5Offset()        { return 0; }
        int u6Offset()        { return 0; }
        int clhsOffset()      { return 0; }
        int localOffset()     { return 0; }
        int clhseOffset()     { return 0; }
        int pasdOffset()      { return 0; }
        int sasdOffset()      { return 0; }

        Integer getField(int offset, int length) throws IOException {
            if (offset != 0) {
                if (length == 1) {
                    return new Integer(space.readUnsignedByte(address + offset));
                } else if (length == 2) {
                    return new Integer(space.readUnsignedShort(address + offset));
                } else {
                    return new Integer(space.readInt(address + offset));
                }
            }
            return null;
        }

        public int readInt(int offset) throws IOException {
            return space.readInt(address + offset);
        }

        String getFieldString(Integer field) throws IOException {
            return field == null ? null : hex(field.intValue());
        }

        public Integer getCode() throws IOException { return getField(codeOffset(), codeLength()); }
        public String getCodeString() throws IOException { return getFieldString(getCode()); }
        public Integer getPsw() throws IOException { return getField(pswOffset(), pswLength()); }
        public String getPswString() throws IOException { return getFieldString(getPsw()); }
        public Integer getAddress() throws IOException { return getField(addressOffset(), 4); }
        public String getAddressString() throws IOException { return getFieldString(getAddress()); }
        public Integer getUnique1() throws IOException { return getField(u1Offset(), 4); }
        public String getUnique1String() throws IOException { return getFieldString(getUnique1()); }
        public Integer getUnique2() throws IOException { return getField(u2Offset(), u2Length()); }
        public String getUnique2String() throws IOException { return getFieldString(getUnique2()); }
        public Integer getUnique3() throws IOException { return getField(u3Offset(), u3Length()); }
        public String getUnique3String() throws IOException { return getFieldString(getUnique3()); }
        public Integer getUnique4() throws IOException { return getField(u4Offset(), u4Length()); }
        public String getUnique4String() throws IOException { return getFieldString(getUnique4()); }
        public Integer getUnique5() throws IOException { return getField(u5Offset(), u5Length()); }
        public String getUnique5String() throws IOException { return getFieldString(getUnique5()); }
        public Integer getUnique6() throws IOException { return getField(u6Offset(), 4); }
        public String getUnique6String() throws IOException { return getFieldString(getUnique6()); }
        public Integer getPsaclhs() throws IOException { return getField(clhsOffset(), clhsLength()); }
        public String getPsaclhsString() throws IOException { return getFieldString(getPsaclhs()); }
        public Integer getPsalocal() throws IOException { return getField(localOffset(), 4); }
        public String getPsalocalString() throws IOException { return getFieldString(getPsalocal()); }
        public Integer getPsaclhse() throws IOException { return getField(clhseOffset(), 4); }
        public String getPsaclhseString() throws IOException { return getFieldString(getPsaclhse()); }
        public Integer getPasd() throws IOException { return getField(pasdOffset(), 2); }
        public String getPasdString() throws IOException { return getFieldString(getPasd()); }
        public Integer getSasd() throws IOException { return getField(sasdOffset(), 2); }
        public String getSasdString() throws IOException { return getFieldString(getSasd()); }
        public String getDescription() throws IOException { return null; }
    }

    /**
     * This class represents system trace entries without timestamps. An extra word is used to
     * hold the last entry that had a timestamp. This is used for sorting purposes.
     * See IHATTE for descriptions of entries.
     */
    private abstract class TimelessEntry extends Entry {
        Entry lastTimedEntry;

        TimelessEntry(Context context, int address, Entry lastTimedEntry) {
            super(context, address);
            this.lastTimedEntry = lastTimedEntry;
        }

        public int length() throws IOException {
            return ((getId() & 0xf) + 1) * 4;
        }

        long getRawTime() {
            return lastTimedEntry.getRawTime();
        }

        public String getTimestamp() {
            return lastTimedEntry.getTimestamp();
        }

        public boolean hasTimestamp() {
            return false;
        }

        public int getAsid() throws IOException {
            return lastTimedEntry.getHomeAsid();
        }

        public String getTcbAddress() throws IOException {
            return lastTimedEntry.getTcbAddress();
        }

        public String getCodeString() throws IOException {
            return " ...";
        }

        public String getAddressString() throws IOException {
            if (addressOffset() == 0)
                return null;
            else
                return hex(space.readInt(address + addressOffset()) & 0x7fffffff);
        }
    }

    /**
     * This class stores some context about the current trace entry. Do this rather than
     * store the info in the trace entry itself to save space.
     */
    private class Context {
        long tod;
        String processorId;
        String physicalProcessorId;

        Context(long tod, int pid, int ppid) {
            this.tod = tod;
            this.processorId = hexpad(pid, 2);
            this.physicalProcessorId = hexpad(ppid, 2);
        }

        String getProcessorId() {
            return processorId;
        }

        String getPhysicalProcessorId() {
            return physicalProcessorId;
        }
    }

    private Context getContext(long tod, int pid, int ppid) {
        Integer key = new Integer((pid << 16) | ppid);
        Context context = (Context)contextMap.get(key);
        if (context == null) {
            context = new Context(tod, pid, ppid);
            contextMap.put(key, context);
        }
        return context;
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    private static String hexpad(int word) {
        return hexpad(word, 8);
    }
    
    private static String hexpad(int word, int length) {
        String s = Integer.toHexString(word);

        for (int i = s.length(); i < length; i++)
            s = "0" + s;
        return s;
    }

    private static String spacepad(String s, int length) {
        for (int i = s.length(); i < length; i++)
            s = " " + s;
        return s;
    }

    private static String rspacepad(String s, int length) {
        for (int i = s.length(); i < length; i++)
            s += " ";
        return s;
    }

    private static String hexpad(long word) {
        String s = Long.toHexString(word);

        for (int i = s.length(); i < 8; i++)
            s = "0" + s;
        return s;
    }

    /* Here follow the classes for the various trace entry types */

    private class SSCH extends Entry {
        SSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SSCH"; }
        public int getIdent()           { return SSCH; }
        int codeOffset()                { return 22; }
        public String getCodeString() throws IOException {
            int code = space.readUnsignedShort(address + 22);
            int u6 = space.readUnsignedShort(address + 46);
            if (code != u6)
                code |= 0x10000;
            return hexpad(code, 5);
        }
        int pswOffset()                 { return 20; }
        int pswLength()                 { return 2; }
        public String getPswString() throws IOException {
            return hexpad(space.readUnsignedByte(address + 20) & 3, 2) + "  " + hexpad(space.readUnsignedByte(address + 21), 2) + "  ";
        }
        int addressOffset()             { return 40; }
        int u1Offset()                  { return 24; }
        int u2Offset()                  { return 28; }
        int u3Offset()                  { return 32; }
        int u4Offset()                  { return 46; }
        int u4Length()                  { return 2; }
        public String getUnique4String() throws IOException {
            int code = space.readUnsignedShort(address + 22);
            int u6 = space.readUnsignedShort(address + 46);
            if (code != u6)
                return spacepad(hexpad(u6, 4), 8);
            else
                return null;
        }
    }

    private class EXT extends Entry {
        EXT(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "EXT"; }
        public int getIdent()           { return EXT; }
        int codeOffset()                { return 32; }
        int codeLength()                { return 4; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 32; }
        int clhsOffset()                { return 36; }
        int localOffset()               { return 40; }
        int clhseOffset()               { return 44; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class SVC extends Entry {
        SVC(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SVC"; }
        public int getIdent()           { return SVC; }
        int codeOffset()                { return 16; }
        int pswOffset()                 { return 20; }
        int addressOffset()             { return 24; }
        int u1Offset()                  { return 28; }
        int u2Offset()                  { return 32; }
        int u3Offset()                  { return 36; }

        public String getDescription() throws IOException {
            int svcnumber = space.readUnsignedShort(address + 16);
            int r15 = space.readInt(address + 28);
            int r0 = space.readInt(address + 32);
            int r1 = space.readInt(address + 36);
            switch (svcnumber) {
            case 0xa:
                return r1 < 0 ? "Getmain" : "Freemain";
            case 0x78:
                return (r15 & 1) == 1 ? "Freemain" : "Getmain";
            case 0x2e:
                if ((r1 & 8) == 0) {
                    if ((r1 & 1) == 1) {
                        return "TTimer Cancel";
                    } else {
                        return "TTimer Set";
                    }
                } else {
                    if ((r1 & 1) == 1) {
                        return "STimerM Cancel";
                    } else {
                        return "STimerM";
                    }
                }
            case 0x2f:
                if ((r0 & 0x04000000) == 0) {
                    return "STimer   Set";
                } else {
                    return "STimerM  Set";
                }
            case 0x4f:
                switch (r0 & 0xffff) {
                case 0x1: return "MCStep Set";
                case 0x3: return "NDStep Set";
                case 0x4: return "NDSys Set";
                case 0x5: return "NDTcb Set";
                case 0x6: return "Stop  SRBs/TCBs";
                default:  return "Uknown";
                }
/*
4F Status   NDTcb Reset
4F Status   SDTcb Set
4F Status   Start SRBs only
4F Status   Start SRBs/TCBs
4F Status   Start TCB(s)
*/
            default:
                return svcDescriptions.getProperty(hex(svcnumber).toUpperCase());
            }
        }
    }

    private class PGM extends Entry {
        PGM(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "PGM"; }
        public int getIdent()           { return PGM; }
        int codeOffset()                { return 46; }
        public String getCodeString() throws IOException {
            return hexpad(space.readUnsignedShort(address + 46), 3);
        }
        int pswOffset()                 { return 36; }
        int addressOffset()             { return 40; }
        int u1Offset()                  { return 44; }
        int u2Offset()                  { return 48; }
        int u5Offset()                  { return 52; }
        int clhsOffset()                { return 24; }
        int localOffset()               { return 28; }
        int clhseOffset()               { return 32; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class IO extends Entry {
        IO(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "I/O"; }
        public int getIdent()           { return IO; }
        int codeOffset()                { return 16; }
        public String getCodeString() throws IOException {
            int code = space.readUnsignedShort(address + 16);
            int u4 = space.readInt(address + 64);
            if ((u4 & 0x00020000) != 0)
                code |= 0x10000;
            return hexpad(code, 5);
        }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 32; }
        int u2Offset()                  { return 36; }
        int u3Offset()                  { return 40; }
        int u4Offset()                  { return 64; }
        public String getUnique4String() throws IOException {
            int u4 = space.readInt(address + 64);
            if ((u4 & 0x00020000) != 0)
                return spacepad(hexpad(u4 & 0xffff, 4), 8);
            else
                return null;
        }
        int u5Offset()                  { return 48; }
        int u6Offset()                  { return 44; }
        int clhsOffset()                { return 52; }
        int localOffset()               { return 56; }
        //int clhseOffset()               { try {System.out.println("I/O: " + hex(space.readInt(address + 64)));} catch (Exception e) {} return 60; }
        int clhseOffset()               { return 60; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class DSP extends Entry {
        DSP(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "DSP"; }
        public int getIdent()           { return DSP; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 40; }
        int u2Offset()                  { return 32; }
        int u3Offset()                  { return 36; }
        int clhsOffset()                { return 44; }
        int localOffset()               { return 48; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class SUSP extends Entry {
        SUSP(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SUSP"; }
        public int getIdent()           { return SUSP; }
        int addressOffset()             { return 20; }
        int u1Offset()                  { return 24; }
        int u2Offset()                  { return 32; }
        int u3Offset()                  { return 36; }
        int u4Offset()                  { return 28; }
        int clhsOffset()                { return 40; }
        int localOffset()               { return 44; }
        int clhseOffset()               { return 48; }

        public String getUnique2String() throws IOException {
            return space.readEbcdicString(address + 32, 4) + "    ";
        }
    }

    private class EMS extends Entry {
        EMS(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "EMS"; }
        public int getIdent()           { return EMS; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 32; }
        int u2Offset()                  { return 36; }
        int u3Offset()                  { return 40; }
        int u4Offset()                  { return 44; }
        int clhsOffset()                { return 48; }
        int localOffset()               { return 52; }
        int clhseOffset()               { return 56; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class SRB extends Entry {
        SRB(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SRB"; }
        public int getIdent()           { return SRB; }
        int pswOffset()                 { return 20; }
        int addressOffset()             { return 24; }
        int u1Offset()                  { return 28; }
        int u2Offset()                  { return 32; }
        int u3Offset()                  { return 36; }
        int u4Offset()                  { return 40; }
        int u5Offset()                  { return 16; }
        int u5Length()                  { return 1; }
        public String getUnique5String() throws IOException {
            return rspacepad(hexpad(space.readUnsignedByte(address + 16), 2), 8);
        }
        int clhsOffset()                { return 17; }
        int clhsLength()                { return 1; }
        public String getPsaclhsString() throws IOException {
            return spacepad(hexpad(space.readUnsignedByte(address + 17), 2), 8);
        }
        int pasdOffset()                { return 18; }
        int sasdOffset()                { return 18; }
    }

    private class SVCR extends Entry {
        SVCR(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SVCR"; }
        public int getIdent()           { return SVCR; }
        int codeOffset()                { return 16; }
        int pswOffset()                 { return 20; }
        int addressOffset()             { return 24; }
        int u1Offset()                  { return 28; }
        int u2Offset()                  { return 32; }
        int u3Offset()                  { return 36; }
    }

    private class SSRV extends Entry {
        SSRV(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SSRV"; }
        public int getIdent()           { return SSRV; }
        int codeOffset()                { return 16; }
        int addressOffset()             { return 20; }
        int u1Offset()                  { return 24; }
        int u2Offset()                  { return 28; }
        int u3Offset()                  { return 32; }
        int u4Offset()                  { return 36; }

        public String getDescription() throws IOException {
            int ssid = space.readUnsignedShort(address + 16);
            switch (ssid) {
            case 0x5f:
                int r0 = space.readInt(address + 28);   // assumption
                switch (r0 & 0xff) {
                case 0x03:
                    return "Sysevent NIOWait";
                case 0x04:
                    return "Sysevent UserRdy";
                case 0x0c:
                    return "Sysevent QsceSt";
                case 0x0d:
                    return "Sysevent QsceCmp";
                case 0x14:
                    return "Sysevent EnqHold";
                case 0x15:
                    return "Sysevent EnqRlse";
                case 0x29:
                    return "Sysevent DontSwap";
                case 0x2a:
                    return "Sysevent OkSwap";
                case 0x52:
                    return "Sysevent ReqASD";
                case 0x57:
                    return "Sysevent EncCreat";
                case 0x58:
                    return "Sysevent EncDelet";
                case 0x69:
                    return "Sysevent WlmQueue";
                case 0x6a:
                    return "Sysevent EncAssoc";
                default:
                    return "Unknown r0: " + hex(r0);
                }
            case 0x78:
                int r15 = space.readInt(address + 24);
                return (r15 & 1) == 1 ? "Freemain" : "Getmain";
            default:
                return ssrvDescriptions.getProperty(hex(ssid).toUpperCase());
            }
        }
    }

    private class SSRB extends Entry {
        SSRB(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SSRB"; }
        public int getIdent()           { return SSRB; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 40; }
        int u3Offset()                  { return 36; }
        int u4Offset()                  { return 32; }
        int clhsOffset()                { return 17; }
        int clhsLength()                { return 1; }
        public String getPsaclhsString() throws IOException {
            return spacepad(hexpad(space.readUnsignedByte(address + 17), 2), 8);
        }
        int localOffset()               { return 44; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class CALL extends Entry {
        CALL(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "CALL"; }
        public int getIdent()           { return CALL; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 32; }
        int u2Offset()                  { return 36; }
        int clhsOffset()                { return 40; }
        int localOffset()               { return 44; }
        int clhseOffset()               { return 48; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class RSCH extends Entry {
        RSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "RSCH"; }
        public int getIdent()           { return RSCH; }
        int codeOffset()                { return 22; }
        public String getCodeString() throws IOException {
            return hexpad(space.readUnsignedShort(address + 22), 5);
        }
        int pswOffset()                 { return 20; }
        int pswLength()                 { return 2; }
        public String getPswString() throws IOException {
            return hexpad(space.readUnsignedByte(address + 20) & 3, 2) + "  " + hexpad(space.readUnsignedByte(address + 21), 2) + "  ";
        }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 24; }
    }

    private class SIGA extends Entry {
        SIGA(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SIGA"; }
        public int getIdent()           { return SIGA; }
        int codeOffset()                { return 22; }
        int pswOffset()                 { return 20; }
        int pswLength()                 { return 2; }
        public String getPswString() throws IOException {
            return hexpad(space.readUnsignedByte(address + 20) & 3, 2) + "  " + hexpad(space.readUnsignedByte(address + 21), 2) + "  ";
        }
        int addressOffset()             { return 36; }
        int u1Offset()                  { return 24; }
        int u2Offset()                  { return 28; }
        int u3Offset()                  { return 32; }
        int u4Offset()                  { return 40; }
    }

    private class UNKNOWN extends Entry {
        UNKNOWN(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "UNKNOWN"; }
        public int getIdent()           { return UNKNOWN; }
    }

    private class WAIT extends Entry {
        WAIT(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "WAIT"; }
        public int getIdent()           { return WAIT; }
    }

    private class HSCH extends Entry {
        HSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "HSCH"; }
        public int getIdent()           { return HSCH; }
    }

    private class SS extends Entry {
        SS(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SS"; }
        public int getIdent()           { return SS; }
    }

    private class CSCH extends Entry {
        CSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "CSCH"; }
        public int getIdent()           { return CSCH; }
    }

    private class CLKC extends Entry {
        CLKC(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "CLKC"; }
        public int getIdent()           { return CLKC; }
        int pswOffset()                 { return 24; }
        int addressOffset()             { return 28; }
        int u1Offset()                  { return 32; }
        int u2Offset()                  { return 36; }
        int u3Offset()                  { return 42; }
        int u3Length()                  { return 2;  }
        public String getUnique3String() throws IOException {
            return "    " + hexpad(space.readUnsignedShort(address + 42), 4);
        }
        int clhsOffset()                { return 44; }
        int localOffset()               { return 48; }
        int clhseOffset()               { return 52; }
        int pasdOffset()                { return 20; }
        int sasdOffset()                { return 22; }
    }

    private class XSCH extends Entry {
        XSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "XSCH"; }
        public int getIdent()           { return XSCH; }
    }

    private class SVCE extends Entry {
        SVCE(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SVCE"; }
        public int getIdent()           { return SVCE; }
    }

    private class USR0 extends Entry {
        USR0(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "USR0"; }
        public int getIdent()           { return USR0; }
    }

    private class MCH extends Entry {
        MCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "MCH"; }
        public int getIdent()           { return MCH; }
    }

    private class RST extends Entry {
        RST(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "RST"; }
        public int getIdent()           { return RST; }
    }

    private class ACR extends Entry {
        ACR(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "ACR"; }
        public int getIdent()           { return ACR; }
    }

    private class ALTR extends Entry {
        ALTR(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "ALTR"; }
        public int getIdent()           { return ALTR; }
    }

    private class RCVY extends Entry {
        RCVY(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "RCVY"; }
        public int getIdent()           { return RCVY; }
    }

    private class TIME extends Entry {
        TIME(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "TIME"; }
        public int getIdent()           { return TIME; }
    }

    private class MSCH extends Entry {
        MSCH(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "MSCH"; }
        public int getIdent()           { return MSCH; }
    }

    private class SPER extends Entry {
        SPER(Context context, int address) { super(context, address); }
        public String getIdentString()  { return "SPER"; }
        public int getIdent()           { return SPER; }
    }

    private class UNKNOWNT extends TimelessEntry {
        UNKNOWNT(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "UNKNOWN"; }
        public int getIdent()           { return UNKNOWN; }
    }

    private class SSAR extends TimelessEntry {
        SSAR(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "SSAR"; }
        public int getIdent()           { return SSAR; }
        public String getPswString() throws IOException {
            return spacepad(hexpad(space.readUnsignedShort(address + 2), 4), 8);
        }
        int sasdOffset()                { return 2; }
    }

    private class BSG extends TimelessEntry {
        BSG(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "BSG"; }
        public int getIdent()           { return BSG; }
        public String getPswString() throws IOException {
            return spacepad(hexpad(space.readInt(address) & 0xffffff, 4), 8);
        }
        int addressOffset()             { return 4; }
    }

    private class PC extends TimelessEntry {
        PC(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "PC"; }
        public int getIdent()           { return PC; }
        public String getPswString() throws IOException {
            return "  " + (space.readUnsignedByte(address + 1) >> 4) + "     ";
        }
        int u2Offset()                  { return 2; }
        int u2Length()                  { return 2; }
        int addressOffset()             { return 4; }
        String pcnumber() throws IOException {
            return hexpad(space.readInt(address) & 0xfffff, 5);
        }
        public String getUnique2String() throws IOException {
            return spacepad(pcnumber(), 8);
        }
        public String getDescription() throws IOException {
            return pcDescriptions.getProperty(pcnumber().toUpperCase());
        }
    }

    private class PC64 extends TimelessEntry {
        PC64(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "PC"; }
        public int getIdent()           { return PC64; }
        public String getPswString() throws IOException {
            return "  " + (space.readUnsignedByte(address + 1) >> 4) + "   0 ";
        }
        int addressOffset()             { return 4; }
        int u2Offset()                  { return 8; }
    }

    private class PR extends TimelessEntry {
        PR(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "PR"; }
        public int getIdent()           { return PR; }
        public String getPswString() throws IOException {
            return "  " + (space.readUnsignedByte(address + 1) >> 4) + "     ";
        }
        int addressOffset()             { return 4; }
        int pasdOffset()                { return 2; }
        public String getUnique2String() throws IOException {
            if ((space.readUnsignedByte(address + 1) & 2) == 2)
                return "00_" + hexpad(space.readInt(address + 8));
            else
                return hexpad(space.readInt(address + 8) & 0x7fffffff);
        }
    }

    private class PT extends TimelessEntry {
        PT(Context context, int address, Entry lastTimedEntry) { super(context, address, lastTimedEntry); }
        public String getIdentString()  { return "PT"; }
        public int getIdent()           { return PT; }
        public String getPswString() throws IOException {
            return "  " + (space.readUnsignedByte(address + 1) >> 4) + "     ";
        }
        int addressOffset()             { return 4; }
        public String getUnique2String() throws IOException {
            return hexpad(space.readUnsignedShort(address + 2), 4) + "    ";
        }
    }

    private static final int log2bytes[] = {
        -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };

    private static int log2(int n) {
        int bit = 0;
        while ((n & 0xffffff00 ) != 0) {
            bit += 8;
            n >>>= 8;
        }
        return bit + log2bytes[n];
    }
}
