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

import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;


public abstract class Pool<StructType extends DataType> 
{
	protected J9PoolPointer pool;	
	protected Class<StructType> structType;
	protected long elementSize;
	protected static final Logger logger = Logger.getLogger(LoggerNames.LOGGER_WALKERS_POOL);
	
	/* Do not instantiate. Use the factory */
	@SuppressWarnings("unchecked")
	protected <T extends DataType> Pool(J9PoolPointer structure, Class<T> structType) throws CorruptDataException 
	{
		pool = structure;
		this.structType = (Class<StructType>)structType;
		if(pool.notNull()) {
			elementSize = structure.elementSize().longValue();
		}
	}

	/**
	 * Factory method to construct an appropriate pool handler.
	 * @param <T>
	 * 
	 * @param structure the J9Pool structure to use
	 * 
	 * @return an instance of Pool 
	 * @throws CorruptDataException 
	 */
	public static <T extends DataType> Pool<T> fromJ9Pool(J9PoolPointer structure, Class<T> structType) throws CorruptDataException
	{
		return fromJ9Pool(structure, structType, true);
	}
	
	public static <T extends DataType> Pool<T> fromJ9Pool(J9PoolPointer structure, Class<T> structType, boolean isInline) throws CorruptDataException
	{
		switch(AlgorithmVersion.getVMMinorVersion()) {
			default :
				logger.fine("Creating version 2.6.0 pool walker");
				return new Pool_29_V0<T>(structure, structType, isInline);
		}
	}
	
	/**
	 *	Returns the number of elements in a given pool.
	 *
	 * @return the number of elements in the pool
	 */
	public abstract long numElements();

	/**
	 * Returns the total capacity of a pool
	 *
	 * @return the capacity of the the pool
	 */
	public abstract long capacity();
	
	/**
	 * See if an element is currently allocated from a pool.
	 *
	 * @param[in] pool The pool to check
	 * @param[in] anElement The element to check
	 *
	 * @return boolean - true if the element is allocated, false otherwise
	 */
	public abstract boolean includesElement(StructType anElement);

	/**
	 * Returns an iterator over the elements in the pool
	 * @return an Iterator 
	 */
	public abstract SlotIterator<StructType> iterator();
		
}
