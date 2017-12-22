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
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpaceImageInputStream;

/**
 * This class represents a DLL (Dynamically Linked Library). 
 */

public class Dll {

    /** The address of the DLCB structure */
    private long address;
    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpaceImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** The name of this DLL */
    private String name;
    /** The WSA (Writable Static Area) address */
    private long wsa;
    /** The next DLL in the chain (or null) */
    private Dll next;
    /** The array of DllVariables */
    private DllVariable[] variables;
    /** The array of DllFunctions */
    private DllFunction[] functions;
    /** Dlcb template - size depends on 32/64 bit mode */
    /*static*/ CeexdlcbTemplate ceexdlcbTemplate;
    /** Logger */
    private static Logger log = Logger.getLogger(Dll.class.getName());

    /**
     * Constructs a new Dll given the address of the DLCB (DLL Control Block) structure.
     * The normal way for a user to get hold of an instance though would be via
     * {@link com.ibm.dtfj.corereaders.zos.le.Edb#getFirstDll}.
     */
    public Dll(long address, AddressSpace space) {
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
        if (ceexdlcbTemplate == null) {
            if (space.is64bit()) {
                ceexdlcbTemplate = new Ceexdlcb64Template();
            } else {
                ceexdlcbTemplate = new Ceexdlcb32Template();
            }
        }
    }

    /**
     * Returns the name of this DLL.
     * @throws IOException
     */
    public String getName() throws IOException {
        if (name == null) {
            long dlcbnamelen = ceexdlcbTemplate.getDlcbnamelen(inputStream, address);
            long dlcbnameptr = ceexdlcbTemplate.getDlcbnameptr(inputStream, address);
            name = space.readEbcdicString(dlcbnameptr, (int)dlcbnamelen);
        }
        return name;
    }

    /**
     * Returns the load address for this DLL.
     */
    public long getLoadAddress() {
        throw new Error("tbc");
    }

    /**
     * Returns the address of the WSA (Writable Static Area) for this DLL.
     * @throws IOException
     */
    public long getWsa() throws IOException {
        if (wsa == 0) {
            wsa = ceexdlcbTemplate.getDlcbwsaptr(inputStream, address);
        }
        return wsa;
    }

    /**
     * Returns the next Dll in the chain (or null if none).
     * @throws IOException
     */
    public Dll getNext() throws IOException {
        if (next == null) {
            long dlcbnextptr = ceexdlcbTemplate.getDlcbnextptr(inputStream, address);
            if (dlcbnextptr != 0)
                next = new Dll(dlcbnextptr, space);
        }
        return next;
    }

    /** 
     * Returns the named {@link com.ibm.dtfj.corereaders.zos.le.DllVariable} (or null if it can't be found).
     */
    public DllVariable getVariable(String name) throws IOException {
        if (getVariables() == null)
            return null;
        for (int i = 0; i < variables.length; i++) {
            if (variables[i].name.equals(name)) {
                return variables[i];
            }
        }
        return null;
    }

    /**
     * Returns an array of the {@link com.ibm.dtfj.corereaders.zos.le.DllVariable}s belonging to this DLL
     */
    public DllVariable[] getVariables() throws IOException {
        if (variables != null)
            return variables;
        long dlcbiewbcie = ceexdlcbTemplate.getDlcbiewbcie(inputStream, address);
        int eyecatcher = space.readInt(dlcbiewbcie);
        if (eyecatcher == 0xC9C5E6C2) {     // IEWB
            long ciet2_version = Ciet2Template.getCiet2_version(inputStream, dlcbiewbcie);
            if (ciet2_version == 2) {
                long ciet2_var_count = Ciet2Template.getCiet2_var_count(inputStream, dlcbiewbcie);
                assert ciet2_var_count >=0 && ciet2_var_count < 1000000 : ciet2_var_count;
                long ciet2_var_addr = Ciet2Template.getCiet2_var_addr(inputStream, dlcbiewbcie);
                variables = new DllVariable[(int)ciet2_var_count];
                for (int i = 0; i < ciet2_var_count; i++) {
                    try {
                        long base = ciet2_var_addr + (i * Ciet2ExpVarEntryTemplate.length());
                        long ciet2_exp_var_is_addr = Ciet2ExpVarEntryTemplate.getCiet2_exp_var_is_addr(inputStream, base);
                        long ciet2_exp_var_offset = Ciet2ExpVarEntryTemplate.getCiet2_exp_var_offset(inputStream, base);
                        long ciet2_exp_var_name_addr = Ciet2ExpVarEntryTemplate.getCiet2_exp_var_name_addr(inputStream, base);
                        String varName = space.readEbcdicString(ciet2_exp_var_name_addr);
                        long varAddr = ciet2_exp_var_is_addr == 0 ? ciet2_exp_var_offset + getWsa() : ciet2_exp_var_offset;
                        long varValue = space.readInt(varAddr);
                        variables[i] = new DllVariable(varName, varAddr, varValue);
                    } catch (IOException e) {
                        variables[i] = new DllVariable(e.toString(), 0, 0);
                    }
                }
            } else if (ciet2_version == 1) {
                long ciet_var_count = CietTemplate.getCiet_var_count(inputStream, dlcbiewbcie);
                long ciet_var_addr = CietTemplate.getCiet_var_addr(inputStream, dlcbiewbcie);
                ciet_var_addr = space.readInt(ciet_var_addr);
                log.finer("ciet_var_addr = " + hex(ciet_var_addr));
                variables = new DllVariable[(int)ciet_var_count];
                log.finer("count = " + ciet_var_count);
                for (int i = 0; i < ciet_var_count; i++) {
                    long base = ciet_var_addr + (i * CietExpEntryTemplate.length());
                    log.finer("base = " + hex(base));
                    long ciet_is_addr = CietExpEntryTemplate.getCiet_is_addr(inputStream, base);
                    log.finer("ciet_is_addr = " + ciet_is_addr);
                    long ciet_is_function = CietExpEntryTemplate.getCiet_is_function(inputStream, base);
                    log.finer("ciet_is_function = " + hex(ciet_is_function));
                    long ciet_exp_offset = CietExpEntryTemplate.getCiet_exp_offset(inputStream, base);
                    log.finer("ciet_exp_offset = " + hex(ciet_exp_offset));
                    long ciet_exp_name_addr = CietExpEntryTemplate.getCiet_exp_name_addr(inputStream, base);
                    log.finer("ciet_exp_name_addr = " + hex(ciet_exp_name_addr) + " length = " + space.readUnsignedShort(ciet_exp_name_addr));
                    String varName = space.readEbcdicString(ciet_exp_name_addr);
                    log.finer("varName = " + varName);
                    long varAddr = ciet_is_addr == 0 ? ciet_exp_offset + getWsa() : ciet_exp_offset;
                    log.finer("varAddr = " + hex(varAddr));
                    log.finer("ciet_is_addr = " + hex(ciet_is_addr));
                    long varValue = 0;
                    try {
                        varValue = space.readInt(varAddr);
                    } catch (IOException e) {} // ignore bad addresses
                    variables[i] = new DllVariable(varName, varAddr, varValue);
                }
            } else
                throw new Error("expected ciet2_version 1 or 2 but instead found " + ciet2_version);
        } else if (space.readEbcdicString(dlcbiewbcie, 8).equals("@@DL370$")) {
            long dlloffexpvar = DllcsectTemplate.getDlloffexpvar(inputStream, dlcbiewbcie);
            long dllexpvars = dlcbiewbcie + dlloffexpvar;
            long dllexpvarscount = DllexpvarsTemplate.getDllexpvarscount(inputStream, dllexpvars);
            if (dllexpvarscount < 0 || dllexpvarscount > 1000000) {
                log.config("impossible dllexpvarscount: " + dllexpvarscount);
                variables = new DllVariable[0];
                return variables;
            }
            variables = new DllVariable[(int)dllexpvarscount];
            for (int i = 0; i < dllexpvarscount; i++) {
                long dllexpvarsname = DllexpvarsTemplate.getDllexpvarsname(inputStream, dllexpvars);
                long dllexpvarsqcon = DllexpvarsTemplate.getDllexpvarsqcon(inputStream, dllexpvars);
                String varName = space.readEbcdicString(dlcbiewbcie + dllexpvarsname);
                long varAddr = getWsa() + dllexpvarsqcon;
                long varValue = space.readInt(varAddr);
                variables[i] = new DllVariable(varName, varAddr, varValue);
                // XXX this is naughty - we should handle arrays properly
                dllexpvars += DllexpvarsTemplate.getDllexpvarsarray$length() / 8;
            }
        } else {
            throw new Error("tbc");
        }
        return variables;
    }

    /** 
     * Returns the named {@link com.ibm.dtfj.corereaders.zos.le.DllFunction} (or null if it can't be found).
     */
    public DllFunction getFunction(String name) throws IOException {
        if (getFunctions() == null)
            return null;
        for (int i = 0; i < functions.length; i++) {
            if (functions[i].name.equals(name)) {
                return functions[i];
            }
        }
        return null;
    }

    /**
     * Returns an array of the {@link com.ibm.dtfj.corereaders.zos.le.DllFunction}s belonging to this DLL
     */
    public DllFunction[] getFunctions() throws IOException {
        if (functions != null)
            return functions;
        long dlcbiewbcie = ceexdlcbTemplate.getDlcbiewbcie(inputStream, address);
        int eyecatcher = space.readInt(dlcbiewbcie);
        if (eyecatcher == 0xC9C5E6C2) {     // IEWB
            long ciet2_version = Ciet2Template.getCiet2_version(inputStream, dlcbiewbcie);
            if (ciet2_version == 2) {
                long ciet2_func_count = Ciet2Template.getCiet2_func_count(inputStream, dlcbiewbcie);
                assert ciet2_func_count >=0 && ciet2_func_count < 1000000 : ciet2_func_count;
                long ciet2_func_addr = Ciet2Template.getCiet2_func_addr(inputStream, dlcbiewbcie);
                functions = new DllFunction[(int)ciet2_func_count];
                for (int i = 0; i < ciet2_func_count; i++) {
                    try {
                        long base = ciet2_func_addr + (i * Ciet2ExpFuncEntryTemplate.length());
                        long ciet2_exp_func_is_addr = Ciet2ExpFuncEntryTemplate.getCiet2_exp_func_is_addr(inputStream, base);
                        long ciet2_exp_func_offset = Ciet2ExpFuncEntryTemplate.getCiet2_exp_func_offset(inputStream, base);
                        long ciet2_exp_func_name_addr = Ciet2ExpFuncEntryTemplate.getCiet2_exp_func_name_addr(inputStream, base);
                        String funcName = space.readEbcdicString(ciet2_exp_func_name_addr);
                        long funcAddr = ciet2_exp_func_is_addr == 0 ? ciet2_exp_func_offset + getWsa() : ciet2_exp_func_offset;
                        long ciet2_exp_ada_is_addr = Ciet2ExpFuncEntryTemplate.getCiet2_exp_ada_is_addr(inputStream, base);
                        long ciet2_exp_func_ada_offset = Ciet2ExpFuncEntryTemplate.getCiet2_exp_func_ada_offset(inputStream, base);
                        long funcAda = ciet2_exp_ada_is_addr == 0 ? ciet2_exp_func_ada_offset + getWsa() : ciet2_exp_func_ada_offset;
                        functions[i] = new DllFunction(funcName, funcAddr, null, funcAda);
                    } catch (IOException e) {
                        functions[i] = new DllFunction(e.toString(), 0, null, space.WILD_POINTER);
                    }
                }
            } else if (ciet2_version == 1) {
                long ciet_func_count = CietTemplate.getCiet_func_count(inputStream, dlcbiewbcie);
                long ciet_func_addr = CietTemplate.getCiet_func_addr(inputStream, dlcbiewbcie);
                ciet_func_addr = space.readInt(ciet_func_addr);
                functions = new DllFunction[(int)ciet_func_count];
                log.finer("count = " + ciet_func_count);
                for (int i = 0; i < ciet_func_count; i++) {
                    long base = ciet_func_addr + (i * CietExpEntryTemplate.length());
                    log.finer("base = " + hex(base));
                    long ciet_is_addr = CietExpEntryTemplate.getCiet_is_addr(inputStream, base);
                    long ciet_is_function = CietExpEntryTemplate.getCiet_is_function(inputStream, base);
                    log.finer("ciet_is_function = " + hex(ciet_is_function));
                    long ciet_exp_offset = CietExpEntryTemplate.getCiet_exp_offset(inputStream, base);
                    log.finer("ciet_exp_offset = " + hex(ciet_exp_offset));
                    long ciet_exp_name_addr = CietExpEntryTemplate.getCiet_exp_name_addr(inputStream, base);
                    log.finer("ciet_exp_name_addr = " + hex(ciet_exp_name_addr) + " length = " + space.readUnsignedShort(ciet_exp_name_addr));
                    String funcName = space.readEbcdicString(ciet_exp_name_addr);
                    log.finer("funcName = " + funcName);
                    long funcAddr = ciet_is_addr == 0 ? ciet_exp_offset + getWsa() : ciet_exp_offset;
                    log.finer("funcAddr = " + hex(funcAddr));
                    log.finer("ciet_is_addr = " + hex(ciet_is_addr));
                    functions[i] = new DllFunction(funcName, funcAddr, null, space.WILD_POINTER);
                }
            } else
                throw new Error("expected ciet2_version 1 or 2 but instead found " + ciet2_version);
        } else if (space.readEbcdicString(dlcbiewbcie, 8).equals("@@DL370$")) {
            long dlloffexpfunc = DllcsectTemplate.getDlloffexpfunc(inputStream, dlcbiewbcie);
            long dllexpfuncs = dlcbiewbcie + dlloffexpfunc;
            long dllexpfuncscount = DllexpfuncsTemplate.getDllexpfuncscount(inputStream, dllexpfuncs);
            if (dllexpfuncscount < 0 || dllexpfuncscount > 1000000) {
                log.config("impossible dllexpfuncscount: " + dllexpfuncscount);
                functions = new DllFunction[0];
                return functions;
            }
            functions = new DllFunction[(int)dllexpfuncscount];
            for (int i = 0; i < dllexpfuncscount; i++) {
                long dllexpfuncsname = DllexpfuncsTemplate.getDllexpfuncsname(inputStream, dllexpfuncs);
                long dllexpfuncsaddr = DllexpfuncsTemplate.getDllexpfuncsaddr(inputStream, dllexpfuncs);
                String funcName = space.readEbcdicString(dlcbiewbcie + dllexpfuncsname);
                functions[i] = new DllFunction(funcName, dllexpfuncsaddr, null, getWsa());
                // XXX this is naughty - we should handle arrays properly
                dllexpfuncs += DllexpfuncsTemplate.getDllexpfuncsarray$length() / 8;
            }
        } else {
            throw new Error("tbc");
        }
        return functions;
    }

    /** 
     * From the given AddressSpace returns the named {@link com.ibm.dtfj.corereaders.zos.le.DllFunction}
     * (or null if it can't be found). This caches previously found functions for speed.
     * Note that this searches all the Dlls and returns the first match it finds so take care
     * if multiple Dlls declare the same name!
     * @param space the AddressSpace to search
     * @param name the name of the required function
     */
    public static DllFunction getFunction(AddressSpace space, String name) throws IOException {
        DllFunction function = (DllFunction)space.getUserMap().get(name);
        if (function != null)
            return function;
        Edb edb = Edb.getSampleEdb(space);
        if (edb == null)
            return null;
        for (Dll dll = edb.getFirstDll(); dll != null; dll = dll.getNext()) {
            function = dll.getFunction(name);
            if (function != null) {
                space.getUserMap().put(name, function);
                return function;
            }
        }
        return null;
    }

    /**
     * Add the given {@link com.ibm.dtfj.corereaders.zos.le.DllFunction} to the set of functions for this
     * AddressSpace. This can be used to add fake entries, eg see {@link com.ibm.dtfj.corereaders.zos.le.FunctionEmulator#recordCalledFunctions}
     */
    public static void addFunction(AddressSpace space, String name, DllFunction function) {
        space.getUserMap().put(name, function);
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    public String toString() {
        try {
            return getName();
        } catch (IOException e) {
            return "oops: " + e;
        }
    }
}
