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

package com.ibm.j9ddr.vm29.j9.walkers;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;

public class ArrayIterator<StructType extends DataType>  implements Iterator<StructType> {
	private final int total;
	private int current = 0;
	private PointerPointer node = null;
	protected Class<StructType> structType;
	private long address = 0;
	
	@SuppressWarnings("unchecked")
	public <T extends DataType> ArrayIterator(Class<T> structType, int total, PointerPointer nodes) throws CorruptDataException {
		this.structType = (Class<StructType>)structType;
		this.total = total;
		if(total > 0) {
			node = nodes;		//set the first node pointer
			address = node.at(0).getAddress();
			setNextItem();
		}
	}

	public Iterator<StructType> iterator() {
		return this;
	}
	
	public boolean hasNext() {
		return current < total;
	}

	@SuppressWarnings("unchecked")
	public StructType next() {
		if(hasNext()) {
			try {
				StructType ptr = (StructType)DataType.getStructure(structType, address);
				address = 0;
				setNextItem();
				return ptr;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, true);		//cannot try to recover from this
				return null;
			}
		}
		throw new NoSuchElementException();
	}

	private void setNextItem() throws CorruptDataException {
		while((current < total) && (address == 0)) {
			node = node.add(1);
			current++;
			address = node.at(0).getAddress();
		}
	}
	
	public void remove() {
		throw new UnsupportedOperationException();
	}
	
}
