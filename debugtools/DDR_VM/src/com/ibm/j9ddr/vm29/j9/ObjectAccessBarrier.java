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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.MMObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.I32;

public final class ObjectAccessBarrier
{
	protected static final MMObjectAccessBarrier mmObjectAccessBarrier;

	/* Do not instantiate. Static behaviour only. */
	private ObjectAccessBarrier() 
	{
	}

	static 
	{
		MMObjectAccessBarrier objectAccessBarrier = null; 
		try {
			objectAccessBarrier = MMObjectAccessBarrier.from();
		} catch (CorruptDataException cde) {
			raiseCorruptDataEvent("Error initializing the object access barrier", cde, true);
			objectAccessBarrier = null;
		}
		mmObjectAccessBarrier = objectAccessBarrier;
	}
	
	/**
	 * Determine the basic hash code for the specified object. 
	 * 
	 * @param object[in] the object to be hashed
	 * @return the persistent, basic hash code for the object 
	 * @throws CorruptDataException 
	 */
	public static I32 getObjectHashCode(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getObjectHashCode(object);
	}

	/**
	 * Fetch the finalize link field of object.
	 * @param object[in] the object to read
	 * @return the value stored in the object's finalizeLink field
	 */
	public static J9ObjectPointer getFinalizeLink(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getFinalizeLink(object);
	}
	
	/**
	 * Fetch the ownableSynchronizer link field of object.
	 * @param object[in] the object to read
	 * @return the value stored in the object's ownableSynchronizer field
	 */
	public static J9ObjectPointer getOwnableSynchronizerLink(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getOwnableSynchronizerLink(object);
	}

	public static J9ObjectPointer isObjectInOwnableSynchronizerList(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.isObjectInOwnableSynchronizerList(object);
	}

	/**
	 * Fetch the reference link field of the specified reference object.
	 * @param object the object to read
	 * @return the value stored in the object's reference link field
	 */
	public static J9ObjectPointer getReferenceLink(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getReferenceLink(object);
	}

	/**
	 * Return the lockword for the given object, or NULL if it does not have a lockword.
	 * 
	 * @param object the object who's lockword we are after
	 * 
	 * @return An J9ObjectMonitorPointer representing NULL, a flatlock, or an inflated monitor.
	 */
	public static J9ObjectMonitorPointer getLockword(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getLockword(object);
	}
	
	/**
	 * Return an object representing the monitor for the given object, or NULL if it does not have a lockword/monitor.
	 */
	public static ObjectMonitor getMonitor(J9ObjectPointer object) throws CorruptDataException
	{
		return mmObjectAccessBarrier.getMonitor(object);
	}

	/**
	 * Converts token (e.g. compressed pointer value) into real heap pointer.
	 * @return the heap pointer value.
	 */
	public static J9ObjectPointer convertPointerFromToken(long token)
	{
		return mmObjectAccessBarrier.convertPointerFromToken(token);
	}
	
	/**
	 * Converts token (e.g. compressed pointer value) into real heap pointer.
	 * @return the heap pointer value.
	 */
	public static J9ObjectPointer convertPointerFromToken(int token)
	{
		// Zero extend int to long before using
		return convertPointerFromToken((long)token & 0xFFFFFFFFL);
	}
	
	/**
	 * Converts real heap pointer into token (e.g. compressed pointer value).
	 * @return the compressed pointer value.
	 * 
	 * @note this function is not virtual because it must be callable from out-of-process
	 */
	public static long convertTokenFromPointer(J9ObjectPointer pointer)
	{
		return mmObjectAccessBarrier.convertTokenFromPointer(pointer);
	}
}
