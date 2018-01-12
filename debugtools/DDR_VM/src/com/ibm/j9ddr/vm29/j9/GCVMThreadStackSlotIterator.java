/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_SKIP_INLINES;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_VISIBLE_ONLY;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.stackwalker.IStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class GCVMThreadStackSlotIterator 
{
	static void scanSlots(J9VMThreadPointer walkThread, IStackWalkerCallbacks stackWalkerCallbacks, boolean includeStackFrameClassReferences, boolean trackVisibleFrameDepth)
	{
		WalkState walkState = new WalkState();
		walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK;
		walkState.walkThread = walkThread;
		walkState.callBacks = stackWalkerCallbacks;
		
		if (trackVisibleFrameDepth) {
			walkState.skipCount = 0;
			walkState.flags |= J9_STACKWALK_VISIBLE_ONLY;
		} else {
			walkState.flags |= J9_STACKWALK_SKIP_INLINES;
		}
		
		if (includeStackFrameClassReferences) {
			walkState.flags |= J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;
		}

		StackWalkResult result = StackWalkResult.STACK_CORRUPT;
		result = StackWalker.walkStackFrames(walkState);
		
		if (StackWalkResult.NONE != result) {
			/* Explicitly raising a corrupt data event here since returning StackWalkResult.STACK_CORRUPT is fairly common if a core is not taken at a safe gc point */
			CorruptDataException corruptDataException = new CorruptDataException("Stack walk returned: " + result + " for vmThread: " + walkThread.getHexAddress());
			EventManager.raiseCorruptDataEvent("", corruptDataException, false);
		}
	}

}
