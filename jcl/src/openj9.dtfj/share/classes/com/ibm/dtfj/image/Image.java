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

import java.net.URI;
import java.util.Iterator;
import java.util.Properties;

/**
 * <p>Represents an entire operating system image (for example, a core file).</p> 
 * 
 * <p>There are methods for accessing information about the architecture 
 * of the machine on which the image was running - hardware and 
 * operating system. The major feature, however, is the ability to 
 * iterate over the Address Spaces contained within the image.</p> 
 */
public interface Image {
	
	/**
	 * Get the set of address spaces within the image - typically one but may be more on some 
	 * systems such as Z/OS. 
	 * @return an Iterator which iterates over all of the address spaces
	 * described by this Image
	 * 
	 * @see ImageAddressSpace
     * @see CorruptData
	 */
	public Iterator getAddressSpaces();

	/**
	 * Get the family name for the processor on which the image was
	 * running.
	 * @return the family name for the processor on which the image was
	 * running. This corresponds to the value you would find in the
	 * "os.arch" System property.
	 * 
	 * @throws DataUnavailable if this data cannot be inferred from this core type
	 * @throws CorruptDataException if expected data cannot be read from the core
	 */
	public String getProcessorType() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the precise model of the CPU.
	 * @return the precise model of the CPU (note that this can be an empty string but not null).
	 * <br>
	 * e.g. getProcessorType() will return "x86" where getProcessorSubType() may return "Pentium IV step 4"
	 * <p>
	 * Note that this value is platform and implementation dependent.
	 * @throws DataUnavailable 
	 * @throws CorruptDataException 
	 */
	public String getProcessorSubType() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the number of CPUs running in the system on which the image was running.
	 * @return the number of CPUs running in the system on which the
	 * image was running
	 * 
	 * @exception DataUnavailable if the information cannot be provided
	 */
	public int getProcessorCount() throws DataUnavailable;
	
	/**
	 * Get the family name for the operating system.
	 * @return the family name for the operating system.  This should be the same value
	 * that would be returned for the "os.name" system property
	 * 
	 * @throws DataUnavailable if this data cannot be inferred from this core type
	 * @throws CorruptDataException if expected data cannot be read from the core
	 */
	public String getSystemType() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the detailed name of the operating system.
	 * @return the detailed name of the operating system, or an empty string
	 * if this information is not available (null will never be returned).  This should be
	 * the same value that would be returned for the "os.version" system property
	 * @throws DataUnavailable 
	 * @throws CorruptDataException 
	 */
	public String getSystemSubType() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the amount of physical memory (in bytes) installed in the system on which
	 * the image was running.
	 * @return the amount of physical memory installed in the system on which
	 * the image was running.  The return value is specified in bytes.
	 * 
	 * @exception DataUnavailable if the information cannot be provided
	 */
	public long getInstalledMemory() throws DataUnavailable;
	
	/**
	 * Get the time when the image was created
	 * 
	 * @return the image creation time in milliseconds since 1970
	 * @throws DataUnavailable 
	 */
	public long getCreationTime() throws DataUnavailable;
	
	/**
	 * Get the host name of the system where the image was running. 
	 * @return The host name of the system where the image was running.  This string will
	 * be non-null and non-empty
	 * 
	 * @throws DataUnavailable If the image did not provide this information (would happen
	 * if the system did not know its host name or if the image predated this feature).
	 * @throws CorruptDataException 
	 * 
	 * @since 1.0
	 */
	public String getHostName() throws DataUnavailable, CorruptDataException;
	
	/**
	 * The set of IP addresses (as InetAddresses) which the system running the image possessed. 
	 * @return An Iterator over the IP addresses (as InetAddresses) which the system running 
	 * the image possessed.  The iterator will be non-null (but can be empty if the host is 
	 * known to have no IP addresses).
	 * 
	 * @throws DataUnavailable If the image did not provide this information (would happen
	 * if the system failed to look them up or if the image pre-dated this feature).
	 * 
	 * @since 1.0
	 * 
	 * @see java.net.InetAddress
	 * @see CorruptData
	 */
	public Iterator getIPAddresses() throws DataUnavailable;
	
	 /**
	  * <p>Close this image and any associated resources.</p>
	  * 
	  * <p>Some kinds of Image require the generation of temporary resources, for example temporary files created 
	  * when reading core files and libraries from .zip archives. Ordinarily, these resources are deleted at JVM shutdown,
	  * but DTFJ applications may want to free them earlier. This method should only be called when the Image is no
	  * longer needed. After this method has been called, any objects associated with the image will be in an invalid state.</p>
	  * 
	  * @since 1.4
	  */
	 public void close();
	 
	 /**
      * Gets the OS specific properties for this image.
      *  
      * @return a set of OS specific properties
      * @since 1.7
      */
    public Properties getProperties();
    
    /**
     * A unique identifier for the source of this image
     * @return URI for this image or null if this was not used when the image was created. 
     * @since 1.10
     */
    public URI getSource();
    
	/**
	 * Get the value of the JVM's high-resolution timer when the image was created.
	 *  
	 * @return the value of the high-resolution timer, in nanoseconds
	 * 
	 * @throws DataUnavailable if the image creation time is not available
	 * @throws CorruptDataException if the image creation time is corrupted
	 * 
	 * @since 1.12
	 */
	public long getCreationTimeNanos() throws DataUnavailable, CorruptDataException;
}
