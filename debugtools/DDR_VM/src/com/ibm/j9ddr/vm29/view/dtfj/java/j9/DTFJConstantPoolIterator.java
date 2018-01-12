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
package com.ibm.j9ddr.vm29.view.dtfj.java.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRCorruptData;
import com.ibm.j9ddr.vm29.j9.gc.GCConstantPoolClassSlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCConstantPoolObjectSlotIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaObject;

//provides a DTFJ wrapper around the values returned from the J9 model

@SuppressWarnings("unchecked")
public class DTFJConstantPoolIterator implements Iterator, IEventListener {
	private GCConstantPoolObjectSlotIterator poolObjects;
	private GCConstantPoolClassSlotIterator poolClasses;
	private J9DDRCorruptData cdata = null;
	private boolean EOIObjects = true;			//flag for End Of Iterator
	private boolean EOIClasses = true;			//flag for End Of Iterator
	
	public DTFJConstantPoolIterator(J9ClassPointer ptr) {
		try {
			register(this);		//register this class for notifications	
			poolObjects = GCConstantPoolObjectSlotIterator.fromJ9Class(ptr);
			poolClasses = GCConstantPoolClassSlotIterator.fromJ9Class(ptr);
			EOIObjects = !poolObjects.hasNext();
			EOIClasses = !poolClasses.hasNext();
		} catch (CorruptDataException e) {
			corruptData("Unable to instantiate ConstantPool iterator", e, true);
		} finally {
			unregister(this);	//remove notifier registration
		}
	}

	public boolean hasNext() {
		return !(EOIObjects && EOIClasses) || (cdata != null);
	}

	public Object next() {
		if(hasNext()) {
			Object retval = null;
			if(!EOIObjects) {		//if there are some more items to retrieve from the pool
				try {
					register(this);		//register this class for notifications	
					retval = new DTFJJavaObject(poolObjects.next());	//get the next item
					EOIObjects = !poolObjects.hasNext();						//see if there are any more
				} finally {
					unregister(this);	//remove notifier registration
				}
				return retval;
			}
			if(!EOIClasses) {		//if there are some more items to retrieve from the pool
				try {
					register(this);		//register this class for notifications	
					try {
						//DTFJ returns the java.lang.Class object associated with this class
						retval = new DTFJJavaObject(poolClasses.next().classObject());	//get the next item
					} catch (CorruptDataException e) {
						retval = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);					//return the corrupt data
					}
					EOIClasses = !poolClasses.hasNext();						//see if there are any more
				} finally {
					unregister(this);	//remove notifier registration
				}
				return retval;
			}
			if(cdata != null) {		//there are no more items in the pool
				retval = cdata;		//but there may be corrupt data items to return
				cdata = null;
			}
			return retval;
		}
		throw new NoSuchElementException("There are no more elements in this iterator");
	}

	public void remove() {
		throw new UnsupportedOperationException("The image is read only and cannot be modified.");
	}

	/* Record the corrupt data and serve after the current item has been processed
	 * (non-Javadoc)
	 * @see com.ibm.j9ddr.events.IEventListener#corruptData(java.lang.String, com.ibm.j9ddr.CorruptDataException, boolean)
	 */
	public void corruptData(String message, CorruptDataException e, boolean fatal) {
		EOIObjects = true;
		EOIClasses = true;
		cdata = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);
	}

	
	
}
