/*
 * Copyright IBM Corp. and others 2009
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

import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;

/**
 * Base implementation of IStackWalkerCallbacks that does performs no-ops 
 * for each callback.
 * 
 * Provided to make writing implementations of IStackWalkerCallbacks easier
 * 
 * @author andhall
 *
 */
public class BaseStackWalkerCallbacks implements IStackWalkerCallbacks
{

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm.j9.stackwalker.IStackWalkerCallbacks#frameWalkFunction(com.ibm.j9ddr.vm.j9.stackwalker.WalkState)
	 */
	@Override
	public FrameCallbackResult frameWalkFunction(WalkState walkState)
	{
		// Deliberately do nothing
		return FrameCallbackResult.KEEP_ITERATING;
	}

	@Override
	public void objectSlotWalkFunction(WalkState walkState, PointerPointer objectSlot,
			VoidPointer stackLocation)
	{
		// Deliberately do nothing
	}

	@Override
	public void fieldSlotWalkFunction(WalkState walkState, ObjectReferencePointer objectSlot,
			VoidPointer stackLocation)
	{
		// Deliberately do nothing
		
	}

}
