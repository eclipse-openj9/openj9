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
package com.ibm.dtfj.image;

import java.util.Iterator;
import java.util.Properties;

/**
 * Represents a native operating system process.
 */
public interface ImageProcess {

    /**
     * Fetch the command line for this process. This may be the exact command line
     * the user issued, or it may be a reconstructed command line based on
     * argv[] and argc.
     * 
     * @return the command line for the process
     *
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     */
    public String getCommandLine() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the environment variables for this process.
     * @return the environment variables for this process
     * 
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     */
    public Properties getEnvironment() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the system-wide identifier for the process.
     * @return a system-wide identifier for the process (e.g. a process id (pid) on Unix like
     * systems)
     * 
   	 * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     */
    public String getID() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the set of shared libraries which are loaded in this process.
     * @return an iterator to iterate over the shared libraries which are loaded in this
     * process
     * 
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     * 
     * @see ImageModule
     * @see CorruptData
     */
    public Iterator getLibraries() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the module representing the executable within the image.
     * @return the module representing the executable within the image (as opposed to
     * modules representing libraries)
     * 
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     * 
     * @see ImageModule
     */
    public ImageModule getExecutable() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the set of image threads in the image.
     * @return an iterator to iterate over each ImageThread in the image
     * 
     * There is not necessarily any relationship between JavaThreads and
     * ImageThreads. A JVM implementation may use an n:m mapping of JavaThreads to
     * ImageThreads, and not all ImageThreads are necessarily attached.
     * 
     * @see ImageThread 
     * @see CorruptData
     */
    public Iterator getThreads();

    /**
     * Find the thread which triggered the creation of the image
     * 
     * @return the ImageThread which caused the image to be created, or
     * null if the image was not created due to a specific thread
     * @throws CorruptDataException 
     * 
     * @see ImageThread
     */
    public ImageThread getCurrentThread() throws CorruptDataException;
    
    /**
     * Get the set of the known ManagedRuntime environments in the image. 
     * @return an iterator to iterate over all of the known ManagedRuntime
     * environments in the image. 
     * 
     * In a typical image, there will be only one runtime, and it will be
     * an instance of JavaRuntime. However any user of this API should be aware
     * that there is a possibility that other runtimes may exist in the image
     * 
     * @see com.ibm.dtfj.runtime.ManagedRuntime 
     * @see com.ibm.dtfj.java.JavaRuntime
     * @see CorruptData
     */
    public Iterator getRuntimes();
    
    /**
     * Get the OS signal number in this process which triggered the creation 
     * of this image.
     * @return the OS signal number in this process which triggered the creation 
     * of this image, or 0 if the image was not created because of a signal in this
     * process
     * 
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     */
    public int getSignalNumber() throws DataUnavailable, CorruptDataException;
    
    /**
     * Get the name of the OS signal in this process which triggered the 
     * creation of this image.
     * @return the name of the OS signal in this process which triggered the 
     * creation of this image, or null if the image was not created because of a 
     * signal in this process
     * 
     * @exception DataUnavailable if the information cannot be provided
     * @throws CorruptDataException 
     */
    public String getSignalName() throws DataUnavailable, CorruptDataException;
    
    /**
     * Determine the pointer size used by this process.
     * Currently supported values are 31, 32 or 64.
     * In the future, other pointer sizes may also be supported.
     * 
     * @return the size of a pointer, in bits
     */
    public int getPointerSize();

	 /**
     * Gets the OS specific properties for this process.
     *  
     * @return a set of OS specific properties
     * @since 1.7
     */
    public Properties getProperties();
    
}
