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

import java.io.IOException;
import java.util.Properties;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpaceImageInputStream;

/**
 * This class represents an LE Enclave Data Block. This provides access to such things as the
 * chain of Dlls. Normally this object is obtained via the Caa, but as a convenience a method
 * is provided to obtain one directly from an AddressSpace.
 *
 * @has - - - com.ibm.dtfj.corereaders.zos.le.Dll
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
    /*static*/ CeexedbTemplate ceexedbTemplate;
    /** Logger */
    private static Logger log = Logger.getLogger(Edb.class.getName());

    /** Constructs a new Edb given the address of the edb structure */
    public Edb(long address, AddressSpace space) {
        this.address = address;
        this.space = space;
        inputStream = space.getImageInputStream();
        /* Create any templates we need if not already created */
        createTemplates(space);
    }

    /**
     * Create any templates we need if not already created.
     */
    /*static*/ void createTemplates(AddressSpace space) {
        if (ceexedbTemplate == null) {
            if (space.is64bit()) {
                ceexedbTemplate = new Ceexedb64Template();
            } else {
                ceexedbTemplate = new Ceexedb32Template();
            }
        }
    }

    /**
     * Returns the address of this edb.
     */
    public long address() {
        return address;
    }

    /** 
     * Returns the ceeosigr address
     * @throws IOException if an error occurred reading from the address space
     */
    public long ceeedb_ceeosigr() throws IOException {
        return ceexedbTemplate.getCeeedb_ceeosigr(inputStream, address);
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
     * Returns the first in the chain of Dlls (or null if there aren't any).
     * @throws IOException if an error occurred reading from the address space
     */
    public Dll getFirstDll() throws IOException {
        long ceeedb_dlcb_first = ceexedbTemplate.getCeeedb_dlcb_first(inputStream, address);
        log.fine("ceeedb_dlcb_first = " + hex(ceeedb_dlcb_first));
        return ceeedb_dlcb_first == 0 ? null : new Dll(ceeedb_dlcb_first, space);
    }

    /**
     * Return a java.util.Properties object containing the environment variables.
     */
    public Properties getEnvVars() throws IOException {
        if (envVars != null)
            return envVars;
        envVars = new Properties();
        long ceeedbenvar = ceexedbTemplate.getCeeedbenvar(inputStream, address);
        for (long env = 0; (env = space.readWord(ceeedbenvar)) != 0; ceeedbenvar += space.getWordLength()) {						
            String name = space.readEbcdicCString(env);
            int eq = name.indexOf('=');
            if (eq > 0) {
            	String key = name.substring(0, eq);
            	String value = name.substring(eq + 1);
            	envVars.put(key, value);
            } else {
            	// no equals sign, so not a complete property pair, just return the key
            	envVars.put(name, "");
            }
        }
        return envVars;
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
