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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

import java.io.IOException;

/**
 * This interface represents a zSeries emulator.
 */

public interface Emulator {

    /**
     * This interface represents a mutable address space. It is used by the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.Emulator}
     * to read and write values. It is similar to <a href="http://java.sun.com/j2se/1.4.2/docs/api/javax/imageio/stream/ImageOutputStreamImpl.html">ImageOutputStreamImpl</a>
     * except that it adds the ability to allocate an area of memory.
     */
    public interface ImageSpace {
        /**
         * Read a byte at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public int readUnsignedByte(long address) throws IOException;

        /**
         * Read an unsigned short at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public int readUnsignedShort(long address) throws IOException;

        /**
         * Read a short at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public short readShort(long address) throws IOException;

        /**
         * Read an int at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public int readInt(long address) throws IOException;

        /**
         * Read an unsigned int at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public long readUnsignedInt(long address) throws IOException;

        /**
         * Read a long at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public long readLong(long address) throws IOException;

        /**
         * Write a byte at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public void writeByte(long address, int value) throws IOException;

        /**
         * Write a short at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public void writeShort(long address, int value) throws IOException;

        /**
         * Write an int at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public void writeInt(long address, int value) throws IOException;

        /**
         * Write a long at the specified address.
         * @throws IOException if the given address is not present in this address space
         */
        public void writeLong(long address, long value) throws IOException;

        /**
         * Allocate a chunk of unused memory. This searches the known address ranges and tries
         * to find an unallocated area of the given size. If successful, this area effectively
         * becomes part of this ImageSpace and may be read and written to as normal.
         * This method is used by users of the Emulator for things like allocating
         * stack, arguments etc.
         * @return the address of the allocated memory
         */
        public long malloc(int size) throws IOException;

        /**
         * Return true if this dump is from a 64-bit machine
         */
        public boolean is64bit();
    }

    /**
     * This interface is implemented by users who want to receive trace events. One of its
     * methods will be called for every instruction executed. These methods are called after
     * the emulator machine state has been updated, so if for instance you want to see the
     * resulting condition code you could call the emulator {@link #getConditionCode} method.
     */
    public interface TraceListener extends BranchTraceListener {

        /**
         * Trace an instruction such as compare that has no word result (but may set 
         * condition codes or fiddle with storage in some way).
         */
        void trace(Emulator em, Instruction inst) throws IOException;

        /**
         * This method is called for unknown instructions. 
         */
        void trace(Emulator em, Instruction inst, String detail) throws IOException;

        /**
         * This method is called for RR format instructions. The result contains the
         * result of the instruction execution.
         */
        void trace(Emulator em, Instruction inst, int result) throws IOException;

        /**
         * This method is called for floating point RR format instructions. The result contains the
         * result of the instruction execution.
         */
        void trace(Emulator em, Instruction inst, double result) throws IOException;

        /**
         * This method is called for instructions such as compare that involve two values.
         */
        void trace(Emulator em, Instruction inst, int arg1, int arg2) throws IOException;

        /**
         * This method is called for instructions such as floating point compare that involve two values.
         */
        void trace(Emulator em, Instruction inst, double arg1, double arg2) throws IOException;

        /**
         * This method is called for RRE format instructions. The result contains the
         * result of the instruction execution.
         */
        void trace(Emulator em, Instruction inst, long result) throws IOException;

        /**
         * This method is called for instructions such as compare that involve two values.
         */
        void trace(Emulator em, Instruction inst, long arg1, long arg2) throws IOException;

        /**
         * This method is called for instructions such as load multiple that modify a
         * number of things.
         */
        void trace(Emulator em, Instruction inst, int[] result) throws IOException;

        /**
         * This method is called for instructions such as load multiple that modify a
         * number of things.
         */
        void trace(Emulator em, Instruction inst, long[] result) throws IOException;

        /**
         * This method is called for RX format instructions. The result contains the
         * result of the instruction execution. Also includes the operand address.
         */
        void trace(Emulator em, Instruction inst, int result, long address) throws IOException;

        /**
         * This method is called for floating point RX format instructions. The result contains the
         * result of the instruction execution. Also includes the operand address.
         */
        void trace(Emulator em, Instruction inst, double result, long address) throws IOException;

        /**
         * Trace a branch on count instruction.
         * @param count the value of the count after it has been decremented
         */
        void traceBranchOnCount(Emulator em, Instruction inst, int count) throws IOException;
    }

    /**
     * This interface is implemented by users who want to receive branch trace events. It is
     * basically a subset of {@link TraceListener} that only deals with instructions that
     * involve branching. It is provided as a separate interface for performance reasons so
     * that if a user only wants to trace function entry/exit for instance, they won't get
     * a trace callback for every single instruction.
     */
    public interface BranchTraceListener {

        /**
         * Trace a branch and save instruction (either BASR, BAS, BAL, BALR or BRAS).
         * @param returnAddress the address to return to
         * @param targetAddress the entry point we are branching to
         */
        void traceBranchAndSave(Emulator em, Instruction inst, long returnAddress, long targetAddress) throws IOException;

        /**
         * Trace a branch on condition instruction.
         * @param branched true if the branch was taken
         * @param targetAddress the address we are branching to
         */
        void traceBranchOnCondition(Emulator em, Instruction inst, boolean branched, long targetAddress) throws IOException;
    }

    public interface FunctionReplacer {
        boolean replace(Emulator em, long targetAddress) throws IOException;
    }

    /**
     * Call back interface used by Emulator to call user registered function.
     */
    public interface CallbackFunction {
        /** Call registered function. Returns true if the original (overridden) instruction
         * should be run */
        boolean call(Emulator em) throws IOException;
    }

    /**
     * Register a callback function for the given address. When the emulator encounters
     * a branch to this address it will call the given callback instruction rather than
     * the branch to the address itself. For a branch and save this results in the entire
     * target subroutine being skipped with the supplied function being called in its place.
     * Upon return, execution continues at the current instruction address plus 4 (for a 
     * branch and save - for a simple branch it is assumed that this is only used for
     * exit from the emulator).
     */
    public void registerCallbackFunction(long address, CallbackFunction function);

    public void saveState() ;

    public void restoreState();

    /**
     * Stop the emulator.
     */
    public void stop();

    /**
     * Resume the emulator.
     */
    public void resume();

    /**
     * Sets a TraceListener
     * @param traceListener used to inform the user of each instruction as it is executed. If
     * null then no tracing is performed.
     */
    public void setTraceListener(TraceListener traceListener);

    /**
     * Allow SVC calls (which are just ignored). The default is to throw an IOException
     * when they are encountered.
     */
    public void setAllowSVC(boolean allowSVC);

    /**
     * Sets a BranchTraceListener
     * @param branchTraceListener used to inform the user of each branch instruction as it is executed. If
     * null then no tracing is performed.
     */
    public void setBranchTraceListener(BranchTraceListener branchTraceListener);
    public void setFunctionReplacer(FunctionReplacer functionReplacer);

    /**
     * Turns stores into loads. Special convenience mode for disassembly.
     */
    public void setInvertStores(boolean invertStores);

    /**
     * Sets whether the emulator should ignore exceptions caused by bad memory accesses. This
     * can be useful when using the emulator as a disassembler.
     */
    public void setIgnoreExceptions(boolean ignoreExceptions);

    /**
     * Sets whether the emulator should allow the unimplemented (so far) "Set Address Space
     * Control" instruction to do nothing. If not set the emulator will throw an unrecognized
     * instruction exception.
     */
    public void setAllowSAC(boolean allowSAC);

    /**
     * Sets whether the emulator should fork on conditional branches. This puts the emulator in
     * a special mode where it will execute all possible paths. A given path is only executed
     * once. Useful when using the emulator as a disassembler.
     * XXX Note that callback functions will still be called to enable return from a function.
     */
    public void setForkOnBranches(boolean forkOnBranches);

    /**
     * Run the emulator starting at the given address.
     * @param space an instance of {@link ImageSpace}.
     * @param address the address of the first instruction to execute. Normally this would be at
     * the start of a function.
     */
    public void run(ImageSpace space, long address) throws IOException;

    /**
     * Run all possible paths in the emulator starting at the given address. (For disassembler
     * use). This also sets ignoreExceptions and forkOnBranches.
     * @param space an instance of {@link ImageSpace}.
     * @param address the address of the first instruction to execute. Normally this would be at
     * the start of a function.
     */
    public void runAllPaths(ImageSpace space, long address) throws IOException;

    /**
     * Returns the current value of the given register.
     */
    public long getRegister(int reg);

    /**
     * Sets the register to the given int value.
     */
    public void setRegister(int reg, int value);

    /**
     * Sets the register to the given long value.
     */
    public void setRegister(int reg, long value);

    /**
     * Sets the four-bit PSW key.
     */
    public void setPswKey(int key);

    /**
     * Returns the current condition code.
     */
    public int getConditionCode();

    /**
     * The base class of all instructions.
     */
    public interface Instruction {
        /**
         * Returns a unique id identifying this instruction. This is the opcode as
         * described in Principles Of Operation.
         */
        public int id();

        /**
         * Return the address of this instruction.
         */
        public long address();

        /**
         * Returns a description of this instruction in normal assembler listing format.
         * For convenience the string is padded to a fixed width.
         */
        public String toString();
    }
    
    /**
     * This class represents one of the branch relative family of instructions.
     */
    public interface BranchRelative {
        /**
         * Returns the offset of the instruction branched to.
         */
        public long getOffset();
    }
}
