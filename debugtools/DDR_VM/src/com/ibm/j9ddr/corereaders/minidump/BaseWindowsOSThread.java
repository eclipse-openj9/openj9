/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.minidump.unwind.RuntimeFunction;
import com.ibm.j9ddr.corereaders.minidump.unwind.UnwindInfo;
import com.ibm.j9ddr.corereaders.minidump.unwind.UnwindModule;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.OSStackFrame;

/**
 * Abstract class containing stack-walking logic for Windows OS threads.
 * 
 * @author andhall
 *
 */
public abstract class BaseWindowsOSThread implements IOSThread
{
	
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	private List<IOSStackFrame> stackFrames;
	
	protected final IProcess process;
	
	protected BaseWindowsOSThread(IProcess process) {
		this.process = process;
	}
	
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.osthread.IOSThread#getStackFrames()
	 */
	public List<IOSStackFrame> getStackFrames()
	{
		if (null == stackFrames) {
			stackFrames = new LinkedList<IOSStackFrame>();
	
			try {
				if( process.bytesPerPointer() == 4 ) {
					walkStack32();
				} else {
					walkStack64();
				}
			} catch (CorruptDataException ex) {
				logger.logp(Level.FINE, "com.ibm.j9ddr.corereaders.minidump.BaseWindowsOSThread", "getStackFrames", "Problem walking native stack: " + ex.getMessage()); 
			}
		}
		
		return Collections.unmodifiableList(stackFrames);
	}
	
	protected abstract long getStackStart();
	
	protected abstract long getStackEnd();

	private void walkStack32() throws CorruptDataException
	{
		// Windows stack frames can be read by following the ebp to the base of
		//the stack: old ebp is at ebp(0) and and return address to parent context
		//is ebp(sizeof(void*))
		// 1) find the ebp in the register file
		long ebp = getBasePointer();
		long eip = getInstructionPointer();
		long esp = getStackPointer();
		
		long stackStart = getStackStart();
		long stackEnd = getStackEnd();
		
		// eip may be -1 if we're in a system call
		if (-1 == eip && stackStart <= esp && esp < stackEnd) {
			try {
				eip = process.getPointerAt(esp);
			} catch (MemoryFault e) {
				//ignore
			}
		}
				
		int bytesPerPointer = process.bytesPerPointer();
				
		//Add the current frame first. If ebp doesn't point into the stack, try esp.
		if (Addresses.lessThanOrEqual(stackStart, ebp) && Addresses.lessThan(ebp,stackEnd)) {
			stackFrames.add(new OSStackFrame(ebp, eip));
		} else if (Addresses.lessThan(stackStart, esp) && Addresses.lessThan(esp, stackEnd)) {
			stackFrames.add(new OSStackFrame(esp, eip));
			ebp = esp + bytesPerPointer;
		}
				
		while (stackStart <= ebp && ebp < stackEnd) {
			try {
				long newBP = process.getPointerAt(ebp);
				long retAddress = process.getPointerAt(ebp + bytesPerPointer);
				stackFrames.add(new OSStackFrame(newBP,retAddress));
				ebp = newBP;
			} catch (MemoryFault e) {
				//stop trying to read meaningless memory
				break;
			}
		}
	}

	
	/* Walking 64 bit stacks is not like 32 bit stacks.
	 * We have to apply the unwind info contained in the dll's.
	 * 
	 * This is documented here:
	 * http://msdn.microsoft.com/en-us/library/8ydc79k6%28v=vs.110%29.aspx
	 */
	private void walkStack64() throws CorruptDataException {
		// Get the module for the current instruction pointer.
		long ip = getInstructionPointer();
		long rsp = getStackPointer();

		// System.err.println(String.format("Initial rsp = 0x%08x", rsp));
		// System.err.println(String.format("Initial ip = 0x%08x", ip));

		while( ip != 0x0) {		

			// Create a stack frame from that base pointer and instruction pointer.
			// On x86-64 the there is no base pointer.
			stackFrames.add(new OSStackFrame(rsp,ip));

			// Get the unwind info in the right module, for the current instruction pointer. (Step 1)
			UnwindModule module = getModuleForInstructionAddress(ip);
			RuntimeFunction rf = null;
			if( module != null ) {
				rf = module.getUnwindDataForAddress(ip - module.getLoadAddress());
			} else {
				break;
			}

			if( rf == null ) {
				// We failed to find any unwind data in the dump for this function so we can't unwind any further.
				// This could be a JVM assembler function and we won't find any but potentially could work
				// out what to do.
				// Also Windows XP does not have unwind data for NtWaitForSingleObject and some other core
				// functions. (Windows 7 does.)
				break;
			} else {

				// System.err.println("Found unwind data: " + rf + " for " + SymbolUtil.getProcedureNameForAddress(process, ip));
				UnwindInfo info = new UnwindInfo(process.getAddressSpace(), module, rf.getUnwindInfoAddress());
				// Uncomment to dump unwind information as we apply it.
				// System.err.println("Applying UNWIND_INFO: " + info);

				// Apply the unwind info to the stack and get the new
				// base pointer and stack pointer.
				rsp = info.apply(rsp);

				// Get the instruction/base pointer for the next frame.
				ip = process.getPointerAt(rsp);
				
				// New stack pointer is the slot after that. (I think)
				rsp += 8;
			}

			// System.err.println(String.format("Next rsp = 0x%08x", rsp));
			// System.err.println(String.format("Next ip = 0x%08x", ip));
		}
	}
	
	public UnwindModule getModuleForInstructionAddress(long address) throws CorruptDataException
	{
		Collection<? extends IModule> modules = process.getModules();
		
		IModule matchingModule = null;
		
OUTER_LOOP:	for (IModule thisModule : modules) {
			for (IMemoryRange thisRange : thisModule.getMemoryRanges()) {
				if (thisRange.contains(address)) {
					matchingModule = thisModule;
					break OUTER_LOOP;
				}
			}
		}
		
		if (matchingModule == null || !(matchingModule instanceof UnwindModule) ) {
			return null;
		}

		return (UnwindModule) matchingModule;
	}

	protected long getValueOfNamedRegister(Collection<? extends IRegister> registers,
			String string)
	{
		for (IRegister register : registers) {
			if (register.getName().equals(string)) {
				return register.getValue().longValue();
			}
		}
		return -1; //maybe this should throw
	}
	

}
