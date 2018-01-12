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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.GC_FinalizeListManagerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public class GCFinalizableObjectIterator extends GCIterator
{
	public final static int state_start = 0;
	public static final int state_system = 1;
	public static final int state_default = 2;
	public static final int state_reference = 3;
	public static final int state_end = 4;
	
	protected int _state = state_start;
	protected J9ObjectPointer _currentSystemObject;
	protected J9ObjectPointer _currentDefaultObject;
	protected J9ObjectPointer _currentReferenceObject;
	
	GC_FinalizeListManagerPointer _manager = null;	/* the finalize list manager */
	
	/* Do not instantiate. Use the factory */	
	protected GCFinalizableObjectIterator(GC_FinalizeListManagerPointer manager) throws CorruptDataException 
	{
		this._manager = manager;
				
		_currentSystemObject = getFirstSystemFinalizableObject();
		_currentDefaultObject = getFirstDefaultFinalizableObject();
		_currentReferenceObject = manager._referenceObjects();
	}
	
	public static GCFinalizableObjectIterator from() throws CorruptDataException
	{
		return new GCFinalizableObjectIterator(getExtensions().finalizeListManager());
	}
	
	public boolean hasNext()
	{
		if(_state == state_end) {
			return false;
		}
		
		switch(_state) {
		
		case state_start:
			_state++;
		
		case state_system:
			if(_currentSystemObject.notNull()) {
				return true;
			}
			_state++;
		
		case state_default:
			if(_currentDefaultObject.notNull()) {
				return true;
			}
			_state++;

		case state_reference:
			if(_currentReferenceObject.notNull()) {
				return true;
			}
			_state++;

		}
	
		return false;
	
	}

	public J9ObjectPointer next()
	{
		try {
			if(hasNext()) {
				J9ObjectPointer result =  J9ObjectPointer.NULL;
				switch(_state) {
				
				case state_system:
					result = _currentSystemObject;
					advanceToNextSystemFinalizableObject();
					break;
				
				case state_default:
					result = _currentDefaultObject;
					advanceToNextDefaultFinalizableObject();
					break;

				case state_reference:
					result = _currentReferenceObject;
					_currentReferenceObject = ObjectAccessBarrier.getReferenceLink(_currentReferenceObject);
					break;
				}
				return result;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch (CorruptDataException cde) {
			raiseCorruptDataEvent("Error getting next item", cde, false);		//can try to recover from this
			if(_state < state_end) {
				/* The current list appears damaged. Move to the next one. */
				_state++;
			}
			return null;
		}
	}

	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
	
	public int getState()
	{
		return _state;
	}
	
	/**
	 * Helper method used to find the first system finalizable object.
	 * @return the first system finalizable object or null if the list(s) is empty
	 */
	private J9ObjectPointer getFirstSystemFinalizableObject() throws CorruptDataException
	{
		J9ObjectPointer firstSystemObject = _manager._systemFinalizableObjects();
		
		return firstSystemObject;
	}
	
	/**
	 * Helper method used to find the first default finalizable object.
	 * @return the first default finalizable object or null if the list(s) is empty
	 */
	private J9ObjectPointer getFirstDefaultFinalizableObject() throws CorruptDataException
	{
		J9ObjectPointer firstDefaultObject = _manager._defaultFinalizableObjects();
		
		return firstDefaultObject;
	}
	
	/**
	 * Advances the _currentSystemObject to the next system
	 * finalizable object in the finalize list manager's list(s)
	 * 
	 * @throws CorruptDataException
	 */
	private void advanceToNextSystemFinalizableObject() throws CorruptDataException
	{
		_currentSystemObject = ObjectAccessBarrier.getFinalizeLink(_currentSystemObject);
		
	}
	
	/**
	 * Advances the _currentDefaultObject to the next default
	 * finalizable object in the finalize list manager's list(s)
	 * 
	 * @throws CorruptDataException
	 */
	private void advanceToNextDefaultFinalizableObject() throws CorruptDataException
	{
		_currentDefaultObject = ObjectAccessBarrier.getFinalizeLink(_currentDefaultObject);
		
	}
}
