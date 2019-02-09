/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.stackwalker.BaseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.FrameCallbackResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.ScanFormatter.formatPointer;

class CheckVMThreadStacks extends Check
{
	@Override
	public void check()
	{
		try {
			GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
			while(vmThreadListIterator.hasNext()) {
				J9VMThreadPointer walkThread = vmThreadListIterator.next();
				
				//GC_VMThreadStackSlotIterator::scanSlots(toScanWalkThread, toScanWalkThread, (void *)&localData, checkStackSlotIterator, false, false);
				WalkState walkState = new WalkState();
				walkState.walkThread = walkThread;
				walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK | J9_STACKWALK_SKIP_INLINES;

				walkState.callBacks = new BaseStackWalkerCallbacks()
				{
					public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackAddress)
					{
						_engine.checkSlotStack(objectSlot, walkThread, stackAddress);
					}
				};
				
				StackWalker.walkStackFrames(walkState);
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "THREAD STACKS";
	}

	@Override
	public void print()
	{
		try {
			GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
			final ScanFormatter formatter = new ScanFormatter(this, "thread stacks");
			while(vmThreadListIterator.hasNext()) {
				J9VMThreadPointer walkThread = vmThreadListIterator.next();
				formatter.section("thread slots", walkThread);
				
				WalkState walkState = new WalkState();
				walkState.walkThread = walkThread;
				walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK | J9_STACKWALK_SKIP_INLINES;

				walkState.callBacks = new BaseStackWalkerCallbacks()
				{
					public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackAddress)
					{
						try {
							formatter.entry(objectSlot.at(0));
						} catch (CorruptDataException e) {}
					}
				};
				
				StackWalker.walkStackFrames(walkState);
				formatter.endSection();
				
				formatter.section("thread stack", walkThread);
				dumpStackTrace(walkThread);
				formatter.endSection();
			}
			formatter.end("thread stacks");
			
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	/* TODO : this is an implementation of the internal VM function dumpStackTrace
	 * and really should be somewhere global and public */
	private void dumpStackTrace(J9VMThreadPointer walkThread)
	{
		WalkState walkState = new WalkState();
		walkState.walkThread = walkThread;
		walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;

		walkState.callBacks = new BaseStackWalkerCallbacks()
		{
			public FrameCallbackResult frameWalkFunction(J9VMThreadPointer walkThread, WalkState walkState)
			{
				String className = "(unknown class)";
				if(walkState.constantPool.notNull()) {
					try {
						className = J9ClassHelper.getName(walkState.constantPool.ramClass());
					} catch (CorruptDataException e) {
						// TODO Auto-generated catch block
					}
				}
				
				if(walkState.method.isNull()) {
					getReporter().println(String.format("0x%08X %s (unknown method)", walkState.pc.getAddress(), className));
				} else {
					if(walkState.jitInfo.isNull()) {
						boolean isNative = false;
						U8Pointer bytecodes = U8Pointer.NULL;
						String name = "(corrupt)";
						String sig = "(corrupt)";
						
						try {
							J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(walkState.method);
							isNative = romMethod.modifiers().allBitsIn(J9AccNative);
							if(!isNative) {
								bytecodes = J9ROMMethodHelper.bytecodes(romMethod);
							}
							name = J9ROMMethodHelper.getName(romMethod);
							sig = J9ROMMethodHelper.getSignature(romMethod);
						} catch (CorruptDataException e) {
							// This should never happen
						}
						
						if(isNative) {
							getReporter().println(String.format(" NATIVE   %s.%s%s", 
									className, 
									name, 
									sig));	
						} else {
							getReporter().println(String.format(" %08X %s.%s%s", 
									walkState.pc.sub(bytecodes).longValue(),
									className, 
									name, 
									sig));
						}
					} else {
						boolean isInlined = walkState.inlineDepth != 0; 
						U8Pointer jitPC = walkState.pc;
						String name = "(corrupt)";
						String sig = "(corrupt)";
						try {
							J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(walkState.method);
							name = J9UTF8Helper.stringValue(romMethod.nameAndSignature().name());
							sig = J9UTF8Helper.stringValue(romMethod.nameAndSignature().signature());
							if(!isInlined) {
								jitPC = U8Pointer.cast(jitPC.sub(U8Pointer.cast(walkState.method.extra())).longValue());
							}
						} catch (CorruptDataException e) {
							// This should never happen
						}
						
						if(isInlined) {
							getReporter().println(String.format(" INLINED  %s.%s%s  (@%s)", 
									className, 
									name, 
									sig, 
									formatPointer(walkState.pc)));	
						} else {
							getReporter().println(String.format(" %08X %s.%s%s  (@%s)", 
									jitPC.getAddress(), 
									className, 
									name, 
									sig, 
									formatPointer(walkState.pc)));	
						}
					}
				}
				
				return FrameCallbackResult.KEEP_ITERATING; 
			}
		};
		
		StackWalker.walkStackFrames(walkState);
	}

}
