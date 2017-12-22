/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;
import java.util.logging.Level;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPuddleListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPuddlePointer;

public class Pool_29_V0 <StructType extends DataType> extends Pool<StructType> implements SlotIterator<StructType> {
	private static int POOLSTATE_FOLLOW_NEXT_POINTERS = 1;
	private final boolean isInline;
	private final State state = new State();
	private int slot = 0;
	private VoidPointer nextItem = null;
	
	StructType nextStruct = null;
	boolean inited = false;
	
	protected <T extends DataType> Pool_29_V0(J9PoolPointer structure, Class<T> structType, boolean isInline) throws CorruptDataException 
	{
		super(structure, structType);
		this.isInline = isInline;
	}

	@Override
	public boolean includesElement(StructType anElement) {
		throw new RuntimeException("Unimplemented");
	}

	@Override
	public SlotIterator<StructType> iterator() {

		return this;
	}
	
	@SuppressWarnings("unchecked")
	public boolean hasNext() {

		// This will be nulled when consumed by a call to next() or nextAddress()
		// prevents multiple calls to hasNext() walking the pool further.
		if(nextStruct != null) {
			return true;
		}

		try {
			// We have to do initialization in hasNext() so we don't raise corrupt data before
			// any handlers are likely to have been installed.
			if( !inited ) {
				nextItem = pool_startDo();										//start the pool walk
			} else {
				nextItem = pool_nextDo();										//continue the pool walk
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error creating iterator", e, true);		//cannot try to recover from this
			nextItem = null;												// make the pool look empty
		} finally {
			inited = true;													// make sure we don't do this again
		}
		while( (nextItem != null) && nextItem.notNull() ) {
			try {
				if(!isInline) {
					PointerPointer ptr = PointerPointer.cast(nextItem);
					nextItem = ptr.at(0);
				}
				nextStruct = (StructType)DataType.getStructure(structType, nextItem.getAddress());
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next pool item", e, false);		//may be able to recover from this
			}
			if( nextStruct != null ) {
				break; // Found an item, move on.
			} else {
				// If a CDE was thrown trying to create nextStruct we will have raised it and can move to
				// the next item in the pool.
				try {
					nextItem = pool_nextDo();										//continue the pool walk
				} catch (CorruptDataException e) {
					raiseCorruptDataEvent("Error creating iterator", e, true);		//cannot try to recover from this
					nextItem = null;												// make the pool look empty
				}
			}
		}
		return nextStruct != null;
	}


	public StructType next() {
		if( hasNext() ) {
			StructType next = nextStruct;
			nextStruct = null;
			return next;
		}
		throw new NoSuchElementException();
	}

	public VoidPointer nextAddress() {
		if(hasNext()) {
			// Invalidate the next element so hasNext() will walk on next call.
			nextStruct = null;
			return nextItem;
		}
		throw new NoSuchElementException();
	}
	
	public void remove() {
		throw new UnsupportedOperationException();
	}

	@Override
	public long numElements() {
		try {
			return J9PoolPuddleListPointer.cast(pool.puddleList()).numElements().longValue();
		} catch (CorruptDataException e) {
			return 0;
		}
	}
	
	@Override
	public long capacity() {
		long numElements=0;
		
		try {
			J9PoolPuddleListPointer puddleList = J9PoolPuddleListPointer.cast(pool.puddleList());
			J9PoolPuddlePointer walk = puddleList.nextPuddle();
			while(!walk.isNull()) {
				numElements += pool.elementsPerPuddle().longValue();
				walk = walk.nextPuddle();
			}
		} catch (CorruptDataException e) {
			return 0;
		}		
		return numElements;
	}
	
	private VoidPointer pool_startDo() throws CorruptDataException {
		slot = 0;
		J9PoolPuddleListPointer puddleList = J9PoolPuddleListPointer.cast(pool.puddleList());
		return poolPuddle_startDo(puddleList.nextPuddle(), true);
	}
	
	private VoidPointer poolPuddle_startDo(J9PoolPuddlePointer currentPuddle, boolean followNextPointers) throws CorruptDataException {
		UDATAPointer currAddr = null;
		
		if(pool.isNull() || currentPuddle.isNull()) {
			return null;
		}
		if(currentPuddle.usedElements().longValue() == 0) {
			if((currentPuddle.nextPuddle().notNull()) && followNextPointers) {
				return poolPuddle_startDo(currentPuddle.nextPuddle(), followNextPointers);
			} else {
				return null;
			}
		}
		while(isPuddleSlotFree(currentPuddle)) {
			slot++;
		}
		currAddr = UDATAPointer.cast(currentPuddle.firstElementAddress().getAddress() + (elementSize * slot));
		state.currentPuddle = currentPuddle;
		state.lastSlot = slot;
		state.leftToDo = currentPuddle.usedElements().intValue() - 1;
		state.flags = 0;
		if(followNextPointers) {
			state.flags |= POOLSTATE_FOLLOW_NEXT_POINTERS;			// TODO find out where this is set
		}
		if(state.leftToDo == 0) {
			if(followNextPointers) {
				state.currentPuddle = state.currentPuddle.nextPuddle();
				state.lastSlot = -1;
			} else {
				state.currentPuddle = null;
			}
		}
		logger.fine(String.format("Next pool item 0x%016x", currAddr.getAddress()));
		if(logger.isLoggable(Level.FINER)) {
			logger.finer(state.toString());
		}
		return VoidPointer.cast(currAddr);
	}
	
	//PUDDLE_SLOT_FREE
	private boolean isPuddleSlotFree(J9PoolPuddlePointer currentPuddle) throws CorruptDataException {
		U32Pointer ptr = U32Pointer.cast(currentPuddle.add(1));
		ptr = ptr.add(slot / 32);
		long value = ptr.at(0).longValue();
		int bitmask = (1 << (31 - slot % 32));
		return (value & bitmask) != 0;
	}
	
	//re-uses the existing state
	private VoidPointer pool_nextDo() throws CorruptDataException {
		slot = 1 + state.lastSlot;
		UDATAPointer currAddr = null;
		
		if(state.leftToDo == 0) {
			if((state.currentPuddle != null) && (state.currentPuddle.notNull())) {
				return poolPuddle_startDo(state.currentPuddle, true);
			} else {
				return null;
			}
		}
		
		while(isPuddleSlotFree(state.currentPuddle)) {
			slot++;
		}
		currAddr = UDATAPointer.cast(state.currentPuddle.firstElementAddress().getAddress() + (elementSize * slot));
		state.lastSlot = slot;
		state.leftToDo--;
		
		if(state.leftToDo == 0) {
			if((state.flags & POOLSTATE_FOLLOW_NEXT_POINTERS) == POOLSTATE_FOLLOW_NEXT_POINTERS) {
				state.currentPuddle = state.currentPuddle.nextPuddle();
				state.lastSlot = -1;
			} else {
				state.currentPuddle = null;
			}
		}
		logger.fine(String.format("Next pool item 0x%016x", currAddr.getAddress()));
		if(logger.isLoggable(Level.FINER)) {
			logger.finer(state.toString());
		}
		return VoidPointer.cast(currAddr);
	}
	
	private class State {
		J9PoolPuddlePointer currentPuddle = null;
		int lastSlot = 0;
		int leftToDo = 0;
		int flags = 0;
		
		@Override
		public String toString() {
			StringBuilder builder = new StringBuilder();
			builder.append("Pool walker state\n\tCurrent Puddle : 0x");
			builder.append(Long.toHexString(currentPuddle.getAddress()));
			builder.append("\n\tLast Slot : ");
			builder.append(lastSlot);
			builder.append("\n\tLeft to do : ");
			builder.append(leftToDo);
			builder.append("\n\tFlags : ");
			builder.append(flags);
			return builder.toString();
		}
		
		
	}
}
