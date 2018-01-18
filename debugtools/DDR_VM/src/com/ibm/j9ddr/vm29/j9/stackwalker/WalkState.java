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
package com.ibm.j9ddr.vm29.j9.stackwalker;

import com.ibm.j9ddr.vm29.pointer.Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9I2JStatePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITDecompilationInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMEntryLocalStoragePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.structure.J9StackWalkConstants.*;

/**
 * Mutable java equivalent of J9StackWalkState
 * 
 * @author andhall
 *
 */
public class WalkState
{
	
	/** Thread to be walked */
	public J9VMThreadPointer walkThread;
	
	/** Flags controlling the walk
	 * @see StackWalkerConstants 
	 */
	public long flags;
	
	/** Base pointer */
	public UDATAPointer bp;
	
	public UDATAPointer unwindSP;
	
	/** Program counter */
	public U8Pointer pc;
	
	/** Top-of-stack pointer */
	public UDATAPointer sp;
	
	/** Address of argument 0 */
	public UDATAPointer arg0EA;
	
	public J9MethodPointer literals;
	
	public UDATAPointer walkSP;
	
	public UDATA argCount;
	
	public J9ConstantPoolPointer constantPool;
	
	public J9MethodPointer method;
	
	public J9JITExceptionTablePointer jitInfo = J9JITExceptionTablePointer.NULL;
	
	public UDATA frameFlags;
	
	public UDATA resolveFrameFlags;
	
	public UDATAPointer searchValue;
	
	public int skipCount;
	
	public long maxFrames;
	
	/* User data excluded. If you want to add your own fields, create a subclass */
	
	public long framesWalked;
	
	public IStackWalkerCallbacks callBacks;
	
	/* Cache isn't duplicated in offline walker */
	
	public Pointer restartPoint;
	
	public Pointer restartException;
	
	public Pointer inlinerMap;
	
	public long inlineDepth;
	
	public UDATAPointer cacheCursor;
	
	public J9JITDecompilationInfoPointer decompilationRecord;
	
	public boolean searchFrameFound;
	
	public UDATAPointer registerEAs[];
	
	{
		registerEAs = new UDATAPointer[(int) J9SW_POTENTIAL_SAVED_REGISTERS];
		
		for (int i=0; i < registerEAs.length; i++) {
			registerEAs[i] = UDATAPointer.NULL;
		}
	}
	
	public J9VMEntryLocalStoragePointer walkedEntryLocalStorage;
	
	public J9I2JStatePointer i2jState;
	
	public J9JITDecompilationInfoPointer decompilationStack;
	
	public PointerPointer pcAddress;
	
	public UDATA outgoingArgCount;
	
	public U8Pointer objectSlotBitVector;
	
	public UDATA elsBitVector;
	
	public U8Pointer bytecodePCOffset;
	
	public UDATAPointer j2iFrame;
	
	public UDATA previousFrameFlags;
	
	public int slotIndex;
	
	public int slotType;
}
