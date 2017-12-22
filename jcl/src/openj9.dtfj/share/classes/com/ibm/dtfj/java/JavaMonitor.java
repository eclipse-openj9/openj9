/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;


/**
 * Represents a Java monitor (either an object monitor or a raw monitor).
 */
public interface JavaMonitor {
    
    /**
     * Get the object associated with this monitor.
     * @return the object associated with this monitor, or null if this is a raw monitor or a valid object could not be retrieved.
     */
    public JavaObject getObject();

    /**
     * Note that the name of a JavaMonitor is not necessarily meaningful but is provided here as it is 
     * usually present in the running VM.  If there is no name for the monitor a synthetic name will be
     * created by DTFJ.
     * 
     * @return the name of the monitor (never null)
     * @throws CorruptDataException 
     */
    public String getName() throws CorruptDataException;
    
    /**
     * Get the thread which currently owns the monitor
     * @return the owner of the monitor, or null if the monitor is unowned
     * @throws CorruptDataException 
     */
    public JavaThread getOwner() throws CorruptDataException;
    
    /**
     * Get the set of threads waiting to enter the monitor
     * @return an iterator over the collection of threads waiting to enter this monitor
     * 
     * @see JavaThread
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getEnterWaiters();
    
    /**
     * Get the set of threads waiting to be notified on the monitor (in the Object.wait method)
     * @return an iterator over the collection of threads waiting to be notified on
     * this monitor
     * 
     * @see JavaThread
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getNotifyWaiters();
    
    /**
     * Get the identifier for this monitor
     * @return The pointer which uniquely identifies this monitor in memory.
     */
    public ImagePointer getID();
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Monitor in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
