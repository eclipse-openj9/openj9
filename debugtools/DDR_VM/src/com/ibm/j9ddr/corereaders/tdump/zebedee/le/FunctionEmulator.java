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

package com.ibm.j9ddr.corereaders.tdump.zebedee.le;

import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.*;
import com.ibm.j9ddr.corereaders.tdump.zebedee.util.*;

import java.util.*;
import java.io.*;
import java.util.logging.*;

/**
 * This class acts as a more convenient way of using the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.Emulator} class.
 * Because it is in the LE package it adds the following benefits:
 * <ul>
 * <li>it knows the XPLINK calling conventions
 * <li>sets up the stack automatically
 * <li>can obtain a function address and environment from a function name
 * <li>provides tracing including function entry and exit
 * </ul>
 */

public class FunctionEmulator {

    /** The AddressSpace we all live in */
    private AddressSpace space;
    /** The emulator itself */
    private Emulator em;
    /** The entry point of the function we are calling */
    private long functionEntryPoint;
    /** This is the set of addresses to ignore during function tracing */
    private BitSet ignoreFunctionTrace = new BitSet();
    /** The MutableAddressSpace that the emulator uses */
    private MutableAddressSpace mspace;
    /** The arguments to the function */
    private int[] args = new int[3];
    /** Return value from the function */
    private long returnValue;
    /** Bottom of the allocated stack */
    private long stackBase;
    /** Print instruction offsets when tracing */
    private boolean printOffsets;
    /** Maps function entry points to return addresses */
    //private IntegerMap returnAddresses = new IntegerMap();
    /** The root of the call tree if capturing was requested */
    private Function callTreeRoot;
    /** The current function while capturing the call tree */
    private Function currentFunction;
    /** Record trace entries rather than printing them */
    private boolean recordTrace;
    /** List of recorded trace entries */
    private ArrayList traceEntries;
    private HashMap replacedFunctions = new HashMap();
    private ObjectMap enteredFunctions = new ObjectMap();
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /**
     * Create a new FunctionEmulator.
     * @param space the AddressSpace containing the code and data we are to use
     * @param functionName the name of the function to call
     * @throws NoSuchMethodException if the given function can't be found
     */
    public FunctionEmulator(AddressSpace space, String functionName) throws IOException, NoSuchMethodException {
        /* Get the address of the function */
        DllFunction function = Dll.getFunction(space, functionName);
        if (function == null)
            throw new NoSuchMethodException("cannot find function: " + functionName);
        log.fine("creating emulator for function " + functionName + " address " + hex(function.address) + " env " + hex(function.env));
        createEmulator(space, function.address, function.env);
        if ((space.readInt(function.address) & 0xff00f000) != 0x90004000 || (space.readInt(function.address + 4) & 0xffff8000) != 0xa74a8000) {
            /* Doesn't look like an XPLINK start so check for non-XPLINK */
            if ((space.readInt(function.address) & 0xfffff000) == 0x47f0f000) {
                /* Non-XPLINK, so set things up appropriately */
                em.setRegister(15, (int)function.address);
                /* These stack values are fairly arbitrary */
                em.setRegister(13, stackBase + 0x1000);
                assert !space.is64bit();
                mspace.writeInt(em.getRegister(13) + 0x4c, (int)stackBase + 0x2000);
                em.setRegister(4, 0xdeadbeef);
                em.setRegister(14, em.getRegister(7));      /* Return address */
                em.setRegister(7, 0xdeadbeef);
            }
        }
    }

    /**
     * Create a new FunctionEmulator. This is a raw form where the address of the function entry
     * point and env are supplied. Note that the so-called "environment" pointer is not required
     * by all functions. It is the value that is stored in R5. If you do need to know it you
     * can sometimes find it by looking in the DSA of the function to find the saved value
     * (assuming the dump has the function in one of the stacks!).
     * @param space the AddressSpace containing the code and data we are to use
     * @param functionEntryPoint the address of the function entry point
     * @param functionEnv the address of the "environment" for the function (not all
     * functions require this to be set)
     * @throws NoSuchMethodException if the given function can't be found
     */
    public FunctionEmulator(AddressSpace space, long functionEntryPoint, long functionEnv) throws IOException, NoSuchMethodException {
        createEmulator(space, functionEntryPoint, functionEnv);
    }

    /**
     * Do the job of creating the emulator.
     * @param functionEntryPoint the address of the function entry point
     * @param functionEnv the address of the "environment" for the function (not all
     * functions require this to be set)
     */
    void createEmulator(AddressSpace space, long functionEntryPoint, long functionEnv) throws IOException {
        this.functionEntryPoint = functionEntryPoint;
        this.space = space;

        /* Create the emulator */
        try {
            em = (Emulator)Class.forName("com.ibm.zebedee.emulator.EmulatorImpl").newInstance();
        } catch (Exception e) {
            throw new Error("unable to instantiate com.ibm.zebedee.emulator.EmulatorImpl");
        }

        /* Create a stack and make R4 point to it (XPLINK convention) */
        mspace = new MutableAddressSpace(space);
        int r4len = 0x10000;
        stackBase = mspace.malloc(r4len);
        em.setRegister(4, stackBase + r4len - 0x1000);

        /* R5 is expected to point at the so-called "environment" for the function */
        em.setRegister(5, functionEnv);

        em.setRegister(6, functionEntryPoint);

        createDummyReturnAddress();
    }

    /**
     * Return the xplink stack floor. Be careful with this.
     */
    public long getStackFloor() {
        return stackBase;
    }

    /**
     * Register a dummy return address. When this is branched to we simply
     * stop the emulator
     */
    public long createDummyReturnAddress() throws IOException {
        long returnAddress = mspace.rmalloc(1);
        em.setRegister(7, returnAddress);
        Emulator.CallbackFunction returnCallback = new Emulator.CallbackFunction() {
            public boolean call(Emulator em) {
                em.stop();
                /* Set the return value */
                returnValue = em.getRegister(3);
                //System.out.println("createDummyReturnAddress, stopped emulator");
                return false;
            }
        };
        em.registerCallbackFunction(returnAddress, returnCallback);
        em.registerCallbackFunction(returnAddress + 2, returnCallback);
        em.registerCallbackFunction(returnAddress + 4, returnCallback);

        return returnAddress;
    }

    /**
     * Include instruction offsets from the start of the function when tracing.
     * XXX doesn't yet work for called functions
     */
    public void setPrintOffsets(boolean printOffsets) {
        this.printOffsets = printOffsets;
    }

    /**
     * Turn instruction tracing on or off. A trace of all instructions executed will be
     * sent to System.out.
     */
    public void setInstructionTracing(boolean instructionTrace) {
        em.setTraceListener(instructionTrace ? new TraceListener() : null);
    }

    /**
     * Turn instruction recording on or off. A trace of all instructions executed will be
     * saved.
     */
    public void setInstructionRecording(boolean instructionRecord) {
        em.setTraceListener(instructionRecord ? new TraceListener() : null);
        if (instructionRecord) {
            recordTrace = true;
            traceEntries = new ArrayList();
        } else {
            recordTrace = false;
        }
    }

    /** Return a list of the instruction trace entries */
    public List getTraceEntries() {
        return traceEntries;
    }

    /**
     * Turn branch instruction tracing on or off. A trace of all branch instructions executed will be
     * sent to System.out.
     */
    public void setBranchInstructionTracing(boolean branchInstructionTrace) {
        em.setBranchTraceListener(branchInstructionTrace ? new BranchTraceListener() : null);
    }

    /**
     * Turn call tree capturing on. The call tree may be retrieved with {@link getCallTreeRoot}.
     */
    public void setCallTreeCapture() {
        callTreeRoot = new Function("root", null);
        currentFunction = callTreeRoot;
        em.setBranchTraceListener(new BranchTraceListener() {
            protected void enterFunction(String name, long targetAddress) {
                currentFunction = currentFunction.addChild(name);
            }
            protected void exitFunction(String name) {
                currentFunction = currentFunction.getParent();
                if (currentFunction == null) {
                    assert false : "shouldn't happen in theory";
                    currentFunction = callTreeRoot;
                }
            }
        });
    }

    /**
     * Returns the root of the call tree. {@link setCallTreeCapture} must have been called
     * prior to the emulator having run. These routines can be used to obtain the actual call
     * tree during a run of the emulator. Note that the root function is a dummy one.
     */
    public Function getCallTreeRoot() {
        return callTreeRoot;
    }

    /**
     * Returns the current function. User callbacks can make use of this.
     */
    public Function getCurrentFunction() {
        return currentFunction;
    }

    /**
     * This class represents a called function. It's child functions may be obtained with
     * {@link getChildren}.
     */
    public class Function {
        private String name;
        private Function parent;
        private ArrayList children = new ArrayList();
        private HashMap childMap = new HashMap();

        Function(String name, Function parent) {
            this.name = name;
            this.parent = parent;
        }

        /** @return the function that called this function */
        public Function getParent() {
            return parent;
        }

        /** 
         * @return the functions called by this function in the order in which they were
         * first called (duplicates are discarded).
         */
        public Iterator getChildren() {
            return children.iterator();
        }

        /** @return the name of this function */
        public String getName() {
            return name;
        }

        /** 
         * Add child if not already present
         * @return the child function
         */
        public Function addChild(String childName) {
            Function child = (Function)childMap.get(childName);
            if (child == null) {
                child = new Function(childName, this);
                children.add(child);
                childMap.put(childName, child);
            }
            return child;
        }
    }

    /** Describes a trace entry */
    public class TraceEntry {
        public long address;
        public long offset;
        public String instruction;
        public String opcode;
        public String args;
        public String result = "";

        TraceEntry(long address, long offset, String instruction) {
            this.instruction = instruction;
            this.address = address;
            this.offset = offset;
            int i = instruction.indexOf(' ');
            if (i != -1) {
                opcode = instruction.substring(0, i);
                while (i < instruction.length() && instruction.charAt(i) == ' ') i++;
                if (i < instruction.length()) {
                    args = instruction.substring(i);
                } else {
                    args = "";
                }
            } else {
                opcode = instruction;
                args = "";
            }
        }

        public String toString() {
            String off = printOffsets ? " + " + hexpad(offset).substring(5) : "";
            return hexpad(address) + off + ": " + instruction + " " + result;
        }
    }

    /**
     * Returns the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.Emulator} object we use.
     */
    public Emulator getEmulator() {
        return em;
    }

    /**
     * Sets the given function argument.
     * @param argNumber the number of the argument to be set, starting at one (XXX currently limited to a max of 3)
     * @param value the value of the argument
     */
    public void setArgument(int argNumber, int value) {
        if (argNumber < 4) {
            em.setRegister(argNumber, value);
        } else {
            try {
                /* XXX only works for XPLINK! */
                mspace.writeInt(em.getRegister(4) + 0x840 + ((argNumber - 1) << 2), value);
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * Sets the given function argument.
     * @param argNumber the number of the argument to be set, starting at one (XXX currently limited to a max of 3)
     * @param value the value of the argument
     */
    public void setArgument(int argNumber, long value) {
        if (argNumber < 4) {
            em.setRegister(argNumber, value);
        } else {
            try {
                /* XXX only works for XPLINK! */
                if (mspace.is64bit())
                    mspace.writeLong(em.getRegister(4) + 0x880 + ((argNumber - 1) << 3), value);
                else
                    mspace.writeInt(em.getRegister(4) + 0x840 + ((argNumber - 1) << 2), (int)value);
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * Gets the given function argument. To be used from callback functions.
     * @param argNumber the number of the argument to be got, starting at one
     */
    public long getArgument(int argNumber) {
        if (argNumber < 4) {
            return em.getRegister(argNumber);
        } else {
            try {
                /* XXX only works for XPLINK! */
                if (mspace.is64bit())
                    return mspace.readLong(em.getRegister(4) + 0x880 + ((argNumber - 1) << 3));
                else
                    return mspace.readInt(em.getRegister(4) + 0x840 + ((argNumber - 1) << 2));
            } catch (IOException e) {
                throw new Error("oops: " + e);
            }
        }
    }

    /**
     * Returns the MutableAddressSpace used by the emulator. This can be used by users wishing
     * to allocate space for input arguments for instance.
     */
    public MutableAddressSpace getMutableAddressSpace() {
        return mspace;
    }

    /**
     * Registers a callback function.
     * @return the address of the linkage pointer. This is what you put in place of a
     * C function pointer.
     */
    public long registerCallbackFunction(final CallbackFunction callback) {
        try {
            /* First we allocate a fake entry point. We don't actually use this space, it's
             * just guaranteed to be unused by anything else */
            long ep = mspace.malloc(1);
            /* Now set up the DLL linkage for this entry point */
            long linkage = mspace.malloc(32);
            if (mspace.is64bit()) {
                mspace.writeLong(linkage + 8, ep);
            } else {
                mspace.writeInt(linkage + 20, (int)ep);
            }
            /* Finally we tell the emulator to call us back when it encounters our
             * fake entry point address */
            em.registerCallbackFunction(ep, new Emulator.CallbackFunction() {
                public boolean call(Emulator em) throws IOException {
                    try {
                        long ret = callback.call(FunctionEmulator.this);
                        /* Set the return value */
                        em.setRegister(3, ret);
                        return false;
                    } catch (CallOriginalException e) {
                        assert false;
                        return true;
                    }
                }
            });
            /* Add the entry point to the set of addresses to ignore when tracing */
            ignoreFunctionTrace.set((int)ep & 0x7fffffff);
            /* Return the linkage pointer to the user. This is what they store as their
             * function pointer. */
            return linkage;
        } catch (IOException e) {
            throw new Error("oops: " + e);
        }
    }

    /**
     * Allows the given function to be "overridden". The supplied callback function will
     * be called rather than the real one.
     * @param functionName the name of the function to override
     * @param callback the user function that will be called rather than the real one
     */
    public void overrideFunction(String functionName, CallbackFunction callback) throws IOException, NoSuchMethodException {
        /* Get the address of the function */
        DllFunction function = Dll.getFunction(space, functionName);
        if (function == null)
            throw new NoSuchMethodException("cannot find function: " + functionName);
        overrideFunction(function.address, callback);
    }

    /**
     * Allows the given function to be "overridden". The supplied callback function will
     * be called rather than the real one. This is the raw version of overrideFunction
     * that takes the address of the function.
     * @param functionEntryPoint the address of the function entry point
     * @param callback the user function that will be called rather than the real one
     */
    public void overrideFunction(long functionEntryPoint, final CallbackFunction callback) throws IOException, NoSuchMethodException {
        /* Make the emulator call us instead when it sees this address */
        em.registerCallbackFunction(functionEntryPoint, new Emulator.CallbackFunction() {
            public boolean call(Emulator em) throws IOException {
                try {
                    long ret = callback.call(FunctionEmulator.this);
                    /* Set the return value */
                    em.setRegister(3, ret);
                    return false;
                } catch (CallOriginalException e) {
                    /* The callee would like us to run the original instruction */
                    return true;
                }
            }
        });
    }

//boolean stuff;
    class Replace {
        long retVal;
        boolean protect;

        Replace(long retVal, boolean protect) {
            this.retVal = retVal;
            this.protect = protect;
        }
    }

    public void overrideFunction(String functionName, long retVal, boolean protect) {
                    //System.out.println("override " + functionName + " protect is " + protect);
        replacedFunctions.put(functionName, new Replace(retVal, protect));
        em.setFunctionReplacer(new Emulator.FunctionReplacer() {
            public boolean replace(Emulator em, long targetAddress) throws IOException {
                //if (!stuff) setInstructionTracing(false);
                String functionName = (String)enteredFunctions.get(targetAddress);
                if (functionName == null) {
                    functionName = DsaStackFrame.getEntryPointName(space, stripTopBit(targetAddress));
                    enteredFunctions.put(targetAddress, functionName);
                }
                Replace ret = (Replace)replacedFunctions.get(functionName);
                if (ret != null) {
                    //System.out.println("found override " + functionName + " protect is " + ret.protect);
                    if (ret.protect) {
                        try {
                            //stuff = true;
                            //setInstructionTracing(true);
                            em.saveState();
                            //System.out.println("about to run...");
                            long returnAddress = createDummyReturnAddress();
                            em.setRegister(7, returnAddress);
                            em.run(mspace, targetAddress);
                            //System.out.println("...finished run");
                            long r = em.getRegister(3);
                            em.restoreState();
                            em.setRegister(3, r);
                        } catch (IOException e) {
                            log.logp(Level.WARNING, "com.ibm.j9ddr.corereaders.tdump.zebedee.le.FunctionEmulator.overrideFunction(...).FunctionReplacer","replace", "Unexpected Exception",e);
                            em.restoreState();
                            em.setRegister(3, ret.retVal);
                        }
                        em.resume();
                        //stuff = false;
                        return true;
                    }
                    em.setRegister(3, ret.retVal);
                    return true;
                }
                return false;
            }
        });
    }

    public void overrideFunction(String functionName, long returnValue) {
        overrideFunction(functionName, returnValue, false);
    }

    private long stripTopBit(long address) {
        return mspace.is64bit() ? address : address & 0x7fffffff;
    }

    /**
     * Now run the emulator. This method returns when the emulated function returns. 
     * @return the return value from the function (if any)
     */
    public long run() throws IOException {
        em.run(mspace, functionEntryPoint);
        return returnValue;
    }

    /**
     * Run the emulator using the supplied xplink C function pointer. This method
     * returns when the emulated function returns. 
     * @return the return value from the function (if any)
     */
    public long run(long functionPointer) throws IOException {
        long ep;
        long env;
        if (mspace.is64bit()) {
            ep = mspace.readLong(functionPointer + 8);
            env = mspace.readLong(functionPointer + 0);
        } else {
            ep = mspace.readInt(functionPointer + 20);
            env = mspace.readInt(functionPointer + 16);
        }

        // XXX note: overlap here with createEmulator

        /* R5 is expected to point at the so-called "environment" for the function */
        em.setRegister(5, env);

        //long returnAddress = returnAddresses.get(functionPointer);
        //if (returnAddress == -1) {
            long returnAddress = createDummyReturnAddress();
            //returnAddresses.put(functionPointer, returnAddress);
        //}
        em.setRegister(7, returnAddress);

        /* XXX because this method was written with recursive use in mind (ie making this
         * call from within a callback from the emulator) we need to save and restore any
         * existing entry point */
        long oldEp = functionEntryPoint;
        functionEntryPoint = ep;
        em.run(mspace, functionEntryPoint);
        functionEntryPoint = oldEp;
        mspace.free(returnAddress);

        return returnValue;
    }

    /**
     * Set the xplink environment register.
     */
    private void setEnvReg(long address) {
        em.setRegister(5, address);

        /* Now this is a bit weird. Appears you can have holes in the area pointed to
         * by the first word. If so, fill 'em in. Need to revisit this! */
        try {
            long ptr = mspace.readInt(address);
            ptr &= ~(Dump.DATABLOCKSIZE - 1);
            for (int i = 0; i < 3; i++, ptr += Dump.DATABLOCKSIZE) {
                try {
                    mspace.readInt(ptr);
                } catch (IOException ee) {
                    /* Found a hole - fill it in */
                    mspace.allocPage(ptr);
                    log.fine("Filled in hole at " + hex(ptr));
                }
            }
        } catch (IOException e) {
            log.fine("Cannot read env: " + hex(address));
        }
    }

    /**
     * Run the emulator using the supplied xplink C function. This method
     * returns when the emulated function returns. 
     * @return the return value from the function (if any)
     */
     // XXX tidy up this mess of different ways of starting/running
    public long run(String functionName) throws IOException, NoSuchMethodException {
        /* Get the address of the function */
        DllFunction function = Dll.getFunction(space, functionName);
        if (function == null)
            throw new NoSuchMethodException("cannot find function: " + functionName);

        // XXX note: overlap here with createEmulator

        /* R5 is expected to point at the so-called "environment" for the function */
        setEnvReg(function.env);

        createDummyReturnAddress();

        em.run(mspace, function.address);

        return returnValue;
    }

    /**
     * Run the emulator and record any called functions as new {@link com.ibm.j9ddr.corereaders.tdump.zebedee.le.DllFunction}s.
     * The purpose of this method is to obtain the entry points for functions which are not
     * in the export list for a DLL. The emulator is run and any functions called are saved
     * as fake DllFunction entries which may then be obtained using
     * {@link com.ibm.j9ddr.corereaders.tdump.zebedee.le.Dll#getFunction}.
     */
    public void recordCalledFunctions() throws IOException {
        em.setBranchTraceListener(new BranchTraceListener() {
            protected void enterFunction(String name, long targetAddress) {
                long r5 = getEmulator().getRegister(5);
                DllFunction function = new DllFunction(name, targetAddress, null, r5);
                Dll.addFunction(space, name, function);
                log.fine("Adding " + name);
            }
            protected void exitFunction(String name) {}
        });
        runAllPaths();
    }

    // same as above but run for real
    public void recordAllCalledFunctions() throws IOException {
        em.setBranchTraceListener(new BranchTraceListener() {
            protected void enterFunction(String name, long targetAddress) {
                long r5 = getEmulator().getRegister(5);
                DllFunction function = new DllFunction(name, targetAddress, null, r5);
                Dll.addFunction(space, name, function);
                log.fine("Adding " + name);
            }
            protected void exitFunction(String name) {}
        });
        run();
    }

    /**
     * Run all paths of the function. This is used for disassembly.
     */
    public void runAllPaths() throws IOException {
        em.runAllPaths(mspace, functionEntryPoint);
    }

    /**
     * This exception can be thrown by callback functions which want to call the overridden
     * function.
     */
    public static class CallOriginalException extends Exception {
    }

    /**
     * This interface is similar to Emulator.CallbackFunction except that it adds the ability
     * to set the return value from the function (which is XPLINK specific).
     */
    public interface CallbackFunction {
        /**
         * This method will be called when the emulator finds a call to the registered
         * function entry point (see {@link #registerCallbackFunction}). The return value
         * from this method represents the return value that the emulator sees after it
         * has made the call. If the called method throws CallOriginalException then
         * execution will resume with the original overridden function.
         */
        public long call(FunctionEmulator em) throws IOException, CallOriginalException;
    }

    /**
     * Convenience class to do emulator tracing if necessary. This traces all instructions.
     */
    private class TraceListener extends BranchTraceListener implements Emulator.TraceListener {

        TraceEntry entry;

        public void trace(Emulator.Instruction inst) throws IOException {
            entry = new TraceEntry(inst.address(), inst.address() - functionEntryPoint, inst.toString());
        }

        void recordOrPrint(TraceEntry entry) {
            if (recordTrace) {
                traceEntries.add(entry);
            } else {
                log.fine(entry.toString());
            }
        }

        protected void enterFunction(String name, long targetAddress) {
            if (!name.equals("(unknown)"))
                entry.result += " (enter " + name + ")";
        }

        protected void exitFunction(String name) {
            if (!name.equals("(unknown)"))
                entry.result += " (exit " + name + ")";
        }

        public void trace(Emulator em, Emulator.Instruction inst) throws IOException {
            trace(inst);
            recordOrPrint(entry);
        }

        public void trace(Emulator em, Emulator.Instruction inst, int[] result) throws IOException {
            trace(inst);
            for (int i = 0; i < result.length; i++)
                entry.result += hexpad(result[i]) + " ";
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, long[] result) throws IOException {
            trace(inst);
            for (int i = 0; i < result.length; i++)
                entry.result += hexpad(result[i]) + " ";
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, String result) throws IOException {
            trace(inst);
            entry.result = result;
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, int result) throws IOException {
            trace(inst);
            entry.result = hexpad(result);
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, double result) throws IOException {
            trace(inst);
            entry.result = "" + result;
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, int arg1, int arg2) throws IOException {
            trace(inst);
            entry.result = hexpad(arg1) + " " + hexpad(arg2);
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, double arg1, double arg2) throws IOException {
            trace(inst);
            entry.result = "" + arg1 + " " + arg2;
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, long result) throws IOException {
            trace(inst);
            entry.result = hexpad(result);
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, long arg1, long arg2) throws IOException {
            trace(inst);
            entry.result = hexpad(arg1) + " " + hexpad(arg2);
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, int result, long address) throws IOException {
            trace(inst);
            entry.result = hexpad(result) + " " + hexpad(address);
            recordOrPrint(entry);
        }
        public void trace(Emulator em, Emulator.Instruction inst, double result, long address) throws IOException {
            trace(inst);
            entry.result = "" + result + " " + hexpad(address);
            recordOrPrint(entry);
        }
        public void traceBranchAndSave(Emulator em, Emulator.Instruction inst, long returnAddress, long targetAddress) throws IOException {
            trace(inst);
            entry.result = hexpad(targetAddress);
            try {
                doTraceBranchAndSave(em, inst, returnAddress, targetAddress);
            } catch (IOException e) {}
            recordOrPrint(entry);
        }
        public void traceBranchOnCondition(Emulator em, Emulator.Instruction inst, boolean branched, long targetAddress) throws IOException {
            trace(inst);
            entry.result = hexpad(targetAddress);
            try {
                doTraceBranchOnCondition(em, inst, branched, targetAddress);
            } catch (IOException e) {}
            recordOrPrint(entry);
        }
        public void traceBranchOnCount(Emulator em, Emulator.Instruction inst, int count) throws IOException {
            trace(inst);
            entry.result = "" + count;
            recordOrPrint(entry);
        }
    }

    /**
     * Class to do tracing of branch instructions.
     */
    private class BranchTraceListener implements Emulator.BranchTraceListener {
        ObjectMap map = new ObjectMap();
        long lastTargetAddress;
        long lastReturnAddress;
        int indent;

        protected void enterFunction(String name, long targetAddress) {
            System.out.println("[" + indent + "] enter " + name);
            indent++;
        }

        protected void exitFunction(String name) {
            indent--;
            System.out.println("[" + indent + "] exit " + name);
        }

        public void traceBranchAndSave(Emulator em, Emulator.Instruction inst, long returnAddress, long targetAddress) throws IOException {
            doTraceBranchAndSave(em, inst, returnAddress, targetAddress);
        }

        void doTraceBranchAndSave(Emulator em, Emulator.Instruction inst, long returnAddress, long targetAddress) throws IOException {
            targetAddress &= 0x7fffffff;
            returnAddress &= 0x7fffffff;
            if (targetAddress == lastReturnAddress) {
                /* Using something like BALR $r1, $r14 to return from a function */
                doTraceBranchOnCondition(em, inst, true, targetAddress);
                lastReturnAddress = 0;
            } else if (!ignoreFunctionTrace.get((int)targetAddress)) {
                String functionName = DsaStackFrame.getEntryPointName(space, targetAddress);
                if (inst.id() == 0xa75) {
                    Emulator.BranchRelative brinst = (Emulator.BranchRelative)inst;
                    if (brinst.getOffset() > 0 && brinst.getOffset() < 32 && functionName.equals("(unknown)")) {
                        /* Should clean this up. This is a BRAS (branch relative and save) which is
                         * probably being used internally just to skip over a data word or something,
                         * so don't trace it. */
                        return;
                    }
                }
                map.put(returnAddress, functionName);
                enterFunction(functionName, targetAddress);
            }
            lastReturnAddress = returnAddress;
            lastTargetAddress = 0;
        }

        public void traceBranchOnCondition(Emulator em, Emulator.Instruction inst, boolean branched, long targetAddress) throws IOException {
            doTraceBranchOnCondition(em, inst, branched, targetAddress);
        }

        void doTraceBranchOnCondition(Emulator em, Emulator.Instruction inst, boolean branched, long targetAddress) throws IOException {
            /* Ignore branch relative because they are not used to return */
            if (inst.id() == 0xa74) {
                return;
            }
            targetAddress &= 0x7fffffff;
            /* This code is made a little messy because a branch can either target the
             * the return address directly or the return address + 4. Immediately following
             * the return address is a NOP which is also a branch instruction. */
            String functionName = (String)map.get(targetAddress - 4);
            if (functionName == null) {
                functionName = (String)map.get(targetAddress);
            }
            /* Appears you can jump to return address + 2 in 64-bit mode */
            if (functionName == null) {
                functionName = (String)map.get(targetAddress - 2);
            }
            if (functionName != null && targetAddress != lastTargetAddress + 4) {
                exitFunction(functionName);
            }
            lastTargetAddress = targetAddress;
        }
    }

    static String hex(long i) {
        return Long.toHexString(i);
    }

    static String hexpad(long i) {
        String s = hex(i);
        while (s.length() < 8)
            s = "0" + s;
        return s;
    }

    static String hex(int i) {
        return Integer.toHexString(i);
    }

    static String hexpad(int i) {
        String s = hex(i);
        while (s.length() < 8)
            s = "0" + s;
        return s;
    }
}
