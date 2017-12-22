/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_SKIP_INLINES;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;

import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.stackwalker.FrameCallbackResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.j9.stackwalker.IStackWalkerCallbacks;

public class StackRoots 
{

	private ArrayList<J9ObjectPointer> _allStackRoots = new ArrayList<J9ObjectPointer>();
	private ArrayList<VoidPointer> _allAddresses = new ArrayList<VoidPointer>();
	private static StackRoots _singleton;

	private class StackWalkerCallbacks implements IStackWalkerCallbacks
	{
		public FrameCallbackResult frameWalkFunction(J9VMThreadPointer walkThread, WalkState walkState)
		{
			return FrameCallbackResult.KEEP_ITERATING;
		}
	
		public void objectSlotWalkFunction(J9VMThreadPointer walkThread, WalkState walkState, PointerPointer objectSlot, VoidPointer stackAddress)
		{
			if (walkState.method.isNull()){
				/* adding an object slot iterator causes us to be called for
				 * xxx methods. These were previously ignored, since the frame
				 *does not have a valid method. We should continue to do so now.
				 */
				return;
			}
			
			try {
				J9ObjectPointer object = J9ObjectPointer.cast(objectSlot.at(0));
				if (object.notNull()) {
					_allStackRoots.add(object);
					_allAddresses.add(VoidPointer.cast(objectSlot));
				}
			} catch (CorruptDataException e) {
				throw new UnsupportedOperationException("Corrupt objectSlot detected");
			}
		}
		
	
		public void fieldSlotWalkFunction(J9VMThreadPointer walkThread,
				WalkState walkState, ObjectReferencePointer objectSlot,
				VoidPointer stackLocation)
		{
			if (walkState.method.isNull()){
				/* adding an object slot iterator causes us to be called for
				 * xxx methods. These were previously ignored, since the frame
				 *does not have a valid method. We should continue to do so now.
				 */
				return;
			}
			
			try {
				J9ObjectPointer object = objectSlot.at(0);
				if (object.notNull()) {
					_allStackRoots.add(object);
					_allAddresses.add(VoidPointer.cast(objectSlot));
				}
			} catch (CorruptDataException e) {
				throw new UnsupportedOperationException("Corrupt objectSlot detected");
			}
	
		}
	
	}
	
	private StackRoots() throws CorruptDataException
	{
		_allStackRoots = new ArrayList<J9ObjectPointer>();
		_allAddresses = new ArrayList<VoidPointer>();
		walkStacks();
	}
	
	public static StackRoots from() throws CorruptDataException 
	{
		if (null != _singleton) {
			return _singleton;
		}

		_singleton = new StackRoots();
		return _singleton;
	}

	
	private void walkStacks() throws CorruptDataException
	{		
		GCVMThreadListIterator threadIterator = GCVMThreadListIterator.from();

		while (threadIterator.hasNext()) {
			J9VMThreadPointer next = threadIterator.next();
			
			WalkState walkState = new WalkState();
			walkState.walkThread = next;
			walkState.flags = J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS;
			

			walkState.callBacks = new StackWalkerCallbacks(); 		
			StackWalkResult result = StackWalkResult.STACK_CORRUPT;
			result = StackWalker.walkStackFrames(walkState);
			
			if (StackWalkResult.NONE != result) {
				throw new UnsupportedOperationException("Failed to walk stack");
			}						
		}
	}
	
	public static ArrayList<J9ObjectPointer> allRoots() throws CorruptDataException	
	{
		StackRoots stackRoots= new StackRoots();
		return stackRoots._allStackRoots;
	}
	
	public static GCIterator stackRootIterator() throws CorruptDataException
	{
		final StackRoots stackRootSet = StackRoots.from();
		final Iterator<J9ObjectPointer> rootSetIterator = stackRootSet._allStackRoots.iterator();
		final Iterator<VoidPointer> rootSetAddressIterator = stackRootSet._allAddresses.iterator();
		
		return new GCIterator() {
			public boolean hasNext() {
				return rootSetIterator.hasNext();
			}

			public VoidPointer nextAddress() {
				rootSetIterator.next();
				return rootSetAddressIterator.next();
			}

			public Object next() {
				rootSetAddressIterator.next();
				return rootSetIterator.next(); 
			}
		};
		
	}
	
}
