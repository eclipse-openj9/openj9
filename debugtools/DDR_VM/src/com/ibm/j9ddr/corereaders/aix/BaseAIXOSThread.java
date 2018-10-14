/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.aix;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.OSStackFrame;

/**
 * Abstract IOSThread for AIX. Encapsulates native stack walking logic.
 * 
 * @author andhall
 *
 */
public abstract class BaseAIXOSThread implements IOSThread
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	protected final IProcess process;
	private List<IOSStackFrame> frames;

	protected BaseAIXOSThread(IProcess process)
	{
		this.process = process;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.osthread.IOSThread#getStackFrames()
	 */
	public List<? extends IOSStackFrame> getStackFrames()
	{
		if (frames == null) {
			walkStack();
		}
		
		return Collections.unmodifiableList(frames);
	}

	private void walkStack()
	{
		frames = new LinkedList<IOSStackFrame>();
		long stackPointer = getStackPointer();
		long instructionPointer = getInstructionPointer();
		if (0 == instructionPointer || !isValidAddress(instructionPointer)) {
			instructionPointer = getBasePointer();
		}
		try {
			if (0 != instructionPointer && 0 != stackPointer &&
					isValidAddress(instructionPointer) && isValidAddress(stackPointer)) {
				frames.add(new OSStackFrame(stackPointer,instructionPointer));
				
				long previousStackPointer = -1;
				final long stepping = process.bytesPerPointer();
				
				int loops = 0; 
				// maximum 256 frames, for protection against excessive or infinite loops in corrupt dumps
				while (previousStackPointer != stackPointer && loops++ < 256) {
					previousStackPointer = stackPointer;
					stackPointer = process.getPointerAt(stackPointer);
					long addressToRead = stackPointer + stepping;
					addressToRead += stepping;//readAddress(); // Ignore conditionRegister
					instructionPointer = process.getPointerAt(addressToRead);
					frames.add(new OSStackFrame(stackPointer,instructionPointer));
				}
			} else {
				//This is a temporary hack until we can understand signal handler stacks
				//better
				logger.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.aix.BaseAIXOSThread","walkStack","MISSED");
				//TODO handle this case sensibly
			}
		} catch (CorruptDataException e) {
			// TODO handle
		}

	}

	private boolean isValidAddress(long address)
	{
		try {
			process.getByteAt(address);
			return true;
		} catch (MemoryFault e) {
			return false;
		}
	}

	public abstract long getStackPointer();

	public abstract long getInstructionPointer();

	public abstract long getBasePointer();
}
