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

import java.util.logging.*;
import java.util.*;
import java.io.IOException;

/**
 * This class represents an LE Enclave Data Block. This provides access to such things as the
 * chain of Dlls. Normally this object is obtained via the Caa, but as a convenience a method
 * is provided to obtain one directly from an AddressSpace.
 *
 * @has - - - com.ibm.zebedee.le.Dll
 */

public class Edb {

    /** The address of the EDB structure */
    private long address;
    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpaceImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** Environment variables for this enclave */
    private Properties envVars;
    /** Edb template - size depends on 32/64 bit mode */
    CeexedbTemplate ceexedbTemplate;
    /** Qpcb template */
    private CeexqpcbTemplate ceexqpcbTemplate;
    /** Qpcb Pool template */
    private CeexqpcbpoolTemplate ceexqpcbpoolTemplate;
    /** Is this a 64bit Edb? */
    private boolean is64bit;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
    /** Dll cache */
    private Dll firstDll = null;

    /** Constructs a new Edb given the address of the edb structure */
    public Edb(long address, AddressSpace space, boolean is64bit) {
        this.address = address;
        this.space = space;
        this.is64bit = is64bit;
        inputStream = space.getImageInputStream();
        createTemplates(space);
    }

    /**
     * Create any templates we need if not already created.
     */
    void createTemplates(AddressSpace space) {
        if (is64bit) {
            ceexedbTemplate = new Ceexedb64Template();
            ceexqpcbTemplate = new Ceexqpcb64Template();
            ceexqpcbpoolTemplate = new Ceexqpcbpool64Template();
        } else {
            ceexedbTemplate = new Ceexedb32Template();
            ceexqpcbTemplate = new Ceexqpcb32Template();
            ceexqpcbpoolTemplate = new Ceexqpcbpool32Template();
        }
    }

    /**
     * Returns the address of this edb.
     */
    public long address() {
        return address;
    }

    /** 
     * Returns the ceeedbensm address
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedbensm() throws IOException {
        return ceexedbTemplate.getCeeedbensm(inputStream, address);
    }

    public long ensm_aheap() throws IOException {
        assert !space.is64bit();
        return ceeedbensm() + EnsmTemplate.getEnsm_aheap$offset();
    }

    public long ensm_uheap() throws IOException {
        assert !space.is64bit();
        return ceeedbensm() + EnsmTemplate.getEnsm_uheap$offset();
    }

    /** 
     * Returns the ceeedboptcb (options control block - ocb) address
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedboptcb() throws IOException {
        return ceexedbTemplate.getCeeedboptcb(inputStream, address);
    }

    /** 
     * Returns the ceeosigr address
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedb_ceeosigr() throws IOException {
        return ceexedbTemplate.getCeeedb_ceeosigr(inputStream, address);
    }

    /** 
     * Returns the qpcb address
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedb_qpcb() throws IOException {
        return ceexedbTemplate.getCeeedb_qpcb(inputStream, address);
    }

    /**
     * Given a malloc'd pointer, returns the size of the area. Note that this may not be the
     * actual size requested in the original malloc call if heap pools are in use because heap
     * pools round up to the nearest pool size. This method is mainly for use in estimating
     * memory usage.
     * @throws IOException if an error occurred reading from the address space
     * @throws IllegalArgumentException if the pointer was not the address of a malloc'd area
     */
    public long getMallocSize(long ptr) throws IOException, IllegalArgumentException {
        long index1 = space.readWord(ptr - 2*space.getWordLength());
        long index2 = space.readWord(ptr - space.getWordLength());
        if (index1 != index2) {
            /* Normal malloc, index2 should be the size and index1 the HANC pointer */
            try {
                String hanc = space.readEbcdicString(index1, 4);
                if (!hanc.equals("HANC")) {
                    throw new IllegalArgumentException("Did not find HANC eyecatcher at index1 " + hex(index1) + " given ptr " + hex(ptr) + " index2 " + hex(index2));
                }
                if (index2 < 0) {
                    throw new IllegalArgumentException("Negative size " + index2 + " given ptr " + hex(ptr));
                }
                return index2;
            } catch (IOException e) {
                throw new IllegalArgumentException("Got exception while reading hanc pointer " + hex(index1) + " given ptr " + hex(ptr));
            }
        }
        /* Must be a heap pool */
        if (index1 < 1 || index1 > 12) {
            throw new IllegalArgumentException("Unexpected index1 " + hex(index1) + " for ptr " + hex(ptr));
        }
        long qpcb = ceeedb_qpcb();
        assert space.readEbcdicString(qpcb, 4).equals("QPCB");
        long pool = qpcb + ceexqpcbTemplate.getQpcb_pool_data_array$offset();
        pool += (index1 - 1) * ceexqpcbpoolTemplate.length();
        long pool_index = ceexqpcbpoolTemplate.getQpcb_pool_index(inputStream, pool);
        //assert pool_index == index1 : "bad pool index " + pool_index + " does not match " + index1;
        if (pool_index != index1) {
            log.fine("bad pool index " + pool_index + " does not match " + index1);
            return 0;
        }
        long size = ceexqpcbpoolTemplate.getQpcb_input_cell_size(inputStream, pool);
        assert (size + 2*space.getWordLength()) == ceexqpcbpoolTemplate.getQpcb_cell_size(inputStream, pool);
        return size;
    }

    /** 
     * Returns the ceeedbdba 
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedbdba() throws IOException {
        return ceexedbTemplate.getCeeedbdba(inputStream, address);
    }

    /**
     * Returns a "sample" Edb for the given address space. Note that for certain applications
     * (CICS?) there can be more than one Edb per address space. Returns null if there is no Edb.
     * XXX to be removed once we fix the AddressSpace/Process issue
     */
    public static Edb getSampleEdb(AddressSpace space) {
        Caa[] caas = Caa.getCaas(space);
        /* XXX this needs fixing. The first couple of Caas can point to strange Edbs.
         * Don't understand it yet. */
        for (int i = 0; i < caas.length; i++) {
            Edb edb = caas[i].getEdb();
            log.fine("found edb " + edb + " for caa " + caas[i]);
            try {
                if (edb.getFirstDll() != null) {
                    log.fine("edb has a dll: " + edb.getFirstDll());
                    return edb;
                }
            } catch (IOException e) {
                log.fine("caught exception: " + e);
            }
        }
        log.fine("no sample edb found");
        return null;
    }

    /**
     * Returns an array of all the Edbs in the given address space.
     */
    public static Edb[] getEdbs(AddressSpace space) {
        HashSet set = new HashSet();
        Caa[] caas = Caa.getCaas(space);
        for (int i = 0; i < caas.length; i++) {
            Edb edb = caas[i].getEdb();
            log.fine("found edb " + edb + " for caa " + caas[i]);
            try {
                if (edb.getFirstDll() != null) {
                    log.fine("edb has a dll: " + edb.getFirstDll());
                    set.add(edb);
                }
            } catch (IOException e) {
                log.fine("caught exception: " + e);
            }
        }
        log.fine("no sample edb found");
        return (Edb[])set.toArray(new Edb[0]);
    }

    /**
     * Returns the first in the chain of Dlls (or null if there aren't any).
     * @throws IOException if an error occurred reading from the address space
     */
    public Dll getFirstDll() throws IOException {
    	if (firstDll == null) {
    		long ceeedb_dlcb_first = ceexedbTemplate.getCeeedb_dlcb_first(inputStream, address);
    		log.fine("ceeedb_dlcb_first = " + hex(ceeedb_dlcb_first));
    		if (ceeedb_dlcb_first != 0) {
    			// cache the first Dll, which will eventually cache the whole chain
    			firstDll = new Dll(ceeedb_dlcb_first, space);
    		}
    	}
    	return firstDll;
    }

    /**
     * Return a java.util.Properties object containing the environment variables.
     */
    public Properties getEnvVars() throws IOException {
        if (envVars != null)
            return envVars;
        envVars = new Properties();
        long ceeedbenvar = ceexedbTemplate.getCeeedbenvar(inputStream, address);
        for (long env = 0; (env = space.readWord(ceeedbenvar, is64bit)) != 0; ceeedbenvar += (is64bit ? 8 : 4)) {
            String name = space.readEbcdicCString(env);
            int eq = name.indexOf('=');
            if (eq <= 0) {
                log.fine("bad name: " + name);
                continue;
            }
            String key = name.substring(0, eq);
            String value = name.substring(eq + 1);
            envVars.put(key, value);
        }
        return envVars;
    }

    /** Returns true if the LE runtime option RPTSTG(ON) is set */
    public boolean rptstg() throws IOException {
        return (space.readUnsignedByte(ceeedboptcb() + 0xcc) & 0x80) != 0;
    }

    /** Returns true if the LE runtime option STORAGE(ON) is set */
    public boolean storage() throws IOException {
        return (space.readUnsignedByte(ceeedboptcb() + 0xec) & 0x80) != 0;
    }

    public boolean equals(Object obj) {
        Edb e = (Edb)obj;
        return e.space == space && e.address == address;
    }

    public int hashCode() {
        return (int)address;
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    public String toString() {
        return hex(address);
    }
}
