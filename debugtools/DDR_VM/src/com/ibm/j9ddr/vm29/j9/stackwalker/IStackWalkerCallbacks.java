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
package com.ibm.j9ddr.vm29.j9.stackwalker;

import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

/**
 * Interface for stack-walker callback routines.
 * 
 * @author andhall
 *
 */
public interface IStackWalkerCallbacks
{

	public FrameCallbackResult frameWalkFunction(J9VMThreadPointer walkThread, WalkState walkState);

	public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackLocation);
	
	/**
	 * This callback doesn't exist in the native C.
	 * 
	 * It's purpose in DDR is passing back field slots in stack-allocated objects (which would be incorrectly handled by a PointerPointer)
	 */
	public void fieldSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, ObjectReferencePointer objectSlot, VoidPointer stackLocation);
}
