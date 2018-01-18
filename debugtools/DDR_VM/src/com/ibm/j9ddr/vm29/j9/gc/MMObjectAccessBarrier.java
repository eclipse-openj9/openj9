/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.I32;

public abstract class MMObjectAccessBarrier extends GCBase
{
	/* Do not instantiate. Use the factory */
	protected MMObjectAccessBarrier() 
	{
	}

	/**
	 * Factory method to construct an appropriate object model.
	 * @param structure the J9JavaVM structure to use
	 * 
	 * @return an instance of ObjectModel 
	 * @throws CorruptDataException 
	 */
	public static MMObjectAccessBarrier from() throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.MM_OBJECT_ACCESS_BARRIER_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new MMObjectAccessBarrier_V1();
		}
	}

	/**
	 * Determine the basic hash code for the specified object. 
	 * 
	 * @param object[in] the object to be hashed
	 * @return the persistent, basic hash code for the object 
	 * @throws CorruptDataException 
	 */
	public abstract I32 getObjectHashCode(J9ObjectPointer object) throws CorruptDataException;
	
	/**
	 * Fetch the finalize link field of object.
	 * @param object[in] the object to read
	 * @return the value stored in the object's finalizeLink field
	 */
	public abstract J9ObjectPointer getFinalizeLink(J9ObjectPointer object) throws CorruptDataException;

	/**
	 * Fetch the ownableSynchronizer link field of object.
	 * @param object[in] the object to read
	 * @return the value stored in the object's ownableSynchronizerLink field
	 */
	public abstract J9ObjectPointer getOwnableSynchronizerLink(J9ObjectPointer object) throws CorruptDataException;

 	/**
	 * check if the object in one of OwnableSynchronizerLists
	 * @param object[in] the object pointer
	 * @return the value stored in the object's reference link field
	 * 		   if reference link field == J9ObjectPointer.NULL, it means the object isn't in the list
	 */
	public abstract J9ObjectPointer isObjectInOwnableSynchronizerList(J9ObjectPointer object) throws CorruptDataException;
	
	/**
	 * Fetch the reference link field of the specified reference object.
	 * @param object the object to read
	 * @return the value stored in the object's reference link field
	 */
	public abstract J9ObjectPointer getReferenceLink(J9ObjectPointer object) throws CorruptDataException;
	
	/**
	 * Return the lockword for the given object, or NULL if it does not have a lockword.
	 * This may return NULL, a flatlock, or an inflated monitor.
	 */
	public abstract J9ObjectMonitorPointer getLockword(J9ObjectPointer object) throws CorruptDataException;

	/**
	 * Return an object representing the monitor for the given object, or NULL if it does not have a lockword/monitor.
	 */
	public abstract ObjectMonitor getMonitor(J9ObjectPointer object) throws CorruptDataException;

	/**
	 * Converts token (e.g. compressed pointer value) into real heap pointer.
	 * @return the heap pointer value.
	 */
	public abstract J9ObjectPointer convertPointerFromToken(long token);
	
	/**
	 * Converts real heap pointer into token (e.g. compressed pointer value).
	 * @return the compressed pointer value.
	 * 
	 * @note this function is not virtual because it must be callable from out-of-process
	 */
	public abstract long convertTokenFromPointer(J9ObjectPointer pointer);
}
