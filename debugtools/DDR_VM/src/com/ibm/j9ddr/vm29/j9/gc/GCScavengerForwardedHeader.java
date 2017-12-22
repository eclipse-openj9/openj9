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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCScavengerForwardedHeader
{
	protected J9ObjectPointer objectPointer;
	
	/* Do not instantiate. Use the factory */
	protected GCScavengerForwardedHeader(J9ObjectPointer object)
	{
		objectPointer = object;
	}
	
	/**
	 * Factory method to construct an appropriate scavenger forwarded header
	 * 
	 * @param object the "J9Object" structure to view as a ScavengerForwardedHeader 
	 * 
	 * @return an instance of GCScavengerForwardedHeader 
	 * @throws CorruptDataException 
	 */
	public static GCScavengerForwardedHeader fromJ9Object(J9ObjectPointer object) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_SCAVENGER_FORWARDED_HEADER_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new GCScavengerForwardedHeader_V1(object);
		}
	}
	
	/**
	 * Determine if the current object is forwarded.
	 * 
	 * @note This function can safely be called for a free list entry. It will
	 * return false in that case.
	 * 
	 * @return true if the current object is forwarded, false otherwise
	 * @throws CorruptDataException 
	 */
	public abstract boolean isForwardedPointer() throws CorruptDataException;
	
	/**
	 * Determine if the current object is a reverse forwarded object.
	 * 
	 * @note reverse forwarded objects are indistinguishable from holes. Only use this
	 * function if you can be sure the object is not a hole.
	 * 
	 * @return true if the current object is reverse forwarded, false otherwise
	 * @throws CorruptDataException 
	 */
	public abstract boolean isReverseForwardedPointer() throws CorruptDataException;
	
	/**
	 * If the object has been forwarded, return the forwarded version of the
	 * object, otherwise return NULL.
	 * 
	 * @note This function can safely be called for a free list entry. It will
	 * return NULL in that case.
	 * 
	 * @return the forwarded version of this object or NULL
	 * @throws CorruptDataException 
	 */
	public abstract J9ObjectPointer getForwardedObject() throws CorruptDataException;
	
	/**
	 * Get the reverse forwarded pointer for this object.
	 * 
	 * @return the reverse forwarded value
	 * @throws CorruptDataException 
	 */
	public abstract J9ObjectPointer getReverseForwardedPointer() throws CorruptDataException;
	
	/**
	 * @return the object pointer represented by the receiver
	 */
	public J9ObjectPointer getObject()
	{
		return objectPointer;
	}
	
	public abstract UDATA getObjectSize() throws CorruptDataException;
	
}
