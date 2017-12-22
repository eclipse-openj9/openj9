/*******************************************************************************
 * Copyright (c) 2012, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.minidump.unwind;

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.Module;

public class UnwindModule extends Module {

	private List<RuntimeFunction> runtimeFunctionEntries;
	
	public UnwindModule(IProcess process, String name,
			List<? extends ISymbol> symbols,
			Collection<? extends IMemoryRange> memoryRanges, long loadAddress,
			Properties properties) {
		super(process, name, symbols, memoryRanges, loadAddress, properties);
	}

	/* Extra constructor that allows us to store a list of runtimeFunctionEntries.
	 * These map address ranges of code onto pointers to the unwind information
	 * required to walk the stack. 
	 */
	public UnwindModule(IProcess process, String name,
			List<? extends ISymbol> symbols,
			Collection<? extends IMemoryRange> memoryRanges, long loadAddress,
			Properties properties, List<RuntimeFunction> runtimeFunctionEntries) {
		super(process, name, symbols, memoryRanges, loadAddress, properties);
		this.runtimeFunctionEntries = runtimeFunctionEntries;
		Collections.sort(runtimeFunctionEntries);
	}
	
	/**
	 * Given an instruction address relative to this module, returns
	 * the appropriate unwind data for that address.
	 * @param address
	 * @return
	 * @throws MemoryFault 
	 */
	public RuntimeFunction getUnwindDataForAddress(long address) throws MemoryFault {
		RuntimeFunction data = null;

		for( RuntimeFunction entry : runtimeFunctionEntries ) {
			if( entry.contains(address) ) {
				data = entry;
				/* Odd numbers for UNWIND_INFO mean that it's really
				 * a pointer to another RUNTIME_FUNCTION structure
				 * with the least significant bit set to identify
				 * it.
				 */
				while( data.getUnwindInfoAddress() % 2 != 0 ) {
					// This is chained data.
					address = address - 1; // Remove LSB.
					int start = process.getIntAt(address + getLoadAddress());
					int end = process.getIntAt(address + getLoadAddress() + 4);
					int unwindAddress = process.getIntAt(address + getLoadAddress() + 4);
					data = new RuntimeFunction(start, end, unwindAddress);
				}
				break;
			}
		}
		return data;
	}
	
	/* Useful debugging function */
	public void dumpUndwindInfo(PrintStream output) {
		if( runtimeFunctionEntries == null ) {
			output.println("No undwind info available for module " + getName());
		}
		output.println("Dumping unwind info for: " + getName());
		try {
			for( RuntimeFunction rf: runtimeFunctionEntries) {
				output.println(String.format("Found ImageRuntimeFunctionEntry %s", rf));
				// Skip chained entries, they don't have real info. They are handled in the
				// get code above so their existence is transparent to everyone else.
				if( rf.getUnwindInfoAddress() % 2 == 0 ) {
					UnwindInfo info = new UnwindInfo(process.getAddressSpace(), this, rf.getUnwindInfoAddress());
					output.println(info);
					output.println();
				}
			}
		} catch (CorruptDataException e) {
			e.printStackTrace(output);
		}
	}
	
}
