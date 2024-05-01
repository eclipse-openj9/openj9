/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.j9.stackwalker;

import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_ROM_METHOD_FROM_RAM_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9SFStackFrame.J9SF_FRAME_TYPE_JNI_NATIVE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9SFStackFrame.J9SF_FRAME_TYPE_GENERIC_SPECIAL;
import static com.ibm.j9ddr.vm29.structure.J9SFStackFrame.J9SF_FRAME_TYPE_METHODTYPE;
import static com.ibm.j9ddr.vm29.structure.J9SFStackFrame.J9SF_MAX_SPECIAL_FRAME_TYPE;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;

public class TerseStackWalkerCallbacks implements IStackWalkerCallbacks {

	@Override
	public FrameCallbackResult frameWalkFunction(WalkState walkState)
	{

		try {
			if (walkState.method.notNull()) {
				J9MethodPointer method = walkState.method;
				J9UTF8Pointer className = ConstantPoolHelpers.J9_CLASS_FROM_METHOD(method).romClass().className();
				J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
				J9UTF8Pointer name = romMethod.nameAndSignature().name();
				J9UTF8Pointer sig = romMethod.nameAndSignature().signature();
				StackWalkerUtils.swPrintf(walkState, 0, "\t!j9method {3}   {0}.{1}{2}",
						J9UTF8Helper.stringValue(className), J9UTF8Helper.stringValue(name), J9UTF8Helper.stringValue(sig),
						walkState.method.getHexAddress());
				return FrameCallbackResult.KEEP_ITERATING;

			}

			if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_JNI_NATIVE_METHOD) {
				StackWalkerUtils.swPrintf(walkState, 0, "\t                        Native method frame");
			} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_GENERIC_SPECIAL) {
				StackWalkerUtils.swPrintf(walkState, 0, "\t                        Generic special frame");
			} else 	if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_METHODTYPE) {
				StackWalkerUtils.swPrintf(walkState, 0, "\t                        MethodType frame");
			} else {
				if (walkState.pc.getAddress() > J9SF_MAX_SPECIAL_FRAME_TYPE) {
					if (walkState.pc.getAddress() == walkState.javaVM.callInReturnPC().getAddress() ||
							walkState.pc.getAddress() == (walkState.javaVM.callInReturnPC().getAddress() + 3)) {
						StackWalkerUtils.swPrintf(walkState, 0, "\t                        JNI call-in frame");
					} else {
						StackWalkerUtils.swPrintf(walkState, 0, "\t                        unknown frame type {0} *{1}",
								walkState.pc, walkState.pc.getHexAddress());
					}
				} else {
					StackWalkerUtils.swPrintf(walkState, 0, "\t                        known but unhandled frame type {0}", 
							walkState.pc);
				}
			}

		} catch (CorruptDataException e) {
			e.printStackTrace();
		}
		
		return FrameCallbackResult.KEEP_ITERATING;
	}

	@Override
	public void objectSlotWalkFunction(WalkState walkState, PointerPointer objectSlot, VoidPointer stackLocation)
	{

	}

	@Override
	public void fieldSlotWalkFunction(WalkState walkState, ObjectReferencePointer objectSlot, VoidPointer stackLocation)
	{

	}

}
